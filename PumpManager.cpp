#include "PumpManager.h"
#include "StorageManager.h" // For reading current PWM resolution

int PumpManager::currentlyRunningPumpIndex = -1;

void PumpManager::init() {
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    pinMode(PUMP_PINS[i], OUTPUT);
    digitalWrite(PUMP_PINS[i], LOW); // Ensure OFF at startup

    // Setup hardware PWM (LEDC) on the pump pin (ESP32 Core 3.0+ API)
    ledcAttach(PUMP_PINS[i], PWM_FREQ_HZ, PWM_RES_BITS);
    ledcWrite(PUMP_PINS[i], 0); // Explicitly ensure OFF via PWM
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

  // Read current commanded duty cycle from NVS cache
  uint8_t duty = StorageManager::getPwmResolution();
  ledcWrite(PUMP_PINS[pumpIndex], duty);
  currentlyRunningPumpIndex = pumpIndex;
}

void PumpManager::stopPump(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS)
    return;

  ledcWrite(PUMP_PINS[pumpIndex], 0);
  if (currentlyRunningPumpIndex == pumpIndex) {
    currentlyRunningPumpIndex = -1;
  }
}

void PumpManager::stopAll() {
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    ledcWrite(PUMP_PINS[i], 0);
  }
  currentlyRunningPumpIndex = -1;
}

bool PumpManager::isAnyPumpRunning() { return currentlyRunningPumpIndex != -1; }

int PumpManager::getRunningPump() { return currentlyRunningPumpIndex; }
