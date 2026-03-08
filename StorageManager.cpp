#include "StorageManager.h"

Preferences StorageManager::prefsCalib;
Preferences StorageManager::prefsConfig;

float StorageManager::calibrations[NUM_PUMPS];
float StorageManager::remaining_ml[NUM_PUMPS];
uint8_t StorageManager::pwm_resolution = DEFAULT_PWM_RESOLUTION;

String StorageManager::getPumpKey(uint8_t pumpIndex) {
  return String("P") + String(pumpIndex);
}

void StorageManager::init() {
  prefsCalib.begin("calibration", false);
  prefsConfig.begin("containers", false);

  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    String key = getPumpKey(i);

    // Load calibrations, set default if not exists
    if (!prefsCalib.isKey(key.c_str())) {
      prefsCalib.putFloat(key.c_str(), DEFAULT_ML_PER_MS);
    }
    calibrations[i] = prefsCalib.getFloat(key.c_str(), DEFAULT_ML_PER_MS);

    // Load container levels, set default if not exists
    if (!prefsConfig.isKey(key.c_str())) {
      prefsConfig.putFloat(key.c_str(), DEFAULT_CONTAINER_CAPACITY_ML);
    }
    remaining_ml[i] =
        prefsConfig.getFloat(key.c_str(), DEFAULT_CONTAINER_CAPACITY_ML);
  }

  // Load PWM settings
  if (!prefsConfig.isKey("pwm_res")) {
    prefsConfig.putUChar("pwm_res", DEFAULT_PWM_RESOLUTION);
  }
  pwm_resolution = prefsConfig.getUChar("pwm_res", DEFAULT_PWM_RESOLUTION);
}

// ==========================================
// CALIBRATION
// ==========================================

float StorageManager::getCalibration(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS)
    return DEFAULT_ML_PER_MS;
  return calibrations[pumpIndex];
}

void StorageManager::setCalibration(uint8_t pumpIndex, float mlPerMs) {
  if (pumpIndex >= NUM_PUMPS)
    return;
  calibrations[pumpIndex] = mlPerMs;
  prefsCalib.putFloat(getPumpKey(pumpIndex).c_str(), mlPerMs);
}

void StorageManager::resetCalibration(uint8_t pumpIndex) {
  setCalibration(pumpIndex, DEFAULT_ML_PER_MS);
}

// ==========================================
// CONTAINERS
// ==========================================

float StorageManager::getRemainingMl(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS)
    return 0.0f;
  return remaining_ml[pumpIndex];
}

void StorageManager::setRemainingMl(uint8_t pumpIndex, float remaining) {
  if (pumpIndex >= NUM_PUMPS)
    return;
  // ensure doesn't go below 0
  if (remaining < 0.0f)
    remaining = 0.0f;

  remaining_ml[pumpIndex] = remaining;
  prefsConfig.putFloat(getPumpKey(pumpIndex).c_str(), remaining);
}

void StorageManager::dispenseFromContainer(uint8_t pumpIndex,
                                           float amountDispensed) {
  if (pumpIndex >= NUM_PUMPS)
    return;
  float current = getRemainingMl(pumpIndex);
  setRemainingMl(pumpIndex, current - amountDispensed);
}

void StorageManager::resetContainer(uint8_t pumpIndex) {
  setRemainingMl(pumpIndex, DEFAULT_CONTAINER_CAPACITY_ML);
}

// ==========================================
// PWM SETTINGS
// ==========================================

uint8_t StorageManager::getPwmResolution() { return pwm_resolution; }

void StorageManager::setPwmResolution(uint8_t pwm_res) {
  pwm_resolution = pwm_res;
  prefsConfig.putUChar("pwm_res", pwm_res);
}
