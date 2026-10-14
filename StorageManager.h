#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "Config.h"
#include <Arduino.h>
#include <Preferences.h>

// Default values for new settings
constexpr float DEFAULT_CONTAINER_CAPACITY_ML_PER_PUMP = 100.0f;
constexpr uint8_t DEFAULT_LOW_LIQUID_WARN_PCT = 20;
constexpr uint16_t DEFAULT_MAX_PURGE_S = 60;
constexpr uint16_t DEFAULT_CALIB_TARGET_ML = 100;
constexpr uint16_t DEFAULT_CALIB_TIMEOUT_S = 300;

class StorageManager {
public:
  static void init();

  // Calibration (ml_per_ms)
  static float getCalibration(uint8_t pumpIndex);
  static void setCalibration(uint8_t pumpIndex, float mlPerMs);
  static void resetCalibration(uint8_t pumpIndex);

  // Containers (remaining_ml)
  static float getRemainingMl(uint8_t pumpIndex);
  static void setRemainingMl(uint8_t pumpIndex, float remaining);
  static void dispenseFromContainer(uint8_t pumpIndex, float amountDispensed);
  static void resetContainer(uint8_t pumpIndex);

  // Container capacity (per pump, ml)
  static float getCapacityMl(uint8_t pumpIndex);
  static void setCapacityMl(uint8_t pumpIndex, float capacity);

  // Accord names (per pump)
  static String getAccordName(uint8_t pumpIndex);
  static void setAccordName(uint8_t pumpIndex, const String& name);

  // PWM Configuration (duty cycle value)
  static uint8_t getPwmResolution();
  static void setPwmResolution(uint8_t pwm_res);

  // Web UI Settings
  static uint16_t getMaxPurgeDurationS();
  static void setMaxPurgeDurationS(uint16_t seconds);

  static uint16_t getCalibTargetMl();
  static void setCalibTargetMl(uint16_t ml);

  static uint16_t getCalibTimeoutS();
  static void setCalibTimeoutS(uint16_t seconds);

  static uint8_t getLowLiquidWarnPct();
  static void setLowLiquidWarnPct(uint8_t pct);

private:
  static Preferences prefsCalib;
  static Preferences prefsConfig;
  static Preferences prefsSettings;
  static Preferences prefsAccords;

  static float calibrations[NUM_PUMPS];
  static float remaining_ml[NUM_PUMPS];
  static float capacity_ml[NUM_PUMPS];
  static uint8_t pwm_resolution;
  static uint16_t maxPurgeDurationS;
  static uint16_t calibTargetMl;
  static uint16_t calibTimeoutS;
  static uint8_t lowLiquidWarnPct;

  static String getPumpKey(uint8_t pumpIndex);
  static String getCapacityKey(uint8_t pumpIndex);
  static String getNameKey(uint8_t pumpIndex);
};

#endif // STORAGE_MANAGER_H
