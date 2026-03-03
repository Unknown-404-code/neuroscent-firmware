#include "PumpManager.h"

int PumpManager::currentlyRunningPumpIndex = -1;

void PumpManager::init() {
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    pinMode(PUMP_PINS[i], OUTPUT);
    digitalWrite(PUMP_PINS[i], LOW); // Ensure OFF at startup
  }
  currentlyRunningPumpIndex = -1;
}

void PumpManager::startPump(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS)
    return;

  // Strict sequential execution: stop all before starting a new one
  if (currentlyRunningPumpIndex != -1 &&
      currentlyRunningPumpIndex != pumpIndex) {
    stopAll();
  }

  digitalWrite(PUMP_PINS[pumpIndex], HIGH);
  currentlyRunningPumpIndex = pumpIndex;
}

void PumpManager::stopPump(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS)
    return;

  digitalWrite(PUMP_PINS[pumpIndex], LOW);
  if (currentlyRunningPumpIndex == pumpIndex) {
    currentlyRunningPumpIndex = -1;
  }
}

void PumpManager::stopAll() {
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    digitalWrite(PUMP_PINS[i], LOW);
  }
  currentlyRunningPumpIndex = -1;
}

bool PumpManager::isAnyPumpRunning() { return currentlyRunningPumpIndex != -1; }

int PumpManager::getRunningPump() { return currentlyRunningPumpIndex; }
