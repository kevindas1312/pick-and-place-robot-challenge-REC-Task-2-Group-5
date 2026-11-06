#pragma once
struct BluetoothSerial {
  bool connected = true;
  int packets = 0;
  char direction = 0, claw = 0;
  void setPin(const char*, int = 0) {}
  void begin(const char*, bool) {}
  bool connect(const char*) { return connected; }
  bool hasClient() { return connected; }
  void printf(const char*, char dir, char target) {
    ++packets; direction = dir; claw = target;
  }
};
