#include <Servo.h>

// L298N Motor Driver Pins
const int ENA = 6;  // PWM speed pin for Motor 1 (Left)  - Timer0
const int IN1 = 5;  // Motor 1 direction
const int IN2 = 4;  // Motor 1 direction
const int IN3 = 3;  // Motor 2 direction
const int IN4 = 2;  // Motor 2 direction
// PWM speed pin for Motor 2 (Right). This MUST be a PWM-capable pin: on the
// ATmega328P only D3, D5, D6, D9, D10 and D11 are, and analogWrite() on any
// other pin just switches it fully on/off. It used to be D7, which is not PWM,
// so the right wheel ignored motorSpeed and "slow turn" became "wheel stopped".
// D9/D10 are left free because the Servo library owns Timer1.
const int ENB = 11; // PWM speed pin for Motor 2 (Right) - Timer2A

// ---- motor direction correction ---------------------------------------
// If a wheel spins the wrong way and its leads are already soldered, flip the
// matching flag here instead of resoldering. It only swaps which pin goes HIGH,
// so speed, braking and the fail-safe stop are all unaffected.
//
// How to tell which one: drive with BOTH sticks forward. The robot should roll
// straight ahead. Any wheel travelling backwards is the reversed one.
const bool LEFT_MOTOR_REVERSED  = true;   // left wheel drives backwards -> true
const bool RIGHT_MOTOR_REVERSED = false;  // right wheel drives backwards -> true

// The pin levels that make each channel go FORWARD, derived from the flags
// above. Everything else is written in terms of these, so a reversal is
// corrected in exactly one place.
const int L_FWD_A = LEFT_MOTOR_REVERSED  ? LOW  : HIGH;   // IN1 when going forward
const int L_FWD_B = LEFT_MOTOR_REVERSED  ? HIGH : LOW;    // IN2 when going forward
const int R_FWD_A = RIGHT_MOTOR_REVERSED ? LOW  : HIGH;   // IN3 when going forward
const int R_FWD_B = RIGHT_MOTOR_REVERSED ? HIGH : LOW;    // IN4 when going forward

// Motor Speed (0 - 255). Lower = slower and easier to control.
// 150 is about 59 % duty. If the robot will not move at all, raise it; if it
// still feels fast, try 120.
const int motorSpeed = 100;

// Turning: the inner wheel runs at this duty so the robot curves instead of
// skidding in a circle. Keep it roughly 55-65 % of motorSpeed, or the inner
// wheel may stall against friction and the "curve" becomes a pivot.
const int turnSpeed = 65;

// Claw servo angles
const int CLAW_NEUTRAL_DEG = 90;
const int CLAW_OPEN_DEG    = 160;
const int CLAW_CLOSE_DEG   = 20;

// Link supervision: the ESP32 sends a packet every 100 ms, so this only has to
// cover a few missed packets. Keep it well above the ESP32's send interval.
const unsigned long LINK_TIMEOUT_MS = 500;
unsigned long lastPacketMs = 0;

// Set to 1 to echo every received packet on the serial monitor.
// Keep it 0 while driving: each print blocks ~50 ms at 9600 baud and starves
// the Bluetooth receive buffer.
#define DEBUG_SERIAL 0

// Incoming frame buffer: <Direction><Claw> terminated by '\n'
char    frameBuf[8];
uint8_t frameLen = 0;

Servo clawServo;

void setup() {
  // HC-05 is connected to hardware RX/TX (Pins 0 and 1)
  Serial.begin(9600);

  delay(1000); 
  Serial.println("========================================");
  Serial.println(" Nano Receiver Initialized & Ready! ");
  Serial.println(" Listening on Hardware Pins 0 & 1");
  Serial.println("========================================");

  // Set motor control pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Attach Servo to D9
  clawServo.attach(9);
  clawServo.write(CLAW_NEUTRAL_DEG); // Start at neutral position

  // Arm the link watchdog from boot: the motors stay stopped until the first
  // valid frame arrives.
  lastPacketMs = millis();
}

void loop() {
  serviceLink();

  // Failsafe: the ESP32 sends a packet every 100 ms, so silence means the link
  // is down (out of range, controller off, flat battery). Never keep driving on
  // a stale command.
  if (millis() - lastPacketMs > LINK_TIMEOUT_MS) {
    stopMotors();
  }
}

// Assemble newline-terminated frames: <Direction><Claw>\n
// Non-blocking, so a half-received frame can never stall the main loop.
void serviceLink() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (frameLen >= 2) {
        applyCommand(frameBuf[0], frameBuf[1]);
        lastPacketMs = millis();
      }
      frameLen = 0;                             // end of frame (or empty frame)
    } else if (frameLen < sizeof(frameBuf) - 1) {
      frameBuf[frameLen++] = c;
    } else {
      frameLen = 0;                             // overflow -> resync
    }
  }
}

// Act on one validated command frame
void applyCommand(char dir, char claw) {
#if DEBUG_SERIAL
  Serial.print("Received Packet -> Dir: [");
  Serial.print(dir);
  Serial.print("] | Claw: [");
  Serial.print(claw);
  Serial.println("]");
#endif

  // Handle Movement Commands based on packet characters
  switch (dir) {
    case 'F': moveForward();        break;
    case 'B': moveBackward();       break;
    case 'L': turnLeft();           break;
    case 'R': turnRight();          break;
    case 'T': spinSpot();           break;  // left wheel back, right wheel fwd
    case 'U': spinSpotCounter();    break;  // left wheel fwd, right wheel back
    case 'W': leftMotorForward();   break;
    case 'X': leftMotorBackward();  break;
    case 'Y': rightMotorForward();  break;
    case 'Z': rightMotorBackward(); break;
    case 'S': stopMotors();         break;  // explicit stop
    default:  stopMotors();         break;  // unknown byte -> fail safe
  }

  // Handle Claw Servo Control from buttons ('N' or anything else = hold)
  if (claw == 'O') {
    clawServo.write(CLAW_OPEN_DEG);
  } else if (claw == 'C') {
    clawServo.write(CLAW_CLOSE_DEG);
  }
}

// --- Motor Control Functions ---
// Direction is always expressed as +1 (forward), -1 (backward) or 0 (coast) and
// translated to pin levels by these two helpers, so the LEFT_MOTOR_REVERSED /
// RIGHT_MOTOR_REVERSED flags are applied in exactly one place.
void setLeftWheel(int dir) {
  if (dir > 0)       { digitalWrite(IN1, L_FWD_A); digitalWrite(IN2, L_FWD_B); }
  else if (dir < 0)  { digitalWrite(IN1, L_FWD_B); digitalWrite(IN2, L_FWD_A); }
  else               { digitalWrite(IN1, LOW);     digitalWrite(IN2, LOW);     }
}

void setRightWheel(int dir) {
  if (dir > 0)       { digitalWrite(IN3, R_FWD_A); digitalWrite(IN4, R_FWD_B); }
  else if (dir < 0)  { digitalWrite(IN3, R_FWD_B); digitalWrite(IN4, R_FWD_A); }
  else               { digitalWrite(IN3, LOW);     digitalWrite(IN4, LOW);     }
}

void moveForward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  setLeftWheel(+1); setRightWheel(+1);
}

void moveBackward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  setLeftWheel(-1); setRightWheel(-1);
}

void turnLeft() {
  analogWrite(ENA, turnSpeed);   // left wheel slower -> curves left
  analogWrite(ENB, motorSpeed);
  setLeftWheel(+1); setRightWheel(+1);
}

void turnRight() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, turnSpeed);   // right wheel slower -> curves right
  setLeftWheel(+1); setRightWheel(+1);
}

void spinSpot() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  setLeftWheel(-1); setRightWheel(+1);   // left back, right forward
}

// 'U': the opposite in-place rotation (left forwards, right backwards)
void spinSpotCounter() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  setLeftWheel(+1); setRightWheel(-1);
}

void leftMotorForward() {
  analogWrite(ENA, motorSpeed); analogWrite(ENB, 0);
  setLeftWheel(+1); setRightWheel(0);   // right wheel coasts
}

void leftMotorBackward() {
  analogWrite(ENA, motorSpeed); analogWrite(ENB, 0);
  setLeftWheel(-1); setRightWheel(0);
}

void rightMotorForward() {
  analogWrite(ENA, 0); analogWrite(ENB, motorSpeed);
  setLeftWheel(0); setRightWheel(+1);   // left wheel coasts
}

void rightMotorBackward() {
  analogWrite(ENA, 0); analogWrite(ENB, motorSpeed);
  setLeftWheel(0); setRightWheel(-1);
}

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  setLeftWheel(0); setRightWheel(0);
}
