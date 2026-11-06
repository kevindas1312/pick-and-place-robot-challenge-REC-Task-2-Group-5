#pragma once
#include <stddef.h>
// The host runner loads this shared object into Python, which supplies libc.
extern "C" int snprintf(char*, size_t, const char*, ...);
struct BluetoothSerial {
  bool connected = true;
  int packets = 0;
  char direction = 0, claw = 0;
  char frame[16] = {};
  void setPin(const char*, int = 0) {}
  void begin(const char*, bool) {}
  bool connect(const char*) { return connected; }
  bool hasClient() { return connected; }
  void printf(const char* format, char dir, char target) {
    ++packets; direction = dir; claw = target;
    snprintf(frame, sizeof(frame), format, dir, target);
  }
};
