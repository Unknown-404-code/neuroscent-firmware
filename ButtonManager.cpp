#include "ButtonManager.h"

ButtonEvent ButtonManager::currentEvent = ButtonEvent::NONE;

ButtonManager::ButtonState ButtonManager::buttons[4] = {
    {BTN_UP_PIN, HIGH, HIGH, 0, ButtonEvent::UP, false, 0},
    {BTN_DOWN_PIN, HIGH, HIGH, 0, ButtonEvent::DOWN, false, 0},
    {BTN_SELECT_PIN, HIGH, HIGH, 0, ButtonEvent::SELECT, false, 0},
    {BTN_BACK_PIN, HIGH, HIGH, 0, ButtonEvent::BACK, false, 0}};

void ButtonManager::init() {
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
}

void ButtonManager::update() {
  for (int i = 0; i < 4; i++) {
    checkButton(buttons[i]);
  }
}

void ButtonManager::checkButton(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);
  unsigned long now = millis();

  if (reading != btn.lastState) {
    btn.lastDebounceTime = now;
  }

  if ((now - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != btn.currentState) {
      btn.currentState = reading;

      // Trigger event only on PRESS (transition from HIGH to LOW)
      if (btn.currentState == LOW) {
        currentEvent = btn.eventToTrigger;

        // Start auto-repeat only for UP and DOWN buttons
        if (btn.eventToTrigger == ButtonEvent::UP ||
            btn.eventToTrigger == ButtonEvent::DOWN) {
          btn.isRepeating = true;
          btn.lastRepeatTime = now;
        }
      } else {
        // Released
        btn.isRepeating = false;
      }
    } else if (btn.currentState == LOW && btn.isRepeating) {
      // Button is being held down.
      unsigned long holdTime = now - btn.lastDebounceTime;

      // Do we start repeating?
      if (holdTime > BUTTON_REPEAT_FIRST_DELAY_MS) {
        unsigned long repeatDelay = BUTTON_REPEAT_START_DELAY_MS;
        unsigned long activeHoldTime = holdTime - BUTTON_REPEAT_FIRST_DELAY_MS;

        // Ramp up speed (decrease delay)
        if (activeHoldTime >= BUTTON_REPEAT_RAMP_TIME_MS) {
          repeatDelay = BUTTON_REPEAT_MIN_DELAY_MS;
        } else {
          // Linear interpolation to reach min delay
          float progress = (float)activeHoldTime / BUTTON_REPEAT_RAMP_TIME_MS;
          repeatDelay = BUTTON_REPEAT_START_DELAY_MS -
                        (progress * (BUTTON_REPEAT_START_DELAY_MS -
                                     BUTTON_REPEAT_MIN_DELAY_MS));
        }

        // Trigger next event if enough time passed since last repeat
        if ((now - btn.lastRepeatTime) >= repeatDelay) {
          currentEvent = btn.eventToTrigger;
          btn.lastRepeatTime = now;
        }
      }
    }
  }

  btn.lastState = reading;
}

ButtonEvent ButtonManager::getEvent() {
  ButtonEvent ev = currentEvent;
  currentEvent = ButtonEvent::NONE;
  return ev;
}
