#pragma once
struct Servo {
  int angle;
  void attach(int) {}
  void write(int value) { angle = value; }
};
