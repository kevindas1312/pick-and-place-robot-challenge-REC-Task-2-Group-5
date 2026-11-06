#include "ArduinoStub.h"
#define CONFIG_BT_ENABLED 1
#define CONFIG_BLUEDROID_ENABLED 1
void attemptConnect(); // Arduino normally generates this prototype.
#include "../controller_code_ESP32.ino"
void tick(unsigned long time) { clockMs = time; sampleClawButtons(); loop(); }
extern "C" int run_tests() {
  setup();
  CHECK(createdTask == clawButtonTask);
  analogPins[J1_VRX] = analogPins[J2_VRX] = 2048;
  analogPins[J1_VRY] = analogPins[J2_VRY] = 2048;
  tick(100);
  CHECK(SerialBT.direction == 'S' && SerialBT.claw == 'N');
  // A 30 ms click entirely between two heartbeats must survive release.
  digitalPins[J1_SW] = LOW; tick(110); tick(130);
  CHECK(Serial.diagnosticLines == 2); // Startup mapping and accepted click.
  digitalPins[J1_SW] = HIGH; tick(140); tick(160);
  CHECK(SerialBT.packets == 1);
  tick(200); CHECK(SerialBT.claw == 'O');
  tick(300); CHECK(SerialBT.claw == 'O');
  digitalPins[J2_SW] = LOW; tick(310); tick(330);
  tick(400); CHECK(SerialBT.claw == 'C');
  // The other button can override a held switch.
  digitalPins[J1_SW] = LOW; tick(410); tick(430);
  tick(500); CHECK(SerialBT.claw == 'O');
  digitalPins[J1_SW] = HIGH; digitalPins[J2_SW] = HIGH;
  tick(510); tick(530);
  // Contact bounce shorter than debounce must not close the claw.
  digitalPins[J2_SW] = LOW; tick(540);
  digitalPins[J2_SW] = HIGH; tick(545); tick(570);
  CHECK(Serial.diagnosticLines == 4); // Bounce must not log a press.
  tick(600); CHECK(SerialBT.claw == 'O');
  analogPins[J1_VRX] = 4095; tick(700);
  CHECK(SerialBT.direction == 'W' && SerialBT.claw == 'O');
  analogPins[J2_VRX] = 4095; tick(800);
  CHECK(SerialBT.direction == 'F');
  analogPins[J1_VRX] = analogPins[J2_VRX] = 0; tick(900);
  CHECK(SerialBT.direction == 'B');
  // Remember a command across a disconnected period.
  SerialBT.connected = false;
  digitalPins[J2_SW] = LOW; tick(910); tick(930);
  SerialBT.connected = true; tick(1000);
  CHECK(SerialBT.claw == 'C');
  return 0;
}
