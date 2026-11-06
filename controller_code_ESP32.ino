#include "BluetoothSerial.h"
//Pick & Place Robot  - REC Task 2 - Group 5
#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif

#if !defined(CONFIG_BT_ENABLED)
#error "Bluetooth support is missing from this ESP32 build (CONFIG_BT_ENABLED undefined). Use a full ESP32 core for a board that has Classic Bluetooth."
#elif !defined(CONFIG_BLUEDROID_ENABLED)
#error "This sketch needs the Bluedroid stack (Classic BT / SPP). ESP32-C3, ESP32-S2 and ESP32-H2 only ship NimBLE and cannot run BluetoothSerial."
#endif

BluetoothSerial SerialBT;

// Name the HC-05 broadcasts (set with AT+NAME on the module). Keep both in sync
// if you ever rename it from the factory default.
const char* BT_SLAVE_NAME = "module4";

// Joystick 1 (Left) Pins
const int J1_VRX = 32;
const int J1_VRY = 33;
const int J1_SW  = 25;

// Joystick 2 (Right) Pins
const int J2_VRX = 34;
const int J2_VRY = 35;
const int J2_SW  = 26;

// 12-bit ADC decision thresholds (0 - 4095).
// Outside this window an axis counts as "pushed", inside it counts as neutral.
// Every axis is tested on its own, so resting a finger on the other stick can
// no longer cancel a single-motor command.
const int AXIS_LOW  = 1000;
const int AXIS_HIGH = 3000;

// Timing
const unsigned long SEND_INTERVAL_MS      = 100;  // packet + heartbeat period
const unsigned long RECONNECT_INTERVAL_MS = 2000; // BT retry period
unsigned long lastConnectAttempt = 0;
unsigned long lastSendMs = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 20;

// Sample switches on every loop, independently of the packet timer. Remember
// the requested position so a quick click is also sent in later heartbeats.
struct ClawButton {
  int pin;
  bool rawPressed;
  bool stablePressed;
  unsigned long changedMs;
};
ClawButton openButton  = {J1_SW, false, false, 0};
ClawButton closeButton = {J2_SW, false, false, 0};
char clawTarget = 'N';

// Keep the custom type visible to Arduino's sketch preprocessor.
bool buttonPressed(ClawButton &button, unsigned long now);

bool buttonPressed(ClawButton &button, unsigned long now) {
  bool pressed = digitalRead(button.pin) == LOW;
  if (pressed != button.rawPressed) {
    button.rawPressed = pressed;
    button.changedMs = now;
  }
  if (pressed != button.stablePressed &&
      now - button.changedMs >= BUTTON_DEBOUNCE_MS) {
    button.stablePressed = pressed;
    return pressed; // One event per debounced press; release does not act.
  }
  return false;
}

inline bool pushedLow(int v)  { return v <  AXIS_LOW;  }
inline bool pushedHigh(int v) { return v >  AXIS_HIGH; }
inline bool isNeutral(int v)  { return v >= AXIS_LOW && v <= AXIS_HIGH; }

void setup() {
  Serial.begin(115200);
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  SerialBT.setPin("1234", 4);
  #else
  SerialBT.setPin("1234");
  #endif
  

  // If your HC-05 asks for a PIN other than the library default ("1234"),
  // set it BEFORE begin():  SerialBT.setPin("1234");
  // Initialize Bluetooth as Master (true enables master mode to initiate pairing)
  SerialBT.begin("ESP32_Controller", true);
  
  // Configure switch pins with internal pull-ups
  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);

  Serial.println("ESP32 Controller Ready. Searching for HC-05...");

  // Attempt to connect to the HC-05. The name it broadcasts is set once with
  // AT+NAME on the module - keep BT_SLAVE_NAME in sync if you rename it.
  attemptConnect();
}

// Connect (or re-connect) to the HC-05. Master mode must already be running.
// Safe to call repeatedly: loop() retries every RECONNECT_INTERVAL_MS while the
// link is down, instead of giving up after a single attempt at boot.
void attemptConnect() {
  lastConnectAttempt = millis();

  if (SerialBT.connect(BT_SLAVE_NAME)) {
    Serial.println("Connected to HC-05 Successfully!");
  } else {
    Serial.println("Failed to connect. Make sure the HC-05 is powered on and in range - retrying.");
  }
}

void loop() {
  unsigned long now = millis();
  bool openPressed = buttonPressed(openButton, now);
  bool closePressed = buttonPressed(closeButton, now);
  // Newest press wins, even while the other switch remains held. If both
  // become pressed together, open takes priority.
  if (openPressed) clawTarget = 'O';
  else if (closePressed) clawTarget = 'C';

  // Keep the SPP link alive. Without this, a connect that fails at boot (HC-05
  // powered up later, out of range, module rebooted) leaves the controller
  // silently transmitting into the void and the robot dead on the floor.
  if (!SerialBT.hasClient()) {          // hasClient() == connected()
    if (millis() - lastConnectAttempt >= RECONNECT_INTERVAL_MS) {
      attemptConnect();
    }
    return;
  }

  if (now - lastSendMs < SEND_INTERVAL_MS) return;
  lastSendMs = now;

  // Read analog axes (0 to 4095 on ESP32 12-bit ADC)
  int j1_x = analogRead(J1_VRX);
  int j1_y = analogRead(J1_VRY);
  int j2_x = analogRead(J2_VRX);
  int j2_y = analogRead(J2_VRY);

  // Read digital button states (LOW when pressed, HIGH when released)
  bool j1_pressed = digitalRead(J1_SW) == LOW;
  bool j2_pressed = digitalRead(J2_SW) == LOW;

  char direction = 'S'; // Stop by default
  String dirText = "Stop";

  // 1. Check for Spin (opposing inputs on the X-axes). Either combination spins
  //    the chassis, in opposite directions - 'T' is the original rotation.
  if (pushedLow(j1_x) && pushedHigh(j2_x)) {
    direction = 'T';                 // left wheel back, right wheel forward
    dirText = "Spin (Left Reverse)";
  } else if (pushedHigh(j1_x) && pushedLow(j2_x)) {
    direction = 'U';                 // left wheel forward, right wheel back
    dirText = "Spin (Left Forward)";
  }
  // 2. Check for Both Forward / Backward (both joysticks pushed together)
  else if (pushedHigh(j1_x) && pushedHigh(j2_x)) {
    direction = 'F';
    dirText = "Forward (Both)";
  } else if (pushedLow(j1_x) && pushedLow(j2_x)) {
    direction = 'B';
    dirText = "Backward (Both)";
  }
  // 3. Check for Left Motor Only (Left X moved while Right X is neutral)
  else if (pushedHigh(j1_x) && isNeutral(j2_x)) {
    direction = 'W'; // Left motor Forward only
    dirText = "Left Motor Only (FWD)";
  } else if (pushedLow(j1_x) && isNeutral(j2_x)) {
    direction = 'X'; // Left motor Backward only
    dirText = "Left Motor Only (REV)";
  }
  // 4. Check for Right Motor Only (Right X moved while Left X is neutral)
  else if (pushedHigh(j2_x) && isNeutral(j1_x)) {
    direction = 'Y'; // Right motor Forward only
    dirText = "Right Motor Only (FWD)";
  } else if (pushedLow(j2_x) && isNeutral(j1_x)) {
    direction = 'Z'; // Right motor Backward only
    dirText = "Right Motor Only (REV)";
  }
  // 5. Check for standard Left / Right steering (on the Y-axes)
  else if (pushedLow(j1_y) || pushedLow(j2_y)) {
    direction = 'L';
    dirText = "Steer Left";
  } else if (pushedHigh(j1_y) || pushedHigh(j2_y)) {
    direction = 'R';
    dirText = "Steer Right";
  }

  // Repeat the last requested position so a missed packet cannot lose a click.
  char claw = clawTarget;

  // Print results matching your exact format + Direction Text + Packet Sent
  Serial.print("LEFT  [X: "); Serial.print(j1_x);
  Serial.print(", Y: "); Serial.print(j1_y);
  Serial.print(", Btn: "); Serial.print(j1_pressed ? "PRESSED" : "OPEN");
  
  Serial.print("] | RIGHT [X: "); Serial.print(j2_x);
  Serial.print(", Y: "); Serial.print(j2_y);
  Serial.print(", Btn: "); Serial.print(j2_pressed ? "PRESSED" : "OPEN");
  Serial.print("] | Dir: "); Serial.print(dirText);
  Serial.print(" --> Packet: "); 
  Serial.print(direction); Serial.println(claw);

  // Send one newline-terminated frame over Bluetooth: <Direction><Claw>\n
  // The '\n' is the delimiter the Nano parses by - never drop it, and keep the
  // payload exactly two bytes so a partial read can never wedge the parser.
  SerialBT.printf("%c%c\n", direction, claw);

}
