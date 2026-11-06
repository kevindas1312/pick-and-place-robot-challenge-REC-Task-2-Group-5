#include "ArduinoStub.h"
#define CONFIG_BT_ENABLED 1
#define CONFIG_BLUEDROID_ENABLED 1
void attemptConnect();
#include "../controller_code_ESP32.ino"
extern "C" char* strstr(const char*, const char*);

bool taskReadyDuringConnect;
int clickPin;

// Simulate an entire click while connect() blocks the Arduino setup/loop.
// Only the background sampler runs; loop() is deliberately never called here.
void clickDuringConnect() {
  taskReadyDuringConnect = createdTask == clawButtonTask;
  digitalPins[clickPin] = LOW;
  sampleClawButtons();
  for (int i = 0; i < 6; ++i) {
    clockMs += 5;
    sampleClawButtons();
  }
  digitalPins[clickPin] = HIGH;
  for (int i = 0; i < 6; ++i) {
    clockMs += 5;
    sampleClawButtons();
  }
}

extern "C" int run_tests() {
  SerialBT.connected = false;
  SerialBT.onConnect = clickDuringConnect;
  clickPin = J1_SW;
  setup();
  CHECK(taskReadyDuringConnect);
  CHECK(readClawTarget() == 'O');
  CHECK(SerialBT.packets == 0);
  CHECK(strstr(Serial.lastDiagnostic, "left=PRESSED right=RELEASED -> OPEN (O)"));

  analogPins[J1_VRX] = analogPins[J2_VRX] = 2048;
  analogPins[J1_VRY] = analogPins[J2_VRY] = 2048;
  SerialBT.connected = true;
  clockMs = 100; loop();
  CHECK(SerialBT.direction == 'S' && SerialBT.claw == 'O');

  // The opposite button must also be retained and logged during a retry.
  SerialBT.connected = false;
  clickPin = J2_SW;
  clockMs = RECONNECT_INTERVAL_MS + 100;
  loop();
  CHECK(readClawTarget() == 'C');
  CHECK(SerialBT.packets == 1);
  CHECK(strstr(Serial.lastDiagnostic, "left=RELEASED right=PRESSED -> CLOSE (C)"));
  SerialBT.connected = true;
  loop();
  CHECK(SerialBT.direction == 'S' && SerialBT.claw == 'C');

  // With neither switch active, the sampler must not invent claw commands.
  int previousLogs = Serial.diagnosticLines;
  for (int i = 0; i < 100; ++i) {
    clockMs += 5;
    sampleClawButtons();
  }
  CHECK(readClawTarget() == 'C' && Serial.diagnosticLines == previousLogs);
  return 0;
}
