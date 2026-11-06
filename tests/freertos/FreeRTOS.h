#pragma once
using TickType_t = unsigned long;
const int pdPASS = 1;
inline TickType_t pdMS_TO_TICKS(unsigned long ms) { return ms; }
struct portMUX_TYPE {};
#define portMUX_INITIALIZER_UNLOCKED {}
inline void portENTER_CRITICAL(portMUX_TYPE*) {}
inline void portEXIT_CRITICAL(portMUX_TYPE*) {}
