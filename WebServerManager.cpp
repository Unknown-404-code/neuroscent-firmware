#include "WebServerManager.h"
#include "AuthManager.h"
#include "FormulaManager.h"
#include "LogManager.h"
#include "OperationState.h"
#include "PumpManager.h"
#include "StorageManager.h"
#include "UIManager.h"
#include "WiFiProvisioning.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

// --------------------------------------------------------
// Static Members
// --------------------------------------------------------
WebServer WebServerManager::server(HTTP_PORT);
int8_t WebServerManager::webPumpIndex     = -1;
unsigned long WebServerManager::webOpStartTime = 0;
bool WebServerManager::webPurging         = false;
bool WebServerManager::webCalibrating     = false;
unsigned long WebServerManager::webTestMs = 0;

// --------------------------------------------------------
// Init
// --------------------------------------------------------
void WebServerManager::init() {
  // Mount LittleFS
  if (!LittleFS.begin(false)) {
    Serial.println("[Web] LittleFS mount failed — trying to format...");
    if (!LittleFS.begin(true)) {
      Serial.println("[Web] LittleFS format failed! Static files unavailable.");
    }
  } else {
    Serial.println("[Web] LittleFS mounted OK.");
  }

  // Wi-Fi is managed by WiFiProvisioning (not here)
  // Register headers for auth
  const char* headers[] = {"Cookie", "Authorization", "Content-Type"};
  server.collectHeaders(headers, 3);

  // ---- Static files ----
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleRoot);  // Redirect all SPA routes to index.html

  // ---- Legacy API (preserved — used by existing AI backend) ----
  server.on("/dispense", HTTP_POST, handleDispense);

  // ---- Auth API ----
  server.on("/api/auth/login",  HTTP_POST, handleAuthLogin);
  server.on("/api/auth/logout", HTTP_POST, handleAuthLogout);
  server.on("/api/auth/status", HTTP_GET,  handleAuthStatus);

  // ---- Status ----
  server.on("/api/status",  HTTP_GET, handleApiStatus);
  server.on("/api/network", HTTP_GET, handleApiNetwork);
  server.on("/api/device",  HTTP_GET, handleApiDevice);

  // ---- Pumps ----
  server.on("/api/pumps",              HTTP_GET,  handleApiPumps);
  server.on("/api/pumps/purge/start",  HTTP_POST, handleApiPurgeStart);
  server.on("/api/pumps/purge/stop",   HTTP_POST, handleApiPurgeStop);
  server.on("/api/pumps/test",         HTTP_POST, handleApiPumpTest);

  // ---- Calibration ----
  server.on("/api/calibration",        HTTP_GET,  handleApiCalibration);
  server.on("/api/calibration/start",  HTTP_POST, handleApiCalibStart);
  server.on("/api/calibration/stop",   HTTP_POST, handleApiCalibStop);
  server.on("/api/calibration/reset",  HTTP_POST, handleApiCalibReset);

  // ---- Accords ----
  server.on("/api/accords",              HTTP_GET,  handleApiAccords);
  server.on("/api/accords/refill",       HTTP_POST, handleApiAccordRefill);
  server.on("/api/accords/set",          HTTP_POST, handleApiAccordSet);
  server.on("/api/accords/capacity",     HTTP_POST, handleApiAccordCapacity);
  server.on("/api/accords/name",         HTTP_POST, handleApiAccordName);

  // ---- Formula ----
  server.on("/api/formula/status", HTTP_GET,  handleApiFormulaStatus);
  server.on("/api/formula/submit", HTTP_POST, handleApiFormulaSubmit);

  // ---- Logs ----
  server.on("/api/logs",       HTTP_GET,    handleApiLogs);
  server.on("/api/logs/clear", HTTP_DELETE, handleApiLogsClear);

  // ---- Settings ----
  server.on("/api/settings",               HTTP_GET,  handleApiSettings);
  server.on("/api/settings/pumps",         HTTP_POST, handleApiSettingsPumps);
  server.on("/api/settings/calibration",   HTTP_POST, handleApiSettingsCalibration);
  server.on("/api/settings/accords",       HTTP_POST, handleApiSettingsAccords);
  server.on("/api/settings/security",      HTTP_POST, handleApiSettingsSecurity);
  server.on("/api/settings/network",       HTTP_POST, handleApiSettingsNetwork);

  // ---- Wi-Fi ----
  server.on("/api/wifi/scan",    HTTP_GET,  handleApiWifiScan);
  server.on("/api/wifi/connect", HTTP_POST, handleApiWifiConnect);
  server.on("/api/wifi/saved",   HTTP_GET,  handleApiWifiSaved);
  server.on("/api/wifi/forget",  HTTP_POST, handleApiWifiForget);

  // ---- 404 handler serves SPA ----
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[Web] HTTP server started on port " + String(HTTP_PORT));
}

// --------------------------------------------------------
// Update (called from loop)
// --------------------------------------------------------
void WebServerManager::update() {
  server.handleClient();

  // Timed web pump test finished
  if (webPurging && webTestMs > 0 && millis() - webOpStartTime >= webTestMs) {
    int8_t pump = webPumpIndex;
    stopWebPurge();
    LogManager::log(LogEvent::PUMP_TEST_COMPLETE, LogSeverity::INFO,
                    "Web pump test complete", pump);
  }

  // Safety timeout for web purge
  if (webPurging && webPumpIndex >= 0) {
    unsigned long maxMs = (unsigned long)StorageManager::getMaxPurgeDurationS() * 1000UL;
    if (millis() - webOpStartTime >= maxMs) {
      Serial.println("[Web] Purge timeout — stopping pump " + String(webPumpIndex + 1));
      stopWebPurge();
      LogManager::log(LogEvent::PUMP_PURGE_STOP, LogSeverity::WARN,
                      "Purge auto-stopped (timeout)", webPumpIndex);
    }
  }

  // Safety timeout for web calibration
  if (webCalibrating && webPumpIndex >= 0) {
    unsigned long maxMs = (unsigned long)StorageManager::getCalibTimeoutS() * 1000UL;
    if (millis() - webOpStartTime >= maxMs) {
      Serial.println("[Web] Calibration timeout — stopping pump " + String(webPumpIndex + 1));
      int8_t pump = webPumpIndex;
      stopWebCalib();
      LogManager::log(LogEvent::CALIB_UPDATED, LogSeverity::WARN,
                      "Calibration timed out (not saved)", pump);
    }
  }
}

// --------------------------------------------------------
// State accessors
// --------------------------------------------------------
bool WebServerManager::isConnected()     { return WiFiProvisioning::isStaConnected(); }
bool WebServerManager::isWebPurging()    { return webPurging; }
bool WebServerManager::isWebCalibrating(){ return webCalibrating; }

void WebServerManager::stopWebPurge() {
  if (webPumpIndex >= 0) {
    PumpManager::stopPump(webPumpIndex);
    // Pump tests track liquid used (capped at the target); plain purges don't
    if (webTestMs > 0) {
      unsigned long ranMs = min(millis() - webOpStartTime, webTestMs);
      StorageManager::dispenseFromContainer(
          webPumpIndex, ranMs * StorageManager::getCalibration(webPumpIndex));
    }
  }
  webTestMs    = 0;
  webPurging   = false;
  webPumpIndex = -1;
  OperationState::release();
}

void WebServerManager::stopWebCalib() {
  if (webPumpIndex >= 0) PumpManager::stopPump(webPumpIndex);
  webCalibrating = false;
  webPumpIndex   = -1;
  OperationState::release();
}

// --------------------------------------------------------
// Helpers
// --------------------------------------------------------
void WebServerManager::sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type,Authorization,Cookie");
}

void WebServerManager::sendJson(int code, const String& json) {
  sendCors();
  server.send(code, "application/json", json);
}

void WebServerManager::sendError(int code, const String& message) {
  sendJson(code, "{\"status\":\"error\",\"message\":\"" + LogManager::jsonEscape(message) + "\"}");
}

void WebServerManager::sendOk(const String& message) {
  sendJson(200, "{\"status\":\"ok\",\"message\":\"" + LogManager::jsonEscape(message) + "\"}");
}

String WebServerManager::getBodyArg() {
  if (server.hasArg("plain")) return server.arg("plain");
  return "";
}

// Simple JSON key extraction (avoids heap allocation for small payloads)
bool WebServerManager::parseJsonBody(const String& body, const String& key, String& out) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  if (!doc.containsKey(key)) return false;
  out = doc[key].as<String>();
  return true;
}

bool WebServerManager::parseJsonBodyFloat(const String& body, const String& key, float& out) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  if (!doc.containsKey(key)) return false;
  out = doc[key].as<float>();
  return true;
}

bool WebServerManager::parseJsonBodyInt(const String& body, const String& key, int& out) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  if (!doc.containsKey(key)) return false;
  out = doc[key].as<int>();
  return true;
}

// --------------------------------------------------------
// Static file serving (LittleFS)
// --------------------------------------------------------
String WebServerManager::getMimeType(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg"))  return "image/jpeg";
  if (path.endsWith(".gz"))   return "application/gzip";
  return "text/plain";
}

bool WebServerManager::serveFromLittleFS(const String& path) {
  // Try gzipped version first
  String gzPath = path + ".gz";
  if (LittleFS.exists(gzPath)) {
    File f = LittleFS.open(gzPath, "r");
    if (f) {
      server.sendHeader("Content-Encoding", "gzip");
      // no-cache: browsers re-check every load, so a new data/ upload shows up immediately
      server.sendHeader("Cache-Control", "no-cache");
      server.streamFile(f, getMimeType(path));
      f.close();
      return true;
    }
  }
  if (LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    if (f) {
      server.sendHeader("Cache-Control", "no-cache");
      server.streamFile(f, getMimeType(path));
      f.close();
      return true;
    }
  }
  return false;
}

void WebServerManager::handleRoot() {
  // Determine which page: AP mode → setup.html, else index.html
  if (WiFiProvisioning::isApActive()) {
    if (!serveFromLittleFS("/setup.html")) {
      server.send(200, "text/html",
                  "<h1>NeuroScent Setup</h1>"
                  "<p>Please upload the web UI files to LittleFS.</p>"
                  "<p>Temporary setup: POST to /api/wifi/connect with {\"ssid\":\"...\",\"password\":\"...\"}</p>");
    }
  } else {
    if (!serveFromLittleFS("/index.html")) {
      server.send(200, "text/html",
                  "<h1>NeuroScent Dashboard</h1>"
                  "<p>Web UI files not found. Please upload the LittleFS data directory.</p>"
                  "<p>API available at /api/status</p>");
    }
  }
}

void WebServerManager::handleStaticFile() {
  if (!serveFromLittleFS(server.uri())) {
    handleNotFound();
  }
}

void WebServerManager::handleNotFound() {
  String uri = server.uri();

  // Never redirect unknown API requests to HTML.
  if (uri.startsWith("/api/") || uri.startsWith("/dispense")) {
    sendError(404, "Not found");
    return;
  }

  // First, try serving the actual requested file from LittleFS.
  if (serveFromLittleFS(uri)) {
    return;
  }

  // Only fall back to index.html for browser navigation routes.
  // Missing CSS, JS, images, etc. must return 404.
  // In setup (AP) mode this also makes captive-portal probes land on setup.html.
  if (server.method() == HTTP_GET && uri.indexOf('.') == -1) {
    handleRoot();
    return;
  }

  sendError(404, "Not found");
}

// --------------------------------------------------------
// Legacy /dispense endpoint (preserved for AI backend)
// --------------------------------------------------------
void WebServerManager::handleDispense() {
  if (UIManager::getState() != UIState::MAIN_MENU || !OperationState::isIdle()) {
    sendError(503, "System is busy");
    return;
  }

  String jsonStr = getBodyArg();
  if (jsonStr.isEmpty()) {
    sendError(400, "Body not found");
    return;
  }

  Serial.println("[Web] /dispense payload: " + jsonStr);
  bool parsed = FormulaManager::parseAndPrepare(jsonStr);

  if (parsed) {
    LogManager::log(LogEvent::FORMULA_RECEIVED, LogSeverity::INFO,
                    "Formula received via /dispense");
    sendJson(200, "{\"status\":\"ok\",\"message\":\"Formula received\"}");
  } else {
    sendError(400, "Invalid JSON or validation failed");
  }
}

// --------------------------------------------------------
// Auth API
// --------------------------------------------------------
void WebServerManager::handleAuthLogin() {
  String body = getBodyArg();
  String password, clientIp;
  clientIp = server.client().remoteIP().toString();

  if (!AuthManager::checkRateLimit(clientIp)) {
    sendError(429, "Too many login attempts. Try again later.");
    return;
  }

  if (!parseJsonBody(body, "password", password)) {
    sendError(400, "Missing password field");
    return;
  }

  if (!AuthManager::checkPassword(password)) {
    AuthManager::recordFailedLogin(clientIp);
    sendError(401, "Invalid password");
    return;
  }

  AuthManager::clearRateLimit(clientIp);
  String token = AuthManager::createSession();
  LogManager::log(LogEvent::ADMIN_LOGIN, LogSeverity::INFO, "Admin logged in");

  // Set session cookie
  server.sendHeader("Set-Cookie",
    "ns_session=" + token + "; Path=/; HttpOnly; SameSite=Strict");
  sendJson(200, "{\"status\":\"ok\",\"token\":\"" + token + "\",\"role\":\"admin\"}");
}

void WebServerManager::handleAuthLogout() {
  String token = AuthManager::getTokenFromRequest(server);
  if (!token.isEmpty()) {
    AuthManager::invalidateSession(token);
    LogManager::log(LogEvent::ADMIN_LOGOUT, LogSeverity::INFO, "Admin logged out");
  }
  server.sendHeader("Set-Cookie",
    "ns_session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  sendOk("Logged out");
}

void WebServerManager::handleAuthStatus() {
  AuthRole role = AuthManager::getRoleFromRequest(server);
  String r = (role == AuthRole::ADMIN) ? "admin" : "guest";
  sendJson(200, "{\"status\":\"ok\",\"role\":\"" + r + "\"}");
}

// --------------------------------------------------------
// Status API
// --------------------------------------------------------
void WebServerManager::handleApiStatus() {
  UIState uiState = UIManager::getState();
  SystemOperation sysOp = OperationState::getCurrent();

  String stateStr;
  switch (uiState) {
    case UIState::MAIN_MENU:       stateStr = "idle"; break;
    case UIState::PURGE_RUNNING:   stateStr = "purging"; break;
    case UIState::CALIBRATION_RUNNING: stateStr = "calibrating"; break;
    case UIState::TEST_RUNNING:    stateStr = "testing"; break;
    case UIState::FORMULA_CONFIRM: stateStr = "formula_pending"; break;
    case UIState::FORMULA_RUNNING: stateStr = "dispensing"; break;
    case UIState::FORMULA_ERROR:   stateStr = "error"; break;
    default:                       stateStr = "busy"; break;
  }

  if (webPurging)     stateStr = "web_purging";
  if (webCalibrating) stateStr = "web_calibrating";

  unsigned long upMs = millis();
  char json[512];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\","
    "\"state\":\"%s\","
    "\"firmware\":\"%s\","
    "\"uptime_ms\":%lu,"
    "\"uptime_s\":%lu,"
    "\"running_pump\":%d,"
    "\"any_pump_running\":%s}",
    stateStr.c_str(),
    FIRMWARE_VERSION,
    upMs,
    upMs / 1000UL,
    PumpManager::getRunningPump(),
    PumpManager::isAnyPumpRunning() ? "true" : "false"
  );
  sendJson(200, json);
}

void WebServerManager::handleApiNetwork() {
  WiFiState wfState = WiFiProvisioning::getState();
  String stateStr;
  switch (wfState) {
    case WiFiState::STA_CONNECTED: stateStr = "connected"; break;
    case WiFiState::CONNECTING:    stateStr = "connecting"; break;
    case WiFiState::AP_ACTIVE:     stateStr = "ap_mode"; break;
    case WiFiState::RECONNECTING:  stateStr = "reconnecting"; break;
    default:                       stateStr = "disconnected"; break;
  }

  char json[512];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\","
    "\"wifi_state\":\"%s\","
    "\"ssid\":\"%s\","
    "\"ip\":\"%s\","
    "\"mdns\":\"%s\","
    "\"rssi\":%d,"
    "\"uptime_s\":%lu}",
    stateStr.c_str(),
    LogManager::jsonEscape(WiFiProvisioning::getSSID()).c_str(),
    WiFiProvisioning::getLocalIP().c_str(),
    WiFiProvisioning::getMdnsHostname().c_str(),
    WiFiProvisioning::getRssi(),
    WiFiProvisioning::getConnectedDurationMs() / 1000UL
  );
  sendJson(200, json);
}

void WebServerManager::handleApiDevice() {
  char json[512];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\","
    "\"device\":\"NeuroScent\","
    "\"chip\":\"ESP32-S3\","
    "\"firmware\":\"%s\","
    "\"flash_mb\":16,"
    "\"free_heap\":%lu,"
    "\"psram_free\":%lu,"
    "\"uptime_s\":%lu}",
    FIRMWARE_VERSION,
    (unsigned long)ESP.getFreeHeap(),
    (unsigned long)ESP.getFreePsram(),
    millis() / 1000UL
  );
  sendJson(200, json);
}

// --------------------------------------------------------
// Pumps API
// --------------------------------------------------------
void WebServerManager::handleApiPumps() {
  String json = "{\"status\":\"ok\",\"pumps\":[";
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    char buf[256];
    float rem   = StorageManager::getRemainingMl(i);
    float cap   = StorageManager::getCapacityMl(i);
    float pct   = (cap > 0) ? (rem / cap * 100.0f) : 0.0f;
    bool  low   = pct <= StorageManager::getLowLiquidWarnPct();
    String name = StorageManager::getAccordName(i);

    snprintf(buf, sizeof(buf),
      "{\"pump\":%d,\"name\":\"%s\",\"running\":%s,"
      "\"remaining_ml\":%.2f,\"capacity_ml\":%.2f,\"percent\":%.1f,\"low_liquid\":%s}",
      i + 1, LogManager::jsonEscape(name).c_str(),
      (PumpManager::getRunningPump() == i) ? "true" : "false",
      rem, cap, pct, low ? "true" : "false"
    );
    if (i > 0) json += ",";
    json += buf;
  }
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiPurgeStart() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS) {
    sendError(400, "Invalid pump number (1-8)");
    return;
  }
  uint8_t pumpIdx = pump - 1;

  // Check system state
  if (!OperationState::isIdle()) {
    sendError(503, "System is busy");
    return;
  }
  if (UIManager::getState() != UIState::MAIN_MENU) {
    sendError(503, "OLED operation in progress");
    return;
  }
  if (!OperationState::acquire(SystemOperation::WEB_PURGING)) {
    sendError(503, "Could not acquire operation slot");
    return;
  }

  PumpManager::startPump(pumpIdx);
  webPumpIndex   = pumpIdx;
  webOpStartTime = millis();
  webPurging     = true;

  LogManager::log(LogEvent::PUMP_PURGE_START, LogSeverity::INFO,
                  "Web purge started", pumpIdx);
  sendOk("Purge started for pump " + String(pump));
}

void WebServerManager::handleApiPurgeStop() {
  if (!AuthManager::requireAdmin(server)) return;

  if (!webPurging) {
    sendError(400, "No purge is running");
    return;
  }

  int8_t stoppedPump = webPumpIndex;
  stopWebPurge();

  LogManager::log(LogEvent::PUMP_PURGE_STOP, LogSeverity::INFO,
                  "Web purge stopped", stoppedPump);
  sendOk("Purge stopped");
}

void WebServerManager::handleApiPumpTest() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0, containerMl = 0, percent = 0;

  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS ||
      !parseJsonBodyInt(body, "container_ml", containerMl) || containerMl < 1 ||
      !parseJsonBodyInt(body, "percent", percent) || percent < 1 || percent > 100) {
    sendError(400, "Invalid parameters: pump (1-8), container_ml, percent (1-100)");
    return;
  }

  if (!OperationState::isIdle() || UIManager::getState() != UIState::MAIN_MENU) {
    sendError(503, "System is busy");
    return;
  }

  uint8_t pumpIdx = pump - 1;
  float reqMl     = (float)containerMl * ((float)percent / 100.0f);
  float mlPerMs   = StorageManager::getCalibration(pumpIdx);

  if (mlPerMs <= 0.0f) {
    sendError(500, "Invalid calibration value");
    return;
  }
  if (StorageManager::getRemainingMl(pumpIdx) < reqMl) {
    sendError(400, "Insufficient liquid in container");
    return;
  }

  unsigned long durationMs = (unsigned long)(reqMl / mlPerMs);

  // For web test, we use web purge slot with a timed auto-stop
  if (!OperationState::acquire(SystemOperation::WEB_PURGING)) {
    sendError(503, "Could not acquire operation slot");
    return;
  }

  PumpManager::startPump(pumpIdx);
  webPumpIndex   = pumpIdx;
  webOpStartTime = millis();
  webPurging     = true;
  webTestMs      = durationMs;  // update() stops the pump when this elapses

  LogManager::log(LogEvent::PUMP_TEST_START, LogSeverity::INFO,
                  "Web pump test started", pumpIdx);

  char resp[256];
  snprintf(resp, sizeof(resp),
    "{\"status\":\"ok\",\"message\":\"Test started\","
    "\"pump\":%d,\"required_ml\":%.2f,\"duration_ms\":%lu}",
    pump, reqMl, durationMs);
  sendJson(200, resp);
}

// --------------------------------------------------------
// Calibration API
// --------------------------------------------------------
void WebServerManager::handleApiCalibration() {
  String json = "{\"status\":\"ok\","
                "\"target_ml\":" + String(StorageManager::getCalibTargetMl()) +
                ",\"calibrations\":[";
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    float mlPerMs = StorageManager::getCalibration(i);
    char buf[128];
    snprintf(buf, sizeof(buf),
      "{\"pump\":%d,\"ml_per_ms\":%.6f,\"ml_per_s\":%.4f}",
      i + 1, mlPerMs, mlPerMs * 1000.0f);
    if (i > 0) json += ",";
    json += buf;
  }
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiCalibStart() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS) {
    sendError(400, "Invalid pump number (1-8)");
    return;
  }
  uint8_t pumpIdx = pump - 1;

  if (!OperationState::isIdle() || UIManager::getState() != UIState::MAIN_MENU) {
    sendError(503, "System is busy");
    return;
  }
  if (!OperationState::acquire(SystemOperation::WEB_CALIBRATING)) {
    sendError(503, "Could not acquire operation slot");
    return;
  }

  PumpManager::startPump(pumpIdx);
  webPumpIndex   = pumpIdx;
  webOpStartTime = millis();
  webCalibrating = true;

  sendOk("Calibration started for pump " + String(pump));
}

void WebServerManager::handleApiCalibStop() {
  if (!AuthManager::requireAdmin(server)) return;

  if (!webCalibrating || webPumpIndex < 0) {
    sendError(400, "No calibration is running");
    return;
  }

  unsigned long elapsed = millis() - webOpStartTime;
  int8_t pumpIdx = webPumpIndex;

  PumpManager::stopPump(pumpIdx);
  webCalibrating = false;
  webPumpIndex   = -1;
  OperationState::release();

  if (elapsed < 100) {
    sendError(400, "Elapsed time too short for valid calibration");
    return;
  }

  float targetMl = (float)StorageManager::getCalibTargetMl();
  float mlPerMs  = targetMl / (float)elapsed;

  // Sanity check: must be in range 0.0001 to 1.0 ml/ms
  if (mlPerMs < 0.0001f || mlPerMs > 1.0f) {
    sendError(400, "Calibration result out of valid range. Check pump and target volume.");
    return;
  }

  StorageManager::setCalibration(pumpIdx, mlPerMs);

  char msg[64];
  snprintf(msg, sizeof(msg), "Pump %d calibrated", pumpIdx + 1);
  LogManager::log(LogEvent::CALIB_UPDATED, LogSeverity::INFO, msg, pumpIdx);

  char json[256];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\",\"pump\":%d,"
    "\"elapsed_ms\":%lu,\"target_ml\":%.1f,"
    "\"ml_per_ms\":%.6f,\"ml_per_s\":%.4f}",
    pumpIdx + 1, elapsed, targetMl, mlPerMs, mlPerMs * 1000.0f);
  sendJson(200, json);
}

void WebServerManager::handleApiCalibReset() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS) {
    sendError(400, "Invalid pump number (1-8)");
    return;
  }
  uint8_t pumpIdx = pump - 1;

  StorageManager::resetCalibration(pumpIdx);
  char msg[64];
  snprintf(msg, sizeof(msg), "Pump %d calibration reset", pumpIdx + 1);
  LogManager::log(LogEvent::CALIB_RESET, LogSeverity::INFO, msg, pumpIdx);
  sendOk(msg);
}

// --------------------------------------------------------
// Accords API
// --------------------------------------------------------
void WebServerManager::handleApiAccords() {
  uint8_t warnPct = StorageManager::getLowLiquidWarnPct();
  String json = "{\"status\":\"ok\",\"warn_pct\":" + String(warnPct) + ",\"accords\":[";
  for (uint8_t i = 0; i < NUM_PUMPS; i++) {
    float rem  = StorageManager::getRemainingMl(i);
    float cap  = StorageManager::getCapacityMl(i);
    float pct  = (cap > 0) ? (rem / cap * 100.0f) : 0.0f;
    bool  low  = pct <= warnPct;
    String name = StorageManager::getAccordName(i);

    char buf[256];
    snprintf(buf, sizeof(buf),
      "{\"pump\":%d,\"name\":\"%s\",\"remaining_ml\":%.2f,"
      "\"capacity_ml\":%.2f,\"percent\":%.1f,\"low_liquid\":%s}",
      i + 1, LogManager::jsonEscape(name).c_str(), rem, cap, pct, low ? "true" : "false");
    if (i > 0) json += ",";
    json += buf;
  }
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiAccordRefill() {
  if (!AuthManager::requireAdmin(server)) return;
  if (!OperationState::isIdle()) { sendError(503, "System is busy"); return; }

  String body = getBodyArg();
  int pump = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS) {
    sendError(400, "Invalid pump number"); return;
  }
  uint8_t pumpIdx = pump - 1;
  StorageManager::resetContainer(pumpIdx);
  char msg[64];
  snprintf(msg, sizeof(msg), "Pump %d refilled to %.0f mL", pumpIdx + 1,
           StorageManager::getCapacityMl(pumpIdx));
  LogManager::log(LogEvent::CONTAINER_REFILLED, LogSeverity::INFO, msg, pumpIdx);
  sendOk(msg);
}

void WebServerManager::handleApiAccordSet() {
  if (!AuthManager::requireAdmin(server)) return;
  if (!OperationState::isIdle()) { sendError(503, "System is busy"); return; }

  String body = getBodyArg();
  int pump = 0;
  float ml = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS ||
      !parseJsonBodyFloat(body, "remaining_ml", ml) || ml < 0) {
    sendError(400, "Invalid parameters"); return;
  }
  uint8_t pumpIdx = pump - 1;
  float cap = StorageManager::getCapacityMl(pumpIdx);
  if (ml > cap) {
    sendError(400, "remaining_ml exceeds capacity"); return;
  }
  StorageManager::setRemainingMl(pumpIdx, ml);
  char msg[64];
  snprintf(msg, sizeof(msg), "Pump %d set to %.1f mL", pumpIdx + 1, ml);
  LogManager::log(LogEvent::CONTAINER_ADJUSTED, LogSeverity::INFO, msg, pumpIdx);
  sendOk(msg);
}

void WebServerManager::handleApiAccordCapacity() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0;
  float cap = 0;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS ||
      !parseJsonBodyFloat(body, "capacity_ml", cap) || cap < 1) {
    sendError(400, "Invalid parameters"); return;
  }
  uint8_t pumpIdx = pump - 1;
  float remaining = StorageManager::getRemainingMl(pumpIdx);
  if (remaining > cap) {
    sendError(400, "New capacity is less than current remaining level. Adjust remaining first.");
    return;
  }
  StorageManager::setCapacityMl(pumpIdx, cap);
  char msg[64];
  snprintf(msg, sizeof(msg), "Pump %d capacity set to %.0f mL", pumpIdx + 1, cap);
  LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO, msg, pumpIdx);
  sendOk(msg);
}

void WebServerManager::handleApiAccordName() {
  if (!AuthManager::requireAdmin(server)) return;

  String body = getBodyArg();
  int pump = 0;
  String name;
  if (!parseJsonBodyInt(body, "pump", pump) || pump < 1 || pump > NUM_PUMPS ||
      !parseJsonBody(body, "name", name)) {
    sendError(400, "Invalid parameters"); return;
  }
  StorageManager::setAccordName(pump - 1, name);
  sendOk("Accord name updated");
}

// --------------------------------------------------------
// Formula API
// --------------------------------------------------------
void WebServerManager::handleApiFormulaStatus() {
  UIState s = UIManager::getState();
  String stateStr;
  switch (s) {
    case UIState::FORMULA_CONFIRM:      stateStr = "awaiting_confirmation"; break;
    case UIState::FORMULA_RUNNING:      stateStr = "dispensing"; break;
    case UIState::FORMULA_STOP_CONFIRM: stateStr = "dispensing"; break;
    case UIState::FORMULA_FINISHED:     stateStr = "completed"; break;
    case UIState::FORMULA_ERROR:        stateStr = "error"; break;
    default:                            stateStr = "idle"; break;
  }

  uint8_t prog = FormulaManager::getCurrentStepProgress();
  char json[256];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\",\"formula_state\":\"%s\",\"step_progress\":%d,"
    "\"running_pump\":%d}",
    stateStr.c_str(), prog, PumpManager::getRunningPump());
  sendJson(200, json);
}

void WebServerManager::handleApiFormulaSubmit() {
  if (!AuthManager::requireAdmin(server)) return;

  // Check busy state (same as /dispense)
  if (UIManager::getState() != UIState::MAIN_MENU || !OperationState::isIdle()) {
    sendError(503, "System is busy — formula requires OLED confirmation, try again when idle");
    return;
  }

  String jsonStr = getBodyArg();
  if (jsonStr.isEmpty()) {
    sendError(400, "Request body is empty"); return;
  }

  bool parsed = FormulaManager::parseAndPrepare(jsonStr);
  if (parsed) {
    LogManager::log(LogEvent::FORMULA_RECEIVED, LogSeverity::INFO,
                    "Formula received via web UI");
    sendJson(200,
      "{\"status\":\"ok\","
      "\"message\":\"Formula queued — confirm on the NeuroScent device\","
      "\"awaiting_confirmation\":true}");
  } else {
    sendError(400, "Invalid formula or validation failed (check pump levels and JSON)");
  }
}

// --------------------------------------------------------
// Logs API
// --------------------------------------------------------
void WebServerManager::handleApiLogs() {
  // Guests cannot view logs
  if (AuthManager::getRoleFromRequest(server) != AuthRole::ADMIN) {
    sendError(401, "Authentication required");
    return;
  }

  uint16_t total = LogManager::getCount();
  String json = "{\"status\":\"ok\",\"total\":" + String(total) + ",\"logs\":[";

  // Return the most recent entries (?limit=N, default and max 100)
  uint16_t limit = min(total, (uint16_t)100);
  if (server.hasArg("limit")) limit = min(limit, (uint16_t)constrain(server.arg("limit").toInt(), 1, 100));
  for (uint16_t i = 0; i < limit; i++) {
    LogEntry e;
    if (LogManager::getEntry(i, e)) {
      if (i > 0) json += ",";
      json += LogManager::entryToJson(e);
    }
  }
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiLogsClear() {
  if (!AuthManager::requireAdmin(server)) return;
  LogManager::clear();
  LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO, "Logs cleared");
  sendOk("Logs cleared");
}

// --------------------------------------------------------
// Settings API
// --------------------------------------------------------
void WebServerManager::handleApiSettings() {
  char json[512];
  snprintf(json, sizeof(json),
    "{\"status\":\"ok\","
    "\"max_purge_s\":%d,"
    "\"calib_target_ml\":%d,"
    "\"calib_timeout_s\":%d,"
    "\"low_liquid_warn_pct\":%d,"
    "\"pwm_duty\":%d}",
    StorageManager::getMaxPurgeDurationS(),
    StorageManager::getCalibTargetMl(),
    StorageManager::getCalibTimeoutS(),
    StorageManager::getLowLiquidWarnPct(),
    StorageManager::getPwmResolution()
  );
  sendJson(200, json);
}

void WebServerManager::handleApiSettingsPumps() {
  if (!AuthManager::requireAdmin(server)) return;
  String body = getBodyArg();
  int val = 0;
  if (parseJsonBodyInt(body, "max_purge_s", val)) {
    StorageManager::setMaxPurgeDurationS(val);
    LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO,
                    "Max purge duration updated");
  }
  sendOk("Pump settings updated");
}

void WebServerManager::handleApiSettingsCalibration() {
  if (!AuthManager::requireAdmin(server)) return;
  String body = getBodyArg();
  int val = 0;
  bool changed = false;
  if (parseJsonBodyInt(body, "calib_target_ml", val)) {
    StorageManager::setCalibTargetMl(val);
    changed = true;
  }
  if (parseJsonBodyInt(body, "calib_timeout_s", val)) {
    StorageManager::setCalibTimeoutS(val);
    changed = true;
  }
  if (changed) LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO,
                               "Calibration settings updated");
  sendOk("Calibration settings updated");
}

void WebServerManager::handleApiSettingsAccords() {
  if (!AuthManager::requireAdmin(server)) return;
  String body = getBodyArg();
  int val = 0;
  if (parseJsonBodyInt(body, "low_liquid_warn_pct", val)) {
    StorageManager::setLowLiquidWarnPct(val);
    LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO,
                    "Low liquid threshold updated");
  }
  sendOk("Accord settings updated");
}

void WebServerManager::handleApiSettingsSecurity() {
  if (!AuthManager::requireAdmin(server)) return;
  String body = getBodyArg();
  String newPass, curPass;
  if (!parseJsonBody(body, "new_password", newPass) || newPass.length() < 6) {
    sendError(400, "new_password must be at least 6 characters"); return;
  }
  parseJsonBody(body, "current_password", curPass);  // Optional if first time

  if (!AuthManager::setPassword(newPass, curPass)) {
    sendError(401, "Current password incorrect or new password invalid"); return;
  }
  LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO, "Admin password changed");
  sendOk("Password updated. All sessions invalidated.");
}

void WebServerManager::handleApiSettingsNetwork() {
  if (!AuthManager::requireAdmin(server)) return;
  // Network changes go through /api/wifi/connect
  sendError(400, "Use /api/wifi/connect to change network credentials");
}

// --------------------------------------------------------
// Wi-Fi scan/connect
// --------------------------------------------------------
void WebServerManager::handleApiWifiScan() {
  // The scan blocks the loop for a few seconds, which would stretch pump timing
  if (PumpManager::isAnyPumpRunning()) {
    sendError(503, "Can't scan while a pump is running");
    return;
  }
  int n = WiFi.scanNetworks(false, false);  // blocking scan
  String json = "{\"status\":\"ok\",\"networks\":[";
  for (int i = 0; i < n; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
      "{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
      LogManager::jsonEscape(WiFi.SSID(i)).c_str(), WiFi.RSSI(i),
      (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false");
    if (i > 0) json += ",";
    json += buf;
  }
  WiFi.scanDelete();
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiWifiConnect() {
  // The setup page runs before any login exists; the AP's own password guards it.
  if (!WiFiProvisioning::isApActive() && !AuthManager::requireAdmin(server)) return;

  // Wi-Fi changes not allowed during dispensing
  if (OperationState::isDispensingOrPending()) {
    sendError(503, "Cannot change Wi-Fi during active dispensing"); return;
  }

  String body = getBodyArg();
  String ssid, password;
  if (!parseJsonBody(body, "ssid", ssid) || ssid.isEmpty()) {
    sendError(400, "SSID is required"); return;
  }
  String useSaved;
  if (parseJsonBody(body, "saved", useSaved) && useSaved == "true") {
    // Switching to a remembered network: the browser never sees the password
    if (!WiFiProvisioning::getSavedPassword(ssid, password)) {
      sendError(404, "Network is not saved"); return;
    }
  } else {
    parseJsonBody(body, "password", password);  // Can be empty for open networks
  }

  bool ok = WiFiProvisioning::attemptConnect(ssid, password);
  if (ok) {
    LogManager::log(LogEvent::WIFI_CONNECTED, LogSeverity::INFO,
                    "Wi-Fi connection initiated: " + ssid);
    sendJson(200,
      "{\"status\":\"ok\",\"message\":\"Connecting to network. "
      "Check /api/network for status.\",\"ssid\":\"" + LogManager::jsonEscape(ssid) + "\"}");
  } else {
    sendError(400, "Failed to initiate connection");
  }
}

void WebServerManager::handleApiWifiSaved() {
  if (!AuthManager::requireAdmin(server)) return;
  String ssids[WiFiProvisioning::MAX_SAVED], passes[WiFiProvisioning::MAX_SAVED];
  uint8_t n = WiFiProvisioning::loadSavedNetworks(ssids, passes);
  String json = "{\"status\":\"ok\",\"networks\":[";
  for (uint8_t i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "\"" + LogManager::jsonEscape(ssids[i]) + "\"";
  }
  json += "]}";
  sendJson(200, json);
}

void WebServerManager::handleApiWifiForget() {
  if (!AuthManager::requireAdmin(server)) return;
  String ssid;
  if (!parseJsonBody(getBodyArg(), "ssid", ssid) || !WiFiProvisioning::forgetNetwork(ssid)) {
    sendError(404, "Network is not saved"); return;
  }
  LogManager::log(LogEvent::SETTINGS_CHANGED, LogSeverity::INFO, "Forgot Wi-Fi network: " + ssid);
  sendOk("Network forgotten");
}
