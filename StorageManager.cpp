#include "StorageManager.h"

Preferences StorageManager::prefsCalib;
Preferences StorageManager::prefsConfig;
Preferences StorageManager::prefsSettings;
Preferences StorageManager::prefsAccords;

float    StorageManager::calibrations[NUM_PUMPS];
float    StorageManager::remaining_ml[NUM_PUMPS];
float    StorageManager::capacity_ml[NUM_PUMPS];
uint8_t  StorageManager::pwm_resolution = DEFAULT_PWM_RESOLUTION;
uint16_t StorageManager::maxPurgeDurationS = DEFAULT_MAX_PURGE_S;
uint16_t StorageManager::calibTargetMl    = DEFAULT_CALIB_TARGET_ML;
uint16_t StorageManager::calibTimeoutS    = DEFAULT_CALIB_TIMEOUT_S;
uint8_t  StorageManager::lowLiquidWarnPct = DEFAULT_LOW_LIQUID_WARN_PCT;

String StorageManager::getPumpKey(uint8_t pumpIndex) {
  return String("P") + String(pumpIndex);
}

String StorageManager::getCapacityKey(uint8_t pumpIndex) {
  return String("C") + String(pumpIndex);
}

String StorageManager::getNameKey(uint8_t pumpIndex) {
  return String("N") + String(pumpIndex);
}

void StorageManager::init() {
  prefsCalib.begin("calibration", false);
  prefsConfig.begin("containers", false);
  prefsSettings.begin("settings", false);
  prefsAccords.begin("accords", false);

  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    String key = getPumpKey(i);
    String capKey = getCapacityKey(i);

    // Load calibrations, set default if not exists
    if (!prefsCalib.isKey(key.c_str())) {
      prefsCalib.putFloat(key.c_str(), DEFAULT_ML_PER_MS);
    }
    calibrations[i] = prefsCalib.getFloat(key.c_str(), DEFAULT_ML_PER_MS);

    // Load container levels, set default if not exists
    if (!prefsConfig.isKey(key.c_str())) {
      prefsConfig.putFloat(key.c_str(), DEFAULT_CONTAINER_CAPACITY_ML);
    }
    remaining_ml[i] = prefsConfig.getFloat(key.c_str(), DEFAULT_CONTAINER_CAPACITY_ML);

    // Load per-pump capacity
    if (!prefsConfig.isKey(capKey.c_str())) {
      prefsConfig.putFloat(capKey.c_str(), DEFAULT_CONTAINER_CAPACITY_ML_PER_PUMP);
    }
    capacity_ml[i] = prefsConfig.getFloat(capKey.c_str(), DEFAULT_CONTAINER_CAPACITY_ML_PER_PUMP);
  }

  // Load PWM settings
  if (!prefsConfig.isKey("pwm_res")) {
    prefsConfig.putUChar("pwm_res", DEFAULT_PWM_RESOLUTION);
  }
  pwm_resolution = prefsConfig.getUChar("pwm_res", DEFAULT_PWM_RESOLUTION);

  // Load web UI settings
  if (!prefsSettings.isKey("max_purge_s")) {
    prefsSettings.putUShort("max_purge_s", DEFAULT_MAX_PURGE_S);
  }
  maxPurgeDurationS = prefsSettings.getUShort("max_purge_s", DEFAULT_MAX_PURGE_S);

  if (!prefsSettings.isKey("calib_ml")) {
    prefsSettings.putUShort("calib_ml", DEFAULT_CALIB_TARGET_ML);
  }
  calibTargetMl = prefsSettings.getUShort("calib_ml", DEFAULT_CALIB_TARGET_ML);

  if (!prefsSettings.isKey("calib_to_s")) {
    prefsSettings.putUShort("calib_to_s", DEFAULT_CALIB_TIMEOUT_S);
  }
  calibTimeoutS = prefsSettings.getUShort("calib_to_s", DEFAULT_CALIB_TIMEOUT_S);

  if (!prefsSettings.isKey("low_warn_pct")) {
    prefsSettings.putUChar("low_warn_pct", DEFAULT_LOW_LIQUID_WARN_PCT);
  }
  lowLiquidWarnPct = prefsSettings.getUChar("low_warn_pct", DEFAULT_LOW_LIQUID_WARN_PCT);
}

// ==========================================
// CALIBRATION
// ==========================================

float StorageManager::getCalibration(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS) return DEFAULT_ML_PER_MS;
  return calibrations[pumpIndex];
}

void StorageManager::setCalibration(uint8_t pumpIndex, float mlPerMs) {
  if (pumpIndex >= NUM_PUMPS) return;
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
  if (pumpIndex >= NUM_PUMPS) return 0.0f;
  return remaining_ml[pumpIndex];
}

void StorageManager::setRemainingMl(uint8_t pumpIndex, float remaining) {
  if (pumpIndex >= NUM_PUMPS) return;
  if (remaining < 0.0f) remaining = 0.0f;
  remaining_ml[pumpIndex] = remaining;
  prefsConfig.putFloat(getPumpKey(pumpIndex).c_str(), remaining);
}

void StorageManager::dispenseFromContainer(uint8_t pumpIndex, float amountDispensed) {
  if (pumpIndex >= NUM_PUMPS) return;
  float current = getRemainingMl(pumpIndex);
  setRemainingMl(pumpIndex, current - amountDispensed);
}

void StorageManager::resetContainer(uint8_t pumpIndex) {
  // Reset to stored capacity (not hardcoded 100)
  setRemainingMl(pumpIndex, capacity_ml[pumpIndex]);
}

// ==========================================
// CONTAINER CAPACITY
// ==========================================

float StorageManager::getCapacityMl(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS) return DEFAULT_CONTAINER_CAPACITY_ML_PER_PUMP;
  return capacity_ml[pumpIndex];
}

void StorageManager::setCapacityMl(uint8_t pumpIndex, float capacity) {
  if (pumpIndex >= NUM_PUMPS) return;
  if (capacity < 1.0f) capacity = 1.0f;   // Minimum 1 mL
  if (capacity > 9999.0f) capacity = 9999.0f;
  capacity_ml[pumpIndex] = capacity;
  prefsConfig.putFloat(getCapacityKey(pumpIndex).c_str(), capacity);
}

// ==========================================
// ACCORD NAMES
// ==========================================

String StorageManager::getAccordName(uint8_t pumpIndex) {
  if (pumpIndex >= NUM_PUMPS) return "";
  return prefsAccords.getString(getNameKey(pumpIndex).c_str(), "");
}

void StorageManager::setAccordName(uint8_t pumpIndex, const String& name) {
  if (pumpIndex >= NUM_PUMPS) return;
  // Limit name length
  String n = name.substring(0, 32);
  prefsAccords.putString(getNameKey(pumpIndex).c_str(), n);
}

// ==========================================
// PWM SETTINGS
// ==========================================

uint8_t StorageManager::getPwmResolution() { return pwm_resolution; }

void StorageManager::setPwmResolution(uint8_t pwm_res) {
  pwm_resolution = pwm_res;
  prefsConfig.putUChar("pwm_res", pwm_res);
}

// ==========================================
// WEB UI SETTINGS
// ==========================================

uint16_t StorageManager::getMaxPurgeDurationS() { return maxPurgeDurationS; }

void StorageManager::setMaxPurgeDurationS(uint16_t seconds) {
  if (seconds < 5) seconds = 5;
  if (seconds > 300) seconds = 300;
  maxPurgeDurationS = seconds;
  prefsSettings.putUShort("max_purge_s", seconds);
}

uint16_t StorageManager::getCalibTargetMl() { return calibTargetMl; }

void StorageManager::setCalibTargetMl(uint16_t ml) {
  if (ml < 5) ml = 5;
  if (ml > 500) ml = 500;
  calibTargetMl = ml;
  prefsSettings.putUShort("calib_ml", ml);
}

uint16_t StorageManager::getCalibTimeoutS() { return calibTimeoutS; }

void StorageManager::setCalibTimeoutS(uint16_t seconds) {
  if (seconds < 30) seconds = 30;
  if (seconds > 600) seconds = 600;
  calibTimeoutS = seconds;
  prefsSettings.putUShort("calib_to_s", seconds);
}

uint8_t StorageManager::getLowLiquidWarnPct() { return lowLiquidWarnPct; }

void StorageManager::setLowLiquidWarnPct(uint8_t pct) {
  if (pct > 50) pct = 50;
  lowLiquidWarnPct = pct;
  prefsSettings.putUChar("low_warn_pct", pct);
}
