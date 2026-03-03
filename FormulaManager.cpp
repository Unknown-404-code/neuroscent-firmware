#include "FormulaManager.h"
#include "PumpManager.h"
#include "StorageManager.h"
#include "UIManager.h" // To communicate UI transitions
#include <ArduinoJson.h>

int FormulaManager::formulaId = 0;
String FormulaManager::formulaName = "";
uint8_t FormulaManager::containerMl = 0;

FormulaStep FormulaManager::steps[MAX_FORMULA_STEPS];
uint8_t FormulaManager::numSteps = 0;

uint8_t FormulaManager::currentStep = 0;
unsigned long FormulaManager::stepStartTime = 0;
bool FormulaManager::isExecuting = false;

void FormulaManager::init() {
  isExecuting = false;
  numSteps = 0;
}

bool FormulaManager::parseAndPrepare(const String &jsonStr) {
  // 512 bytes is generous for our expected JSON size
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  if (!doc.containsKey("id") || !doc.containsKey("name") ||
      !doc.containsKey("container_ml") || !doc.containsKey("formula")) {
    return false;
  }

  formulaId = doc["id"];
  formulaName = doc["name"].as<String>();
  containerMl = doc["container_ml"];

  JsonArray formulaArray = doc["formula"];
  numSteps = 0;

  for (JsonObject step : formulaArray) {
    if (numSteps >= MAX_FORMULA_STEPS)
      break;

    // 1-indexed in JSON, convert to 0-indexed for array
    uint8_t pumpIdRaw = step["pump"];
    if (pumpIdRaw < 1 || pumpIdRaw > NUM_PUMPS)
      continue;

    uint8_t pIdx = pumpIdRaw - 1;
    uint8_t pct = step["percent"];

    float reqMl = (float)containerMl * ((float)pct / 100.0f);
    float mlPerMs = StorageManager::getCalibration(pIdx);
    unsigned long reqMs = (unsigned long)(reqMl / mlPerMs);

    // Pre-Check
    if (StorageManager::getRemainingMl(pIdx) < reqMl) {
      String err = "P" + String(pumpIdRaw) + " Low Vol!";
      UIManager::triggerFormulaError(err);
      return false; // Valid JSON but rejected based on business logic
    }

    steps[numSteps].pumpIndex = pIdx;
    steps[numSteps].percent = pct;
    steps[numSteps].requiredMl = reqMl;
    steps[numSteps].requiredMs = reqMs;
    numSteps++;
  }

  if (numSteps == 0)
    return false;

  // Trigger UI Confirm Screen
  UIManager::triggerFormulaConfirm(formulaId, formulaName);
  return true;
}

void FormulaManager::update() {
  UIState uiSyncState = UIManager::getState();

  // The user pressed START on the UI -> State transitioned to RUNNING
  // But our internal machine hasn't started yet.
  if (uiSyncState == UIState::FORMULA_RUNNING && !isExecuting) {
    isExecuting = true;
    currentStep = 0;

    if (numSteps > 0) {
      stepStartTime = millis();
      PumpManager::startPump(steps[currentStep].pumpIndex);
      UIManager::updateFormulaProgress(steps[currentStep].pumpIndex, false);
    } else {
      finishFormula();
    }
  }

  // Sequence execution (non-blocking)
  if (isExecuting) {
    // If UI transitioned away to stop confirm, the pump should KEEP running!
    // We only stop it if they answer YES and the state transitions to MAIN_MENU
    if (uiSyncState == UIState::MAIN_MENU) {
      // User aborted
      abortFormula("Aborted");
      return;
    }

    unsigned long elapsed = millis() - stepStartTime;
    if (elapsed >= steps[currentStep].requiredMs) {
      // Stop current pump
      PumpManager::stopPump(steps[currentStep].pumpIndex);

      // Deduct ml
      StorageManager::dispenseFromContainer(steps[currentStep].pumpIndex,
                                            steps[currentStep].requiredMl);

      currentStep++;
      if (currentStep < numSteps) {
        // Next Pump
        stepStartTime = millis();
        PumpManager::startPump(steps[currentStep].pumpIndex);
        UIManager::updateFormulaProgress(steps[currentStep].pumpIndex, false);
      } else {
        // Done
        finishFormula();
      }
    }
  }
}

void FormulaManager::abortFormula(const String &errMsg) {
  if (isExecuting) {
    PumpManager::stopAll();
    isExecuting = false;
  }
}

void FormulaManager::finishFormula() {
  isExecuting = false;
  PumpManager::stopAll();
  UIManager::updateFormulaProgress(0, true);
}
