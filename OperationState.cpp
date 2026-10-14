#include "OperationState.h"

SystemOperation OperationState::current = SystemOperation::IDLE;

void OperationState::init() {
  current = SystemOperation::IDLE;
}

bool OperationState::acquire(SystemOperation op) {
  if (current != SystemOperation::IDLE) {
    return false; // Already busy
  }
  current = op;
  return true;
}

void OperationState::release() {
  current = SystemOperation::IDLE;
}

void OperationState::forceRelease() {
  current = SystemOperation::IDLE;
}

SystemOperation OperationState::getCurrent() { return current; }

bool OperationState::isIdle() { return current == SystemOperation::IDLE; }

bool OperationState::isWebOperation() {
  return current == SystemOperation::WEB_PURGING ||
         current == SystemOperation::WEB_CALIBRATING;
}

bool OperationState::isOledOperation() {
  return current == SystemOperation::OLED_PURGING ||
         current == SystemOperation::OLED_CALIBRATING ||
         current == SystemOperation::OLED_TESTING;
}

bool OperationState::isDispensingOrPending() {
  return current == SystemOperation::FORMULA_PENDING ||
         current == SystemOperation::FORMULA_DISPENSING;
}
