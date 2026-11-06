#pragma once
#include "FreeRTOS.h"
using TaskFunction_t = void (*)(void*);
TaskFunction_t createdTask = nullptr;
// Tests drive sampleClawButtons explicitly to simulate background task ticks.
inline int xTaskCreate(TaskFunction_t task, const char*, unsigned int, void*,
                       unsigned int, void*) {
  createdTask = task;
  return pdPASS;
}
inline void vTaskDelay(TickType_t) {}
