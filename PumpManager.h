#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include "Config.h"

class PumpManager {
public:
  static void init();

  // Starts the specified pump (0-indexed: 0 to 7)
  // Ensures only one pump is running at a time
  static void startPump(uint8_t pumpIndex);

  // Stops the specified pump
  static void stopPump(uint8_t pumpIndex);

  // Stops all pumps
  static void stopAll();

  // Checks if any pump is currently running
  static bool isAnyPumpRunning();

  // Gets the index of the currently running pump (-1 if none)
  static int getRunningPump();

private:
  static int currentlyRunningPumpIndex;
};

#endif // PUMP_MANAGER_H
