#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Run `make menuconfig` to and set it up
#endif

BluetoothSerial SerialBT;

// Joystick 1 (Left) Pins
const int J1_VRX = 32;
const int J1_VRY = 33;
const int J1_SW  = 25;

// Joystick 2 (Right) Pins
const int J2_VRX = 34;
const int J2_VRY = 35;
const int J2_SW  = 26;

void setup() {
  Serial.begin(115200);
  
  // Initialize Bluetooth as Master (true enables master mode to initiate pairing)
  SerialBT.begin("ESP32_Controller", true); 
  
  // Configure switch pins with internal pull-ups
  pinMode(J1_SW, INPUT_PULLUP);
  pinMode(J2_SW, INPUT_PULLUP);

  Serial.println("ESP32 Controller Ready. Searching for HC-05...");

  // Attempt to connect to the HC-05 (custom name : "Group 5")
  bool connected = SerialBT.connect("HC-05");
  
  if (connected) {
    Serial.println("Connected to HC-05 Successfully!");
  } else {
    Serial.println("Failed to connect. Make sure the HC-05 is powered on and in range.");
  }
}

void loop() {
  // Read analog axes (0 to 4095 on ESP32 12-bit ADC)
  int j1_x = analogRead(J1_VRX);
  int j1_y = analogRead(J1_VRY);
  int j2_x = analogRead(J2_VRX);
  int j2_y = analogRead(J2_VRY);

  // Read digital button states (LOW when pressed, HIGH when released)
  bool j1_pressed = digitalRead(J1_SW) == LOW;
  bool j2_pressed = digitalRead(J2_SW) == LOW;

  // Helper variables to check if a joystick is centered (neutral zone: 1000 to 3000)
  bool j1_centered = (j1_x >= 1000 && j1_x <= 3000) && (j1_y >= 1000 && j1_y <= 3000);
  bool j2_centered = (j2_x >= 1000 && j2_x <= 3000) && (j2_y >= 1000 && j2_y <= 3000);

  char direction = 'S'; // Stop by default
  String dirText = "Stop";

  // 1. Check for Spin (Opposing inputs on the X-axes)
  if ((j1_x < 1000 && j2_x > 3000) || (j1_x > 3000 && j2_x < 1000)) {
    direction = 'T';
    dirText = "Spin / Turn";
  } 
  // 2. Check for Both Forward / Backward (both joysticks pushed together)
  else if (j1_x > 3000 && j2_x > 3000) {
    direction = 'F';
    dirText = "Forward (Both)";
  } else if (j1_x < 1000 && j2_x < 1000) {
    direction = 'B';
    dirText = "Backward (Both)";
  } 
  // 3. Check for Left Motor Only (Left joystick moved while Right is centered)
  else if (j1_x > 3000 && j2_centered) {
    direction = 'W'; // Left motor Forward only
    dirText = "Left Motor Only (FWD)";
  } else if (j1_x < 1000 && j2_centered) {
    direction = 'X'; // Left motor Backward only
    dirText = "Left Motor Only (REV)";
  } 
  // 4. Check for Right Motor Only (Right joystick moved while Left is centered)
  else if (j2_x > 3000 && j1_centered) {
    direction = 'Y'; // Right motor Forward only
    dirText = "Right Motor Only (FWD)";
  } else if (j2_x < 1000 && j1_centered) {
    direction = 'Z'; // Right motor Backward only
    dirText = "Right Motor Only (REV)";
  }
  // 5. Check for standard Left / Right steering (on the Y-axes)
  else if (j1_y < 1000 || j2_y < 1000) {
    direction = 'L';
    dirText = "Steer Left";
  } else if (j1_y > 3000 || j2_y > 3000) {
    direction = 'R';
    dirText = "Steer Right";
  }

  // Determine Claw Action from buttons
  char claw = 'N'; // No change
  if (j1_pressed) { 
    claw = 'O';    // Open claw (Joystick 1 button)
  } else if (j2_pressed) {
    claw = 'C';    // Close claw (Joystick 2 button)
  }

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

  // Send 2-character packet format over Bluetooth: <Direction><Claw>
  SerialBT.print(direction);
  SerialBT.println(claw);

  delay(100); // Send update every 100ms
}
