#include "ArduinoStub.h"
#include "BluetoothSerial.h"
#include "Servo.h"
#define CONFIG_BT_ENABLED 1
#define CONFIG_BLUEDROID_ENABLED 1

namespace controller {
void attemptConnect();
#include "../controller_code_ESP32.ino"
}

namespace receiver {
SerialStub Serial;
void serviceLink();
void applyCommand(char, char);
void moveForward(); void moveBackward(); void turnLeft(); void turnRight();
void spinSpot(); void spinSpotCounter();
void leftMotorForward(); void leftMotorBackward();
void rightMotorForward(); void rightMotorBackward(); void stopMotors();
#include "../arduino_nano_robot_RX_TX.ino"
}

// Deliver the controller's formatted bytes to the real receiver parser.
void tick(unsigned long time) {
  clockMs = time;
  controller::sampleClawButtons();
  int previousPackets = controller::SerialBT.packets;
  controller::loop();
  if (controller::SerialBT.packets != previousPackets) {
    receiver::Serial.feed(controller::SerialBT.frame);
  }
  receiver::loop();
}

extern "C" int run_tests() {
  receiver::setup();
  controller::setup();
  unsigned long start = clockMs;
  analogPins[controller::J1_VRX] = analogPins[controller::J2_VRX] = 2048;
  analogPins[controller::J1_VRY] = analogPins[controller::J2_VRY] = 2048;
  tick(start);
  CHECK(receiver::clawServo.angle == receiver::CLAW_NEUTRAL_DEG);

  // Left click opens, right click closes while left is still held.
  digitalPins[controller::J1_SW] = LOW;
  tick(start + 10); tick(start + 30); tick(start + 100);
  CHECK(receiver::clawServo.angle == receiver::CLAW_OPEN_DEG);
  digitalPins[controller::J2_SW] = LOW;
  tick(start + 110); tick(start + 130); tick(start + 200);
  CHECK(receiver::clawServo.angle == receiver::CLAW_CLOSE_DEG);

  // Release both; a later simultaneous click follows the existing open priority.
  digitalPins[controller::J1_SW] = digitalPins[controller::J2_SW] = HIGH;
  tick(start + 210); tick(start + 230);
  digitalPins[controller::J1_SW] = digitalPins[controller::J2_SW] = LOW;
  tick(start + 240); tick(start + 260); tick(start + 300);
  CHECK(receiver::clawServo.angle == receiver::CLAW_OPEN_DEG);

  // Right stick forward/reverse reaches the corrected output pins. Left stays off.
  analogPins[controller::J2_VRX] = 4095; tick(start + 400);
  CHECK(controller::SerialBT.direction == 'Y');
  CHECK(pwmPins[receiver::ENA] == 0 && pwmPins[receiver::ENB] == receiver::motorSpeed);
  CHECK(digitalPins[receiver::IN3] == LOW && digitalPins[receiver::IN4] == HIGH);
  analogPins[controller::J2_VRX] = 0; tick(start + 500);
  CHECK(controller::SerialBT.direction == 'Z');
  CHECK(digitalPins[receiver::IN3] == HIGH && digitalPins[receiver::IN4] == LOW);
  CHECK(receiver::clawServo.angle == receiver::CLAW_OPEN_DEG);
  return 0;
}
