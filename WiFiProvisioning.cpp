#include "WiFiProvisioning.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

// --------------------------------------------------------
// Static Members
// --------------------------------------------------------
WiFiState WiFiProvisioning::currentState = WiFiState::DISCONNECTED;
unsigned long WiFiProvisioning::connectStartTime = 0;
unsigned long WiFiProvisioning::lastReconnectAttempt = 0;
unsigned long WiFiProvisioning::connectedSince = 0;
String WiFiProvisioning::cachedSSID = "";
String WiFiProvisioning::cachedIP = "";

// DNS server for captive portal (AP mode)
static DNSServer dnsServer;
static bool dnsServerRunning = false;

// NVS namespace for network credentials
static Preferences netPrefs;
static const char* NET_NS = "network";
static const char* KEY_SSID = "ssid";
static const char* KEY_PASS = "pass";
static const char* KEY_AP_PASS = "ap_pass";

// --------------------------------------------------------
// Init
// --------------------------------------------------------
void WiFiProvisioning::init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // We manage reconnection manually
  WiFi.disconnect(true);

  // Try loading and connecting to saved credentials
  loadAndConnect();
}

// --------------------------------------------------------
// Non-blocking update loop
// --------------------------------------------------------
void WiFiProvisioning::update() {
  unsigned long now = millis();

  switch (currentState) {
    case WiFiState::CONNECTING: {
      if (WiFi.status() == WL_CONNECTED) {
        connectedSince = now;
        cachedSSID = WiFi.SSID();
        cachedIP = WiFi.localIP().toString();
        startMdns();
        transitionTo(WiFiState::STA_CONNECTED);
        Serial.println("[WiFi] Connected! IP: " + cachedIP);
        Serial.println("[WiFi] mDNS: http://" + String(MDNS_HOSTNAME) + ".local");
      } else if (now - connectStartTime >= WIFI_CONNECT_TIMEOUT_MS) {
        // Timeout — switch to AP mode
        Serial.println("[WiFi] Connection timed out, starting setup AP.");
        WiFi.disconnect(true);
        startSetupAP();
      }
      break;
    }

    case WiFiState::STA_CONNECTED: {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost, will attempt reconnection.");
        stopMdns();
        lastReconnectAttempt = now;
        transitionTo(WiFiState::RECONNECTING);
      }
      break;
    }

    case WiFiState::RECONNECTING: {
      if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
        lastReconnectAttempt = now;
        Serial.println("[WiFi] Attempting reconnection...");
        loadAndConnect();
      }
      break;
    }

    case WiFiState::AP_ACTIVE: {
      // Service captive portal DNS
      if (dnsServerRunning) {
        dnsServer.processNextRequest();
      }
      break;
    }

    case WiFiState::DISCONNECTED:
    default:
      break;
  }
}

// --------------------------------------------------------
// State Queries
// --------------------------------------------------------
WiFiState WiFiProvisioning::getState() { return currentState; }

bool WiFiProvisioning::isStaConnected() {
  return currentState == WiFiState::STA_CONNECTED;
}

bool WiFiProvisioning::isApActive() {
  return currentState == WiFiState::AP_ACTIVE;
}

String WiFiProvisioning::getSSID() {
  if (isStaConnected()) return cachedSSID;
  if (isApActive()) return String(WIFI_AP_SSID);
  return "";
}

String WiFiProvisioning::getLocalIP() {
  if (isStaConnected()) return cachedIP;
  if (isApActive()) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

String WiFiProvisioning::getMdnsHostname() {
  return String(MDNS_HOSTNAME) + ".local";
}

int8_t WiFiProvisioning::getRssi() {
  if (isStaConnected()) return WiFi.RSSI();
  return 0;
}

unsigned long WiFiProvisioning::getConnectedDurationMs() {
  if (isStaConnected()) return millis() - connectedSince;
  return 0;
}

// --------------------------------------------------------
// Credential Management
// --------------------------------------------------------
bool WiFiProvisioning::hasSavedCredentials() {
  netPrefs.begin(NET_NS, true); // read-only
  bool has = netPrefs.isKey(KEY_SSID) && netPrefs.getString(KEY_SSID, "").length() > 0;
  netPrefs.end();
  return has;
}

bool WiFiProvisioning::attemptConnect(const String& ssid, const String& password) {
  if (ssid.isEmpty()) return false;

  // Save to NVS
  netPrefs.begin(NET_NS, false);
  netPrefs.putString(KEY_SSID, ssid);
  netPrefs.putString(KEY_PASS, password);
  netPrefs.end();

  Serial.println("[WiFi] Saving credentials and connecting to: " + ssid);

  // If AP is active, stop it
  if (isApActive()) {
    stopSetupAP();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  connectStartTime = millis();
  transitionTo(WiFiState::CONNECTING);
  return true;
}

void WiFiProvisioning::clearSavedCredentials() {
  netPrefs.begin(NET_NS, false);
  netPrefs.remove(KEY_SSID);
  netPrefs.remove(KEY_PASS);
  netPrefs.end();
  Serial.println("[WiFi] Credentials cleared.");
}

// --------------------------------------------------------
// AP Management
// --------------------------------------------------------
void WiFiProvisioning::startSetupAP() {
  // Load or set default AP password
  netPrefs.begin(NET_NS, false);
  if (!netPrefs.isKey(KEY_AP_PASS)) {
    netPrefs.putString(KEY_AP_PASS, WIFI_AP_DEFAULT_PASS);
  }
  String apPass = netPrefs.getString(KEY_AP_PASS, WIFI_AP_DEFAULT_PASS);
  netPrefs.end();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, apPass.c_str());

  delay(100); // Brief delay for AP to start

  // Start captive portal DNS (redirects all DNS to 192.168.4.1)
  dnsServer.start(53, "*", WiFi.softAPIP());
  dnsServerRunning = true;

  Serial.println("[WiFi] Setup AP started.");
  Serial.print("[WiFi] AP SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("[WiFi] AP IP: ");
  Serial.println(WiFi.softAPIP().toString());

  transitionTo(WiFiState::AP_ACTIVE);
}

void WiFiProvisioning::stopSetupAP() {
  if (dnsServerRunning) {
    dnsServer.stop();
    dnsServerRunning = false;
  }
  WiFi.softAPdisconnect(true);
  Serial.println("[WiFi] Setup AP stopped.");
}

String WiFiProvisioning::getApIP() {
  return WiFi.softAPIP().toString();
}

String WiFiProvisioning::getApSSID() {
  return String(WIFI_AP_SSID);
}

// --------------------------------------------------------
// Private Helpers
// --------------------------------------------------------
void WiFiProvisioning::loadAndConnect() {
  if (!hasSavedCredentials()) {
    Serial.println("[WiFi] No saved credentials, starting setup AP.");
    startSetupAP();
    return;
  }

  netPrefs.begin(NET_NS, true);
  String ssid = netPrefs.getString(KEY_SSID, "");
  String pass = netPrefs.getString(KEY_PASS, "");
  netPrefs.end();

  Serial.println("[WiFi] Connecting to: " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  connectStartTime = millis();
  transitionTo(WiFiState::CONNECTING);
}

void WiFiProvisioning::startMdns() {
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.println("[mDNS] Started: http://" + String(MDNS_HOSTNAME) + ".local");
  } else {
    Serial.println("[mDNS] Failed to start.");
  }
}

void WiFiProvisioning::stopMdns() {
  MDNS.end();
}

void WiFiProvisioning::transitionTo(WiFiState newState) {
  currentState = newState;
}
