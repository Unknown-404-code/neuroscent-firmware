#include "ButtonManager.h"
#include "Config.h"
#include "DisplayManager.h"
#include "FormulaManager.h"
#include "PumpManager.h"
#include "StorageManager.h"
#include "UIManager.h"
#include "WebServerManager.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(10); // Standard brief delay for Serial monitor to attach
  Serial.println("\n\n--- ESP32 Perfume Dispenser System ---");

  // 1. Initialize Hardware Abstractions
  ButtonManager::init();
  PumpManager::init();

  // 2. Initialize Displays
  DisplayManager::init();

  // 3. Initialize Storage Component (NVS)
  StorageManager::init();

  // 4. Initialize Network & Web Server
  WebServerManager::init();

  // 5. Initialize Application Logic
  FormulaManager::init();
  UIManager::init(); // Must be initialized after DisplayManager

  Serial.println("System Initialization Complete.");

  // Safety Stop
  PumpManager::stopAll();
}

void loop() {
  // Strictly Non-Blocking Main Loop

  // 1. Hardware Input
  ButtonManager::update();
  ButtonEvent ev = ButtonManager::getEvent();

  // 2. UI Logic
  UIManager::update(ev);

  // 3. Formula Execution Logic
  FormulaManager::update();

  // 4. Network and Web Server
  WebServerManager::update();
}
