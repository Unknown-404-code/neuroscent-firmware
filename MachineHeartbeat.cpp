#include "MachineHeartbeat.h"

#include "Config.h"
#include "OperationState.h"
#include "PumpManager.h"
#include "WiFiProvisioning.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace {
constexpr char DEVICE_ID[] = "neuroscent-machine-01";
constexpr char HEARTBEAT_PATH[] = "/functions/v1/machine-heartbeat";
constexpr size_t ENDPOINT_CAPACITY = 241;
constexpr size_t TOKEN_LENGTH = 64;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;
constexpr uint32_t REQUEST_TIMEOUT_MS = 5000;

struct HeartbeatSnapshot {
  bool wifiConnected = false;
  bool machineBusy = false;
  bool pumpRunning = false;
  char endpoint[ENDPOINT_CAPACITY] = {};
  char token[TOKEN_LENGTH + 1] = {};
};

Preferences heartbeatPreferences;
HeartbeatSnapshot snapshot;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t heartbeatTaskHandle = nullptr;
char serialLine[320] = {};
size_t serialLineLength = 0;
bool serialLineOverflow = false;

bool validToken(const String& token) {
  if (token.length() != TOKEN_LENGTH) return false;
  for (size_t i = 0; i < token.length(); ++i) {
    const char c = token[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool validEndpoint(const String& endpoint) {
  if (!endpoint.startsWith("https://") || endpoint.length() >= ENDPOINT_CAPACITY
      || !endpoint.endsWith(HEARTBEAT_PATH) || endpoint.indexOf('?') >= 0
      || endpoint.indexOf('#') >= 0 || endpoint.indexOf('@') >= 0) return false;

  const int hostEnd = endpoint.indexOf('/', 8);
  if (hostEnd < 0) return false;
  const String host = endpoint.substring(8, hostEnd);
  return host.length() > 12 && host.endsWith(".supabase.co")
      && host.indexOf(':') < 0 && host.indexOf(' ') < 0
      && endpoint.substring(hostEnd) == HEARTBEAT_PATH;
}

void copySnapshot(HeartbeatSnapshot& target) {
  portENTER_CRITICAL(&snapshotMux);
  target = snapshot;
  portEXIT_CRITICAL(&snapshotMux);
}

void setCloudConfiguration(const String& endpoint, const String& token) {
  const size_t endpointBytes = heartbeatPreferences.putString("endpoint", endpoint);
  const size_t tokenBytes = heartbeatPreferences.putString("token", token);
  if (!endpointBytes || !tokenBytes) {
    heartbeatPreferences.remove("endpoint");
    heartbeatPreferences.remove("token");
    Serial.println("[Heartbeat] Could not store cloud settings; no credential was retained.");
    return;
  }

  char endpointCopy[ENDPOINT_CAPACITY] = {};
  char tokenCopy[TOKEN_LENGTH + 1] = {};
  endpoint.toCharArray(endpointCopy, sizeof(endpointCopy));
  token.toCharArray(tokenCopy, sizeof(tokenCopy));
  portENTER_CRITICAL(&snapshotMux);
  memcpy(snapshot.endpoint, endpointCopy, sizeof(snapshot.endpoint));
  memcpy(snapshot.token, tokenCopy, sizeof(snapshot.token));
  portEXIT_CRITICAL(&snapshotMux);
  memset(tokenCopy, 0, sizeof(tokenCopy));

  Serial.println("[Heartbeat] Cloud settings saved to device NVS; the credential was not echoed.");
  if (heartbeatTaskHandle) xTaskNotifyGive(heartbeatTaskHandle);
}

void clearCloudConfiguration() {
  heartbeatPreferences.remove("endpoint");
  heartbeatPreferences.remove("token");
  portENTER_CRITICAL(&snapshotMux);
  memset(snapshot.endpoint, 0, sizeof(snapshot.endpoint));
  memset(snapshot.token, 0, sizeof(snapshot.token));
  portEXIT_CRITICAL(&snapshotMux);
  Serial.println("[Heartbeat] Cloud settings removed from device NVS.");
}

void handleSerialCommand() {
  serialLine[serialLineLength] = '\0';
  const String command(serialLine);
  constexpr char CONFIGURE_PREFIX[] = "heartbeat configure ";

  if (command.startsWith(CONFIGURE_PREFIX)) {
    const int endpointStart = sizeof(CONFIGURE_PREFIX) - 1;
    const int separator = command.indexOf(' ', endpointStart);
    if (separator < 0) {
      Serial.println("[Heartbeat] Configure requires an HTTPS Supabase function URL and a 64-character hex token.");
    } else {
      const String endpoint = command.substring(endpointStart, separator);
      String token = command.substring(separator + 1);
      if (!validEndpoint(endpoint) || !validToken(token)) {
        Serial.println("[Heartbeat] Invalid URL or credential; nothing was saved.");
      } else {
        setCloudConfiguration(endpoint, token);
      }
      token = "";
    }
  } else if (command == "heartbeat clear") {
    clearCloudConfiguration();
  } else if (command == "heartbeat status") {
    HeartbeatSnapshot current;
    copySnapshot(current);
    Serial.printf("[Heartbeat] Endpoint and credential configured: %s\n",
                  current.endpoint[0] && current.token[0] ? "yes" : "no");
    memset(current.token, 0, sizeof(current.token));
  } else if (command.length() > 0) {
    Serial.println("[Heartbeat] Commands: heartbeat configure <https-url> <64-hex-token>, heartbeat status, heartbeat clear");
  }
}

void updateSerialProvisioning() {
  while (Serial.available() > 0) {
    const int incoming = Serial.read();
    if (incoming < 0) break;
    if (incoming == '\r') continue;
    if (incoming == '\n') {
      if (!serialLineOverflow) handleSerialCommand();
      else Serial.println("[Heartbeat] Command was too long and was discarded.");
      memset(serialLine, 0, sizeof(serialLine));
      serialLineLength = 0;
      serialLineOverflow = false;
      continue;
    }
    if (serialLineLength + 1 >= sizeof(serialLine)) {
      serialLineOverflow = true;
      continue;
    }
    if (!serialLineOverflow) serialLine[serialLineLength++] = static_cast<char>(incoming);
  }
}

void heartbeatTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));

    HeartbeatSnapshot current;
    copySnapshot(current);
    if (!current.wifiConnected || !current.endpoint[0] || !current.token[0]) {
      memset(current.token, 0, sizeof(current.token));
      continue;
    }

    WiFiClientSecure secureClient;
    secureClient.useBuiltinCACertBundle();
    secureClient.setHandshakeTimeout(5);

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    if (!http.begin(secureClient, current.endpoint)) {
      Serial.println("[Heartbeat] HTTPS request could not be started.");
      memset(current.token, 0, sizeof(current.token));
      continue;
    }

    http.addHeader("Authorization", String("Bearer ") + current.token);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> document;
    document["device_id"] = DEVICE_ID;
    document["firmware_version"] = FIRMWARE_VERSION;
    document["machine_state"] = current.machineBusy ? "busy" : "idle";
    document["wifi_connected"] = current.wifiConnected;
    document["reported_pump_state"] = current.pumpRunning;
    String body;
    serializeJson(document, body);

    const int status = http.POST(body);
    http.end();
    memset(current.token, 0, sizeof(current.token));
    if (status < 200 || status >= 300) {
      Serial.printf("[Heartbeat] HTTPS request failed with status %d.\n", status);
    } else {
      Serial.printf("[Heartbeat] Sent successfully (HTTP %d).\n", status);
    }
  }
}
} // namespace

void MachineHeartbeat::init() {
  if (!heartbeatPreferences.begin("machine-hb", false)) {
    Serial.println("[Heartbeat] Device settings storage is unavailable.");
    return;
  }

  const String endpoint = heartbeatPreferences.getString("endpoint", "");
  String token = heartbeatPreferences.getString("token", "");
  if (validEndpoint(endpoint) && validToken(token)) {
    char endpointCopy[ENDPOINT_CAPACITY] = {};
    char tokenCopy[TOKEN_LENGTH + 1] = {};
    endpoint.toCharArray(endpointCopy, sizeof(endpointCopy));
    token.toCharArray(tokenCopy, sizeof(tokenCopy));
    portENTER_CRITICAL(&snapshotMux);
    memcpy(snapshot.endpoint, endpointCopy, sizeof(snapshot.endpoint));
    memcpy(snapshot.token, tokenCopy, sizeof(snapshot.token));
    portEXIT_CRITICAL(&snapshotMux);
    memset(tokenCopy, 0, sizeof(tokenCopy));
  } else {
    Serial.println("[Heartbeat] Cloud heartbeat is not configured. Configure it through the USB serial console.");
  }
  token = "";

  const BaseType_t created = xTaskCreate(
    heartbeatTask,
    "machine-heartbeat",
    8192,
    nullptr,
    1,
    &heartbeatTaskHandle
  );
  if (created != pdPASS) {
    heartbeatTaskHandle = nullptr;
    Serial.println("[Heartbeat] Background task could not start.");
  } else if (snapshot.endpoint[0] && snapshot.token[0]) {
    xTaskNotifyGive(heartbeatTaskHandle);
  }
}

void MachineHeartbeat::update() {
  updateSerialProvisioning();

  const bool wifiConnected = WiFiProvisioning::isStaConnected();
  const bool machineBusy = !OperationState::isIdle();
  const bool pumpRunning = PumpManager::isAnyPumpRunning();
  bool changed;

  portENTER_CRITICAL(&snapshotMux);
  changed = snapshot.wifiConnected != wifiConnected
      || snapshot.machineBusy != machineBusy
      || snapshot.pumpRunning != pumpRunning;
  snapshot.wifiConnected = wifiConnected;
  snapshot.machineBusy = machineBusy;
  snapshot.pumpRunning = pumpRunning;
  portEXIT_CRITICAL(&snapshotMux);

  if (changed && heartbeatTaskHandle) xTaskNotifyGive(heartbeatTaskHandle);
}
