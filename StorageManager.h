#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "Config.h"
#include <Arduino.h>
#include <Preferences.h>

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

private:
  static Preferences prefsCalib;
  static Preferences prefsConfig;

  static float calibrations[NUM_PUMPS];
  static float remaining_ml[NUM_PUMPS];

  static String getPumpKey(uint8_t pumpIndex);
};

#endif // STORAGE_MANAGER_H
