#include "AuthManager.h"
#include "ButtonManager.h"
#include "Config.h"
#include "DisplayManager.h"
#include "FormulaManager.h"
#include "LogManager.h"
#include "OperationState.h"
#include "PumpManager.h"
#include "StorageManager.h"
#include "UIManager.h"
#include "WebServerManager.h"
#include "WiFiProvisioning.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("\n\n--- NeuroScent Firmware " FIRMWARE_VERSION " ---");

  // 1. Initialize Log System (first — everything else can log)
  LogManager::init();

  // 2. Initialize Hardware Abstractions
  ButtonManager::init();
  PumpManager::init();

  // 3. Initialize Displays
  DisplayManager::init();

  // 4. Initialize Storage (NVS)
  StorageManager::init();

  // 5. Initialize Operation State
  OperationState::init();

  // 6. Initialize Authentication
  AuthManager::init();

  // 7. Initialize Wi-Fi Provisioning (non-blocking: starts AP or STA attempt)
  WiFiProvisioning::init();

  // 8. Initialize Web Server (starts HTTP server; Wi-Fi managed separately)
  WebServerManager::init();

  // 9. Initialize Application Logic
  FormulaManager::init();
  UIManager::init();  // Must be initialized after DisplayManager

  Serial.println("System Initialization Complete.");
  Serial.println("Web dashboard: http://" + String(MDNS_HOSTNAME) + ".local");

  // Safety Stop — ensure all pumps are OFF
  PumpManager::stopAll();
}

void loop() {
  // Strictly Non-Blocking Main Loop

  // 1. Hardware Input
  ButtonManager::update();
  ButtonEvent ev = ButtonManager::getEvent();

  // 2. UI Logic (OLED state machine)
  UIManager::update(ev);

  // 3. Formula Execution Logic
  FormulaManager::update();

  // 4. Wi-Fi Management (non-blocking: reconnect, captive portal DNS)
  WiFiProvisioning::update();

  // 5. Network/Web Server (HTTP client handling + safety timeouts)
  WebServerManager::update();

  // 6. Auth session cleanup (infrequent, lightweight)
  AuthManager::update();
}
