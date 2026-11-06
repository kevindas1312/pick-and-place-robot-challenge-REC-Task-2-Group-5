#pragma once
#include <stddef.h>
#include "freertos/task.h"
extern "C" int snprintf(char*, size_t, const char*, ...);
using uint8_t = unsigned char;
using String = const char*;
const int LOW = 0, HIGH = 1, OUTPUT = 1, INPUT_PULLUP = 2;
unsigned long clockMs;
int digitalPins[64], analogPins[64], pwmPins[64];
unsigned long millis() { return clockMs; }
void delay(unsigned long ms) { clockMs += ms; }
void pinMode(int pin, int mode) { if (mode == INPUT_PULLUP) digitalPins[pin] = HIGH; }
int digitalRead(int pin) { return digitalPins[pin]; }
int analogRead(int pin) { return analogPins[pin]; }
void digitalWrite(int pin, int value) { digitalPins[pin] = value; }
void analogWrite(int pin, int value) { pwmPins[pin] = value; }
struct SerialStub {
  const char* input;
  int position;
  char lastDiagnostic[256] = {};
  int diagnosticLines = 0;
  void begin(int) {}
  template<typename T> void print(T) {}
  template<typename T> void println(T) {}
  template<typename... Args> void printf(const char* format, Args... args) {
    snprintf(lastDiagnostic, sizeof(lastDiagnostic), format, args...);
    ++diagnosticLines;
  }
  int available() { return input && input[position]; }
  char read() { return input[position++]; }
  void feed(const char* bytes) { input = bytes; position = 0; }
} Serial;
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
