#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include "Config.h"
#include <Arduino.h>

// Wi-Fi connection states
enum class WiFiState {
  DISCONNECTED,     // Not connected, not in AP mode
  CONNECTING,       // Attempting to connect to stored credentials
  STA_CONNECTED,    // Connected to router as station
  AP_ACTIVE,        // Running setup access point
  RECONNECTING      // Lost connection, attempting reconnect
};

class WiFiProvisioning {
public:
  static void init();
  static void update();  // Call from main loop — non-blocking

  // --- State queries ---
  static WiFiState getState();
  static bool isStaConnected();  // true only when STA_CONNECTED
  static bool isApActive();

  // --- Network info ---
  static String getSSID();
  static String getLocalIP();
  static String getMdnsHostname();
  static int8_t getRssi();

  // --- Credential management ---
  static bool hasSavedCredentials();
  static bool attemptConnect(const String& ssid, const String& password);
  static void clearSavedCredentials();

  // --- AP management ---
  static void startSetupAP();
  static void stopSetupAP();
  static String getApIP();
  static String getApSSID();

  // --- Uptime ---
  static unsigned long getConnectedDurationMs();

private:
  static WiFiState currentState;
  static unsigned long connectStartTime;
  static unsigned long lastReconnectAttempt;
  static unsigned long connectedSince;

  static String cachedSSID;
  static String cachedIP;

  static void loadAndConnect();
  static void startMdns();
  static void stopMdns();
  static void transitionTo(WiFiState newState);
};

#endif // WIFI_PROVISIONING_H
