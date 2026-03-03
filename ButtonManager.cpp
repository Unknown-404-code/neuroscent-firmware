#include "ButtonManager.h"

ButtonEvent ButtonManager::currentEvent = ButtonEvent::NONE;

ButtonManager::ButtonState ButtonManager::buttons[4] = {
    {BTN_UP_PIN, HIGH, HIGH, 0, ButtonEvent::UP},
    {BTN_DOWN_PIN, HIGH, HIGH, 0, ButtonEvent::DOWN},
    {BTN_SELECT_PIN, HIGH, HIGH, 0, ButtonEvent::SELECT},
    {BTN_BACK_PIN, HIGH, HIGH, 0, ButtonEvent::BACK}};

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

  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != btn.currentState) {
      btn.currentState = reading;

      // Trigger event only on PRESS (transition from HIGH to LOW)
      if (btn.currentState == LOW) {
        currentEvent = btn.eventToTrigger;
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
