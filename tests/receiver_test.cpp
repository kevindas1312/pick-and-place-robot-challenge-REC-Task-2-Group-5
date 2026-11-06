#include "ArduinoStub.h"
void serviceLink();
void applyCommand(char, char);
void moveForward(); void moveBackward(); void turnLeft(); void turnRight();
void spinSpot(); void spinSpotCounter();
void leftMotorForward(); void leftMotorBackward();
void rightMotorForward(); void rightMotorBackward(); void stopMotors();
#include "../arduino_nano_robot_RX_TX.ino"
extern "C" int run_tests() {
  setup();
  Serial.feed("FO\n"); loop();
  CHECK(digitalPins[IN1] == HIGH && digitalPins[IN2] == LOW);
  CHECK(digitalPins[IN3] == HIGH && digitalPins[IN4] == LOW);
  CHECK(pwmPins[ENA] == motorSpeed && pwmPins[ENB] == motorSpeed);
  CHECK(clawServo.angle == CLAW_OPEN_DEG);
  Serial.feed("BC\n"); loop();
  CHECK(digitalPins[IN1] == LOW && digitalPins[IN2] == HIGH);
  CHECK(clawServo.angle == CLAW_CLOSE_DEG);
  Serial.feed("WN\n"); loop();
  CHECK(digitalPins[IN1] == HIGH && digitalPins[IN2] == LOW);
  CHECK(pwmPins[ENB] == 0 && clawServo.angle == CLAW_CLOSE_DEG);
  // Movement and servo commands remain independent.
  const char directions[] = "FBLRTUWXYZS";
  for (int i = 0; directions[i]; ++i) {
    applyCommand(directions[i], 'O'); CHECK(clawServo.angle == CLAW_OPEN_DEG);
    applyCommand(directions[i], 'C'); CHECK(clawServo.angle == CLAW_CLOSE_DEG);
  }
  Serial.feed("S"); loop();
  CHECK(clawServo.angle == CLAW_CLOSE_DEG);
  Serial.feed("O\r\n"); loop();
  CHECK(clawServo.angle == CLAW_OPEN_DEG);
  Serial.feed("FN\n"); loop();
  clockMs += LINK_TIMEOUT_MS + 1; loop();
  CHECK(pwmPins[ENA] == 0 && pwmPins[ENB] == 0);
  return 0;
}
