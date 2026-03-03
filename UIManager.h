#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "Config.h"

enum class UIState {
  MAIN_MENU,
  PURGE_MENU,
  PURGE_RUNNING,
  CALIBRATION_MENU,
  CALIBRATION_RUNNING,
  VIEW_CALIB_MENU,
  VIEW_CALIB_CONFIRM,
  ACCORDS_MENU,
  ACCORDS_CONFIRM,
  TEST_SELECT_PUMP,
  TEST_SELECT_CONTAINER,
  TEST_SELECT_PERCENT,
  TEST_RUNNING,
  TEST_STOP_CONFIRM,
  FORMULA_CONFIRM,
  FORMULA_RUNNING,
  FORMULA_STOP_CONFIRM,
  FORMULA_FINISHED,
  FORMULA_ERROR
};

class UIManager {
public:
  static void init();
  static void update(ButtonEvent event);

  // For FormulaManager to inject state
  static void triggerFormulaConfirm(int id, const String &name);
  static void triggerFormulaError(const String &msg);
  static void updateFormulaProgress(uint8_t currentPumpIndex, bool finished);

  static UIState getState() { return currentState; }

private:
  static UIState currentState;
  static UIState
      previousState; // Useful for returning from confirmation dialogs

  // Generalized Menu State Variables
  static uint8_t menuSelectedIndex;
  static uint8_t menuScrollOffset;

  // Page-specific variables
  static uint8_t selectedPump;
  // Calibration
  static unsigned long calibrationStartTime;
  // View/Accords confirm target
  static uint8_t confirmTargetPump;
  // Test Mode Variables
  static uint8_t testContainerMl;
  static uint8_t testPercent;
  static unsigned long testStartTime;
  static unsigned long testDurationMs;
  // Formula Mode Variables
  static int formulaId;
  static String formulaName;
  static String formulaErrorMsg;
  static uint8_t activeFormulaPumpIndex;
  static bool formulaDone;

  // View renderers
  static void render();
  static void renderMainMenu();
  static void renderPurgeMenu();
  static void renderPurgeRunning();
  static void renderCalibrationMenu();
  static void renderCalibrationRunning();
  static void renderViewCalibMenu();
  static void renderViewCalibConfirm();
  static void renderAccordsMenu();
  static void renderAccordsConfirm();
  static void renderTestSelectPump();
  static void renderTestSelectContainer();
  static void renderTestSelectPercent();
  static void renderTestRunning();
  static void renderTestStopConfirm();
  static void renderFormulaConfirm();
  static void renderFormulaRunning();
  static void renderFormulaStopConfirm();
  static void renderFormulaFinished();
  static void renderFormulaError();

  // Event handlers
  static void handleMainMenu(ButtonEvent event);
  static void handlePurgeMenu(ButtonEvent event);
  static void handlePurgeRunning(ButtonEvent event);
  static void handleCalibrationMenu(ButtonEvent event);
  static void handleCalibrationRunning(ButtonEvent event);
  static void handleViewCalibMenu(ButtonEvent event);
  static void handleViewCalibConfirm(ButtonEvent event);
  static void handleAccordsMenu(ButtonEvent event);
  static void handleAccordsConfirm(ButtonEvent event);
  static void handleTestSelectPump(ButtonEvent event);
  static void handleTestSelectContainer(ButtonEvent event);
  static void handleTestSelectPercent(ButtonEvent event);
  static void handleTestRunning(ButtonEvent event);
  static void handleTestStopConfirm(ButtonEvent event);
  static void handleFormulaConfirm(ButtonEvent event);
  static void handleFormulaRunning(ButtonEvent event);
  static void handleFormulaStopConfirm(ButtonEvent event);
  static void handleFormulaFinished(ButtonEvent event);
  static void handleFormulaError(ButtonEvent event);

  // Helpers
  static void changeState(UIState newState);
  static void scrollMenu(ButtonEvent event, uint8_t numItems,
                         uint8_t visibleItems = 5);
};

#endif // UI_MANAGER_H
