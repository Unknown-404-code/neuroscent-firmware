#ifndef FORMULA_MANAGER_H
#define FORMULA_MANAGER_H

#include <Arduino.h>

#define MAX_FORMULA_STEPS 8 // At most, one step per pump

struct FormulaStep {
  uint8_t pumpIndex;
  uint8_t percent;
  float requiredMl;
  unsigned long requiredMs;
};

class FormulaManager {
public:
  static void init();
  static void update();

  // Parse json and prepare sequence if valid
  static bool parseAndPrepare(const String &jsonStr);

private:
  static int formulaId;
  static String formulaName;
  static uint8_t containerMl;

  static FormulaStep steps[MAX_FORMULA_STEPS];
  static uint8_t numSteps;

  static uint8_t currentStep;
  static unsigned long stepStartTime;
  static bool isExecuting;

  static void abortFormula(const String &errMsg);
  static void finishFormula();
};

#endif // FORMULA_MANAGER_H
