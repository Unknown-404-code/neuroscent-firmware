#include "UIManager.h"
#include "DisplayManager.h"
#include "FormulaManager.h"
#include "PumpManager.h"
#include "StorageManager.h"

// --------------------------------------------------------
// Static Variables Initialization
// --------------------------------------------------------
UIState UIManager::currentState = UIState::MAIN_MENU;
UIState UIManager::previousState = UIState::MAIN_MENU;

uint8_t UIManager::menuSelectedIndex = 0;
uint8_t UIManager::menuScrollOffset = 0;
uint8_t UIManager::selectedPump = 0;

unsigned long UIManager::calibrationStartTime = 0;
uint8_t UIManager::confirmTargetPump = 0;

uint8_t UIManager::testContainerMl = 10;
uint8_t UIManager::testPercent = 5;
unsigned long UIManager::testStartTime = 0;
unsigned long UIManager::testDurationMs = 0;

int UIManager::formulaId = 0;
String UIManager::formulaName = "";
String UIManager::formulaErrorMsg = "";
uint8_t UIManager::activeFormulaPumpIndex = 0;
bool UIManager::formulaDone = false;

// --------------------------------------------------------
// Menu Constants
// --------------------------------------------------------
const char *MAIN_MENU_ITEMS[] = {"Purging", "Calibration", "View Calibs",
                                 "Accords List", "Test"};
const uint8_t MAIN_MENU_SIZE = 5;

const char *PUMPS_STR[] = {"Pump 1", "Pump 2", "Pump 3", "Pump 4",
                           "Pump 5", "Pump 6", "Pump 7", "Pump 8"};
const uint8_t NUM_PUMPS_UI = 8;

const char *CONTAINERS_STR[] = {"10 ml", "20 ml", "30 ml", "50 ml", "100 ml"};
const uint8_t CONTAINERS_VAL[] = {10, 20, 30, 50, 100};
const uint8_t NUM_CONTAINERS = 5;

const char *PERCENTS_STR[] = {"5 %", "7 %", "10 %", "25 %", "32 %", "46 %"};
const uint8_t PERCENTS_VAL[] = {5, 7, 10, 25, 32, 46};
const uint8_t NUM_PERCENTS = 6;

// --------------------------------------------------------
// Init & Update
// --------------------------------------------------------
void UIManager::init() { changeState(UIState::MAIN_MENU); }

void UIManager::update(ButtonEvent event) {
  // Handle state transitions based on input
  switch (currentState) {
  case UIState::MAIN_MENU:
    handleMainMenu(event);
    break;
  case UIState::PURGE_MENU:
    handlePurgeMenu(event);
    break;
  case UIState::PURGE_RUNNING:
    handlePurgeRunning(event);
    break;
  case UIState::CALIBRATION_MENU:
    handleCalibrationMenu(event);
    break;
  case UIState::CALIBRATION_RUNNING:
    handleCalibrationRunning(event);
    break;
  case UIState::VIEW_CALIB_MENU:
    handleViewCalibMenu(event);
    break;
  case UIState::VIEW_CALIB_CONFIRM:
    handleViewCalibConfirm(event);
    break;
  case UIState::ACCORDS_MENU:
    handleAccordsMenu(event);
    break;
  case UIState::ACCORDS_CONFIRM:
    handleAccordsConfirm(event);
    break;
  case UIState::TEST_SELECT_PUMP:
    handleTestSelectPump(event);
    break;
  case UIState::TEST_SELECT_CONTAINER:
    handleTestSelectContainer(event);
    break;
  case UIState::TEST_SELECT_PERCENT:
    handleTestSelectPercent(event);
    break;
  case UIState::TEST_RUNNING:
    handleTestRunning(event);
    break;
  case UIState::TEST_STOP_CONFIRM:
    handleTestStopConfirm(event);
    break;
  case UIState::FORMULA_CONFIRM:
    handleFormulaConfirm(event);
    break;
  case UIState::FORMULA_RUNNING:
    handleFormulaRunning(event);
    break;
  case UIState::FORMULA_STOP_CONFIRM:
    handleFormulaStopConfirm(event);
    break;
  case UIState::FORMULA_FINISHED:
    handleFormulaFinished(event);
    break;
  case UIState::FORMULA_ERROR:
    handleFormulaError(event);
    break;
  }

  // Always render periodically, or at least every tick
  // (If performance is an issue, we can optimize to dirty-flags)
  render();
}

void UIManager::changeState(UIState newState) {
  if (newState != UIState::TEST_STOP_CONFIRM &&
      newState != UIState::FORMULA_STOP_CONFIRM &&
      newState != UIState::VIEW_CALIB_CONFIRM &&
      newState != UIState::ACCORDS_CONFIRM) {
    // Don't overwrite previousState if we're just going to a popup
    previousState = currentState;
  }

  currentState = newState;

  // Reset cursor when entering a new list menu
  if (newState == UIState::MAIN_MENU || newState == UIState::PURGE_MENU ||
      newState == UIState::CALIBRATION_MENU ||
      newState == UIState::VIEW_CALIB_MENU ||
      newState == UIState::ACCORDS_MENU ||
      newState == UIState::TEST_SELECT_PUMP) {
    menuSelectedIndex = 0;
    menuScrollOffset = 0;
  }
}

// --------------------------------------------------------
// Helper
// --------------------------------------------------------
void UIManager::scrollMenu(ButtonEvent event, uint8_t numItems,
                           uint8_t visibleItems) {
  if (event == ButtonEvent::DOWN) {
    if (menuSelectedIndex < numItems - 1) {
      menuSelectedIndex++;
      if (menuSelectedIndex >= menuScrollOffset + visibleItems) {
        menuScrollOffset++;
      }
    }
  } else if (event == ButtonEvent::UP) {
    if (menuSelectedIndex > 0) {
      menuSelectedIndex--;
      if (menuSelectedIndex < menuScrollOffset) {
        menuScrollOffset--;
      }
    }
  }
}

// --------------------------------------------------------
// Event Handlers
// --------------------------------------------------------

void UIManager::handleMainMenu(ButtonEvent event) {
  scrollMenu(event, MAIN_MENU_SIZE);

  if (event == ButtonEvent::SELECT) {
    switch (menuSelectedIndex) {
    case 0:
      changeState(UIState::PURGE_MENU);
      break;
    case 1:
      changeState(UIState::CALIBRATION_MENU);
      break;
    case 2:
      changeState(UIState::VIEW_CALIB_MENU);
      break;
    case 3:
      changeState(UIState::ACCORDS_MENU);
      break;
    case 4:
      changeState(UIState::TEST_SELECT_PUMP);
      break;
    }
  }
}

void UIManager::handlePurgeMenu(ButtonEvent event) {
  scrollMenu(event, NUM_PUMPS_UI);

  if (event == ButtonEvent::SELECT) {
    selectedPump = menuSelectedIndex;
    PumpManager::startPump(selectedPump);
    changeState(UIState::PURGE_RUNNING);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handlePurgeRunning(ButtonEvent event) {
  if (event == ButtonEvent::BACK) {
    PumpManager::stopPump(selectedPump);
    changeState(UIState::PURGE_MENU);
  }
}

void UIManager::handleCalibrationMenu(ButtonEvent event) {
  scrollMenu(event, NUM_PUMPS_UI);

  if (event == ButtonEvent::SELECT) {
    selectedPump = menuSelectedIndex;
    calibrationStartTime = millis();
    PumpManager::startPump(selectedPump);
    changeState(UIState::CALIBRATION_RUNNING);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleCalibrationRunning(ButtonEvent event) {
  if (event == ButtonEvent::BACK) {
    PumpManager::stopPump(selectedPump);

    unsigned long elapsed = millis() - calibrationStartTime;
    if (elapsed > 0) {
      float ml_per_ms = 100.0f / (float)elapsed; // Hardcoded 100ml calibration
      StorageManager::setCalibration(selectedPump, ml_per_ms);
    }

    // No confirmation needed according to instructions ("Show confirmation ->
    // SELECT -> return"), but let's just show a simple notification if we
    // wanted. For now just return to menu.
    changeState(UIState::CALIBRATION_MENU);
  }
}

void UIManager::handleViewCalibMenu(ButtonEvent event) {
  scrollMenu(event, NUM_PUMPS_UI);

  if (event == ButtonEvent::SELECT) {
    confirmTargetPump = menuSelectedIndex;
    // reuse generic selected index for confirm box (0=Confirm, 1=Cancel)
    menuSelectedIndex = 0;
    changeState(UIState::VIEW_CALIB_CONFIRM);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleViewCalibConfirm(ButtonEvent event) {
  if (event == ButtonEvent::UP || event == ButtonEvent::DOWN) {
    menuSelectedIndex = (menuSelectedIndex == 0) ? 1 : 0;
  } else if (event == ButtonEvent::SELECT) {
    if (menuSelectedIndex == 0) { // Confirm
      StorageManager::resetCalibration(confirmTargetPump);
    }
    changeState(UIState::VIEW_CALIB_MENU);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::VIEW_CALIB_MENU);
  }
}

void UIManager::handleAccordsMenu(ButtonEvent event) {
  scrollMenu(event, NUM_PUMPS_UI);

  if (event == ButtonEvent::SELECT) {
    confirmTargetPump = menuSelectedIndex;
    menuSelectedIndex = 0; // Confirm default
    changeState(UIState::ACCORDS_CONFIRM);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleAccordsConfirm(ButtonEvent event) {
  if (event == ButtonEvent::UP || event == ButtonEvent::DOWN) {
    menuSelectedIndex = (menuSelectedIndex == 0) ? 1 : 0;
  } else if (event == ButtonEvent::SELECT) {
    if (menuSelectedIndex == 0) { // Confirm
      StorageManager::resetContainer(confirmTargetPump);
    }
    changeState(UIState::ACCORDS_MENU);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::ACCORDS_MENU);
  }
}

void UIManager::handleTestSelectPump(ButtonEvent event) {
  scrollMenu(event, NUM_PUMPS_UI);

  if (event == ButtonEvent::SELECT) {
    selectedPump = menuSelectedIndex;
    menuSelectedIndex = 0;
    menuScrollOffset = 0;
    changeState(UIState::TEST_SELECT_CONTAINER);
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleTestSelectContainer(ButtonEvent event) {
  scrollMenu(event, NUM_CONTAINERS);

  if (event == ButtonEvent::SELECT) {
    testContainerMl = CONTAINERS_VAL[menuSelectedIndex];
    menuSelectedIndex = 0;
    menuScrollOffset = 0;
    changeState(UIState::TEST_SELECT_PERCENT);
  } else if (event == ButtonEvent::BACK) {
    menuSelectedIndex = selectedPump; // Restore position
    changeState(UIState::TEST_SELECT_PUMP);
  }
}

void UIManager::handleTestSelectPercent(ButtonEvent event) {
  scrollMenu(event, NUM_PERCENTS);

  if (event == ButtonEvent::SELECT) {
    testPercent = PERCENTS_VAL[menuSelectedIndex];

    // Calculate
    float required_ml = (float)testContainerMl * ((float)testPercent / 100.0f);
    float ml_per_ms = StorageManager::getCalibration(selectedPump);
    testDurationMs = (unsigned long)(required_ml / ml_per_ms);

    testStartTime = millis();
    PumpManager::startPump(selectedPump);

    changeState(UIState::TEST_RUNNING);
  } else if (event == ButtonEvent::BACK) {
    menuSelectedIndex = 0; // Don't care to restore exactly for simplicity
    changeState(UIState::TEST_SELECT_CONTAINER);
  }
}

void UIManager::handleTestRunning(ButtonEvent event) {
  // Check if test completed naturally
  if (PumpManager::isAnyPumpRunning() &&
      (millis() - testStartTime >= testDurationMs)) {
    PumpManager::stopPump(selectedPump);

    // Deduct from container
    float required_ml = (float)testContainerMl * ((float)testPercent / 100.0f);
    StorageManager::dispenseFromContainer(selectedPump, required_ml);

    changeState(UIState::MAIN_MENU); // Done
    return;
  }

  if (event == ButtonEvent::BACK) {
    menuSelectedIndex = 0; // Default to Yes
    changeState(UIState::TEST_STOP_CONFIRM);
  }
}

void UIManager::handleTestStopConfirm(ButtonEvent event) {
  if (event == ButtonEvent::UP || event == ButtonEvent::DOWN) {
    menuSelectedIndex = (menuSelectedIndex == 0) ? 1 : 0;
  } else if (event == ButtonEvent::SELECT) {
    if (menuSelectedIndex == 0) { // Yes
      PumpManager::stopPump(selectedPump);
      changeState(UIState::MAIN_MENU);
    } else { // No
      changeState(UIState::TEST_RUNNING);
    }
  } else if (event == ButtonEvent::BACK) { // Implied No
    changeState(UIState::TEST_RUNNING);
  }
}

// --------------------------------------------------------
// Formula UI Hooks (Called by FormulaManager)
// --------------------------------------------------------

void UIManager::triggerFormulaConfirm(int id, const String &name) {
  formulaId = id;
  formulaName = name;
  menuSelectedIndex = 0; // Default to Start
  changeState(UIState::FORMULA_CONFIRM);
}

void UIManager::triggerFormulaError(const String &msg) {
  formulaErrorMsg = msg;
  changeState(UIState::FORMULA_ERROR);
}

void UIManager::updateFormulaProgress(uint8_t currentPumpIndex, bool finished) {
  activeFormulaPumpIndex = currentPumpIndex;
  formulaDone = finished;

  if (finished && currentState == UIState::FORMULA_RUNNING) {
    changeState(UIState::FORMULA_FINISHED);
  }
}

// --------------------------------------------------------
// Formula State Handlers
// --------------------------------------------------------

void UIManager::handleFormulaConfirm(ButtonEvent event) {
  // 0 = Start, 1 = Cancel
  if (event == ButtonEvent::UP || event == ButtonEvent::DOWN) {
    menuSelectedIndex = (menuSelectedIndex == 0) ? 1 : 0;
  } else if (event == ButtonEvent::SELECT) {
    if (menuSelectedIndex == 0) {
      // Signal FormulaManager to proceed (This is a bit tricky: UIManager sets
      // state, FormulaManager observes state). We just change state to RUNNING.
      changeState(UIState::FORMULA_RUNNING);
    } else {
      changeState(UIState::MAIN_MENU);
    }
  } else if (event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleFormulaRunning(ButtonEvent event) {
  if (event == ButtonEvent::BACK) {
    menuSelectedIndex = 0; // Default to Yes
    changeState(UIState::FORMULA_STOP_CONFIRM);
  }
}

void UIManager::handleFormulaStopConfirm(ButtonEvent event) {
  if (event == ButtonEvent::UP || event == ButtonEvent::DOWN) {
    menuSelectedIndex = (menuSelectedIndex == 0) ? 1 : 0;
  } else if (event == ButtonEvent::SELECT) {
    if (menuSelectedIndex == 0) { // Yes
      PumpManager::stopAll();
      changeState(UIState::MAIN_MENU);
    } else { // No
      changeState(UIState::FORMULA_RUNNING);
    }
  } else if (event == ButtonEvent::BACK) { // Implied No
    changeState(UIState::FORMULA_RUNNING);
  }
}

void UIManager::handleFormulaFinished(ButtonEvent event) {
  if (event == ButtonEvent::SELECT || event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

void UIManager::handleFormulaError(ButtonEvent event) {
  if (event == ButtonEvent::SELECT || event == ButtonEvent::BACK) {
    changeState(UIState::MAIN_MENU);
  }
}

// --------------------------------------------------------
// Rendering Engine
// --------------------------------------------------------

void UIManager::render() {
  switch (currentState) {
  case UIState::MAIN_MENU:
    renderMainMenu();
    break;
  case UIState::PURGE_MENU:
    renderPurgeMenu();
    break;
  case UIState::PURGE_RUNNING:
    renderPurgeRunning();
    break;
  case UIState::CALIBRATION_MENU:
    renderCalibrationMenu();
    break;
  case UIState::CALIBRATION_RUNNING:
    renderCalibrationRunning();
    break;
  case UIState::VIEW_CALIB_MENU:
    renderViewCalibMenu();
    break;
  case UIState::VIEW_CALIB_CONFIRM:
    renderViewCalibConfirm();
    break;
  case UIState::ACCORDS_MENU:
    renderAccordsMenu();
    break;
  case UIState::ACCORDS_CONFIRM:
    renderAccordsConfirm();
    break;
  case UIState::TEST_SELECT_PUMP:
    renderTestSelectPump();
    break;
  case UIState::TEST_SELECT_CONTAINER:
    renderTestSelectContainer();
    break;
  case UIState::TEST_SELECT_PERCENT:
    renderTestSelectPercent();
    break;
  case UIState::TEST_RUNNING:
    renderTestRunning();
    break;
  case UIState::TEST_STOP_CONFIRM:
    renderTestStopConfirm();
    break;
  case UIState::FORMULA_CONFIRM:
    renderFormulaConfirm();
    break;
  case UIState::FORMULA_RUNNING:
    renderFormulaRunning();
    break;
  case UIState::FORMULA_STOP_CONFIRM:
    renderFormulaStopConfirm();
    break;
  case UIState::FORMULA_FINISHED:
    renderFormulaFinished();
    break;
  case UIState::FORMULA_ERROR:
    renderFormulaError();
    break;
  }
}

void UIManager::renderMainMenu() {
  DisplayManager::drawMenu(" MAIN MENU", MAIN_MENU_ITEMS, MAIN_MENU_SIZE,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderPurgeMenu() {
  DisplayManager::drawMenu(" PURGING", PUMPS_STR, NUM_PUMPS_UI,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderPurgeRunning() {
  DisplayManager::clear();
  char buf[32];
  sprintf(buf, "Pump %d Running", selectedPump + 1);
  DisplayManager::drawCenteredText(buf, 20);
  DisplayManager::drawCenteredText("Press BACK to stop", 40);
  DisplayManager::display();
}

void UIManager::renderCalibrationMenu() {
  DisplayManager::drawMenu(" CALIBRATION", PUMPS_STR, NUM_PUMPS_UI,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderCalibrationRunning() {
  DisplayManager::clear();
  DisplayManager::drawHeader(" CALIBRATING...");

  char buf[32];
  sprintf(buf, "Pump %d ON", selectedPump + 1);
  DisplayManager::drawCenteredText(buf, 20);

  unsigned long elapsed = millis() - calibrationStartTime;
  sprintf(buf, "Time: %lu ms", elapsed);
  DisplayManager::drawCenteredText(buf, 35);

  DisplayManager::drawCenteredText("Press BACK to save", 50);
  DisplayManager::display();
}

void UIManager::renderViewCalibMenu() {
  // Custom render because we need to show values alongside names
  DisplayManager::clear();
  DisplayManager::drawHeader(" VIEW CALIBRATIONS");

  uint8_t startY = 12;
  uint8_t visibleItems = 5;

  for (uint8_t i = 0; i < visibleItems; i++) {
    uint8_t itemIndex = menuScrollOffset + i;
    if (itemIndex >= NUM_PUMPS_UI)
      break;

    uint8_t yPos = startY + (i * 10);
    DisplayManager::getDisplay().setCursor(0, yPos);

    if (itemIndex == menuSelectedIndex) {
      DisplayManager::getDisplay().print("> ");
    } else {
      DisplayManager::getDisplay().print("  ");
    }

    char buf[32];
    float val = StorageManager::getCalibration(itemIndex);
    sprintf(buf, "P%d: %.4f ml/ms", itemIndex + 1, val);
    DisplayManager::getDisplay().print(buf);
  }
  DisplayManager::display();
}

void UIManager::renderViewCalibConfirm() {
  DisplayManager::clear();
  char buf[32];
  sprintf(buf, "Reset Pump %d ?", confirmTargetPump + 1);
  DisplayManager::drawCenteredText(buf, 10);

  DisplayManager::getDisplay().setCursor(30, 30);
  DisplayManager::getDisplay().print(menuSelectedIndex == 0 ? "> Confirm"
                                                            : "  Confirm");
  DisplayManager::getDisplay().setCursor(30, 45);
  DisplayManager::getDisplay().print(menuSelectedIndex == 1 ? "> Cancel"
                                                            : "  Cancel");

  DisplayManager::display();
}

void UIManager::renderAccordsMenu() {
  DisplayManager::clear();
  DisplayManager::drawHeader(" ACCORDS (ML)");

  uint8_t startY = 12;
  uint8_t visibleItems = 5;

  for (uint8_t i = 0; i < visibleItems; i++) {
    uint8_t itemIndex = menuScrollOffset + i;
    if (itemIndex >= NUM_PUMPS_UI)
      break;

    uint8_t yPos = startY + (i * 10);
    DisplayManager::getDisplay().setCursor(0, yPos);

    if (itemIndex == menuSelectedIndex) {
      DisplayManager::getDisplay().print("> ");
    } else {
      DisplayManager::getDisplay().print("  ");
    }

    char buf[32];
    float val = StorageManager::getRemainingMl(itemIndex);
    sprintf(buf, "P%d: %.1f ml", itemIndex + 1, val);
    DisplayManager::getDisplay().print(buf);
  }
  DisplayManager::display();
}

void UIManager::renderAccordsConfirm() {
  DisplayManager::clear();
  char buf[32];
  sprintf(buf, "Reset Pump %d ?", confirmTargetPump + 1);
  DisplayManager::drawCenteredText(buf, 10);
  DisplayManager::drawCenteredText("Reset to default?", 20);

  DisplayManager::getDisplay().setCursor(30, 40);
  DisplayManager::getDisplay().print(menuSelectedIndex == 0 ? "> Confirm"
                                                            : "  Confirm");
  DisplayManager::getDisplay().setCursor(30, 50);
  DisplayManager::getDisplay().print(menuSelectedIndex == 1 ? "> Cancel"
                                                            : "  Cancel");

  DisplayManager::display();
}

void UIManager::renderTestSelectPump() {
  DisplayManager::drawMenu(" TEST: PUMP", PUMPS_STR, NUM_PUMPS_UI,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderTestSelectContainer() {
  DisplayManager::drawMenu(" TEST: CONTAINER", CONTAINERS_STR, NUM_CONTAINERS,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderTestSelectPercent() {
  DisplayManager::drawMenu(" TEST: %", PERCENTS_STR, NUM_PERCENTS,
                           menuSelectedIndex, menuScrollOffset);
}

void UIManager::renderTestRunning() {
  DisplayManager::clear();
  DisplayManager::drawHeader(" TEST RUNNING");

  char buf[32];
  sprintf(buf, "Pump %d", selectedPump + 1);
  DisplayManager::drawCenteredText(buf, 20);

  unsigned long elapsed = millis() - testStartTime;
  uint8_t prog = 0;
  if (testDurationMs > 0) {
    prog = (elapsed * 100) / testDurationMs;
  }

  DisplayManager::drawProgressBar(14, 35, 100, 10, prog);

  DisplayManager::drawCenteredText("BACK to stop", 50);
  DisplayManager::display();
}

void UIManager::renderTestStopConfirm() {
  DisplayManager::clear();
  DisplayManager::drawCenteredText("Stop Test?", 10);

  DisplayManager::getDisplay().setCursor(40, 30);
  DisplayManager::getDisplay().print(menuSelectedIndex == 0 ? "> Yes"
                                                            : "  Yes");
  DisplayManager::getDisplay().setCursor(40, 45);
  DisplayManager::getDisplay().print(menuSelectedIndex == 1 ? "> No" : "  No");

  DisplayManager::display();
}

void UIManager::renderFormulaConfirm() {
  DisplayManager::clear();
  char buf[32];
  sprintf(buf, "ID: %d", formulaId);
  DisplayManager::drawCenteredText(buf, 10);
  DisplayManager::drawCenteredText(formulaName.c_str(), 20);

  DisplayManager::getDisplay().setCursor(30, 40);
  DisplayManager::getDisplay().print(menuSelectedIndex == 0 ? "> Start"
                                                            : "  Start");
  DisplayManager::getDisplay().setCursor(30, 50);
  DisplayManager::getDisplay().print(menuSelectedIndex == 1 ? "> Cancel"
                                                            : "  Cancel");

  DisplayManager::display();
}

void UIManager::renderFormulaRunning() {
  DisplayManager::clear();
  DisplayManager::drawHeader(" DISPENSING");

  // Simplistic view: Pump X ...
  // More complex view logic checking formula is handled by FormulaManager,
  // so we'll just show current pump based on activeFormulaPumpIndex

  char buf[32];
  sprintf(buf, "Running Pump %d", activeFormulaPumpIndex + 1);
  DisplayManager::drawCenteredText(buf, 20);

  uint8_t prog = FormulaManager::getCurrentStepProgress();
  DisplayManager::drawProgressBar(14, 35, 100, 10, prog);

  DisplayManager::display();
}

void UIManager::renderFormulaStopConfirm() {
  DisplayManager::clear();
  DisplayManager::drawCenteredText("Stop Formula?", 10);

  DisplayManager::getDisplay().setCursor(40, 30);
  DisplayManager::getDisplay().print(menuSelectedIndex == 0 ? "> Yes"
                                                            : "  Yes");
  DisplayManager::getDisplay().setCursor(40, 45);
  DisplayManager::getDisplay().print(menuSelectedIndex == 1 ? "> No" : "  No");

  DisplayManager::display();
}

void UIManager::renderFormulaFinished() {
  DisplayManager::clear();
  DisplayManager::drawCenteredText("Perfume Finished!", 25);
  DisplayManager::drawCenteredText("Press SELECT", 45);
  DisplayManager::display();
}

void UIManager::renderFormulaError() {
  DisplayManager::clear();
  DisplayManager::drawCenteredText("ERROR!", 10);
  DisplayManager::drawCenteredText(formulaErrorMsg.c_str(), 30);
  DisplayManager::drawCenteredText("Press SELECT", 50);
  DisplayManager::display();
}
