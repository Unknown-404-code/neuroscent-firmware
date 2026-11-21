#ifndef OPERATION_STATE_H
#define OPERATION_STATE_H

#include "Config.h"

// --------------------------------------------------------
// OperationState — Centralized operation mutex
//
// Prevents OLED and Web UI from running conflicting
// hardware operations simultaneously.
//
// Rules:
//   - Only one operation can be ACTIVE at a time.
//   - Web operations check this before starting.
//   - OLED operations also check this before starting.
//   - Formula dispensing is tracked here too.
// --------------------------------------------------------
class OperationState {
public:
  static void init();

  // Attempt to acquire operation slot. Returns false if busy.
  static bool acquire(SystemOperation op);

  // Release the operation slot (return to IDLE).
  static void release();

  // Force release (for safety timeouts, errors)
  static void forceRelease();

  // Query
  static SystemOperation getCurrent();
  static bool isIdle();
  static bool isWebOperation();   // WEB_PURGING or WEB_CALIBRATING
  static bool isOledOperation();  // OLED_PURGING etc.
  static bool isDispensingOrPending();

private:
  static SystemOperation current;
};

#endif // OPERATION_STATE_H
