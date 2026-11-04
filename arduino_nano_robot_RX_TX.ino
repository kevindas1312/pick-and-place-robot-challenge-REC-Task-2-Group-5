#include <Servo.h>

// L298N Motor Driver Pins 
const int ENA = 6;  // PWM speed pin for Motor 1 (Left)
const int IN1 = 5;  // Motor 1 direction
const int IN2 = 4;  // Motor 1 direction
const int IN3 = 3;  // Motor 2 direction
const int IN4 = 2;  // Motor 2 direction
const int ENB = 7;  // PWM speed pin for Motor 2 (Right)

// Motor Speed (0 - 255)
const int motorSpeed = 200;

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
  clawServo.write(90); // Start at neutral position
}

void loop() {
  // Check if we have received at least a 2-character packet from Bluetooth
  if (Serial.available() >= 2) {
    char dir = Serial.read();
    char claw = Serial.read();

    // Clear out any leftover newline or carriage return characters
    while (Serial.available() > 0 && (Serial.peek() == '\n' || Serial.peek() == '\r')) {
      Serial.read();
    }

    // --- DEBUG PRINT ---
    Serial.print("Received Packet -> Dir: [");
    Serial.print(dir);
    Serial.print("] | Claw: [");
    Serial.print(claw);
    Serial.println("]");

    // Handle Movement Commands based on packet characters
    switch (dir) {
      case 'F': moveForward(); break;
      case 'B': moveBackward(); break;
      case 'L': turnLeft(); break;
      case 'R': turnRight(); break;
      case 'T': spinSpot(); break;
      case 'W': leftMotorForward(); break;
      case 'X': leftMotorBackward(); break;
      case 'Y': rightMotorForward(); break;
      case 'Z': rightMotorBackward(); break;
      default:  stopMotors(); break;
    }

    // Handle Claw Servo Control from buttons
    if (claw == 'O') {
      clawServo.write(160); // Open claw angle
    } else if (claw == 'C') {
      clawServo.write(20);  // Close claw angle
    }
  }
}

// --- Motor Control Functions ---
void moveForward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void moveBackward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void turnLeft() {
  analogWrite(ENA, 80);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turnRight() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, 80);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void spinSpot() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void leftMotorForward() {
  analogWrite(ENA, motorSpeed); analogWrite(ENB, 0);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void leftMotorBackward() {
  analogWrite(ENA, motorSpeed); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void rightMotorForward() {
  analogWrite(ENA, 0); analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void rightMotorBackward() {
  analogWrite(ENA, 0); analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
