#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "Config.h"

class ButtonManager {
public:
  static void init();
  static void update();

  // Returns the event that was triggered and clears it
  static ButtonEvent getEvent();

private:
  static ButtonEvent currentEvent;

  struct ButtonState {
    uint8_t pin;
    bool lastState;
    bool currentState;
    unsigned long lastDebounceTime;
    ButtonEvent eventToTrigger;
  };

  static ButtonState buttons[4];
  static void checkButton(ButtonState &btn);
};

#endif // BUTTON_MANAGER_H
