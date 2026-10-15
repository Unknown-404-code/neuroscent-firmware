#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include "Config.h"
#include <WebServer.h>
#include <WiFi.h>

class WebServerManager {
public:
  static void init();
  static void update();

  // State accessors (for OLED network page)
  static bool isConnected();

  // Web purge/calibration state (for safety timeouts)
  static bool isWebPurging();
  static bool isWebCalibrating();
  static void stopWebPurge();   // Called by timeout or explicit stop
  static void stopWebCalib();

private:
  static WebServer server;

  // Web operation state
  static int8_t webPumpIndex;           // -1 = none
  static unsigned long webOpStartTime;  // millis() when operation started
  static bool webPurging;
  static bool webCalibrating;
  static unsigned long webTestMs;       // >0 while a timed web pump test runs

  // ---- Static file serving ----
  static void handleRoot();
  static void handleStaticFile();
  static void handleNotFound();
  static bool serveFromLittleFS(const String& path);
  static String getMimeType(const String& path);

  // ---- Legacy API (preserved) ----
  static void handleDispense();

  // ---- Auth API ----
  static void handleAuthLogin();
  static void handleAuthLogout();
  static void handleAuthStatus();

  // ---- Status API ----
  static void handleApiStatus();
  static void handleApiNetwork();
  static void handleApiDevice();

  // ---- Pumps API ----
  static void handleApiPumps();
  static void handleApiPurgeStart();
  static void handleApiPurgeStop();
  static void handleApiPumpTest();

  // ---- Calibration API ----
  static void handleApiCalibration();
  static void handleApiCalibStart();
  static void handleApiCalibStop();
  static void handleApiCalibReset();

  // ---- Accords API ----
  static void handleApiAccords();
  static void handleApiAccordRefill();
  static void handleApiAccordSet();
  static void handleApiAccordCapacity();
  static void handleApiAccordName();

  // ---- Formula API ----
  static void handleApiFormulaStatus();
  static void handleApiFormulaSubmit();   // Calls existing FormulaManager

  // ---- Logs API ----
  static void handleApiLogs();
  static void handleApiLogsClear();

  // ---- Settings API ----
  static void handleApiSettings();
  static void handleApiSettingsPumps();
  static void handleApiSettingsCalibration();
  static void handleApiSettingsAccords();
  static void handleApiSettingsSecurity();
  static void handleApiSettingsNetwork();

  // ---- Wi-Fi scan ----
  static void handleApiWifiScan();
  static void handleApiWifiConnect();
  static void handleApiWifiSaved();
  static void handleApiWifiForget();

  // ---- Helpers ----
  static void sendCors();
  static void sendJson(int code, const String& json);
  static void sendError(int code, const String& message);
  static void sendOk(const String& message = "ok");
  static String getBodyArg();
  static bool parseJsonBody(const String& body, const String& key, String& out);
  static bool parseJsonBodyFloat(const String& body, const String& key, float& out);
  static bool parseJsonBodyInt(const String& body, const String& key, int& out);
};

#endif // WEBSERVER_MANAGER_H
