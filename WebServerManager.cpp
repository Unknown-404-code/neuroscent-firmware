#include "WebServerManager.h"
#include "FormulaManager.h"
#include "UIManager.h" // For globals state check (Idle vs Busy)

WebServer WebServerManager::server(HTTP_PORT);

void WebServerManager::init() {
  IPAddress ip, gateway, subnet, dns;
  ip.fromString(STATIC_IP);
  gateway.fromString(STATIC_GATEWAY);
  subnet.fromString(STATIC_SUBNET);
  dns.fromString(STATIC_DNS);

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println("Connecting to WiFi...");
  // We cannot block here indefinitely because we shouldn't block setup,
  // but typically WiFi connection in setup is expected.
  // However, to be purely non-blocking, we'll let it connect in the background.

  server.on("/", HTTP_GET, handleRoot);
  server.on("/dispense", HTTP_POST, handleDispense);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void WebServerManager::update() { server.handleClient(); }

bool WebServerManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

void WebServerManager::handleRoot() {
  server.send(200, "text/plain", "ESP32 Perfume Dispenser System ON");
}

void WebServerManager::handleDispense() {
  // Check if system is busy
  if (UIManager::getState() != UIState::MAIN_MENU) {
    server.send(503, "application/json",
                "{\"status\":\"error\",\"message\":\"System is busy\"}");
    return;
  }

  if (server.hasArg("plain") == false) {
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Body not found\"}");
    return;
  }

  String jsonStr = server.arg("plain");
  Serial.println("Received Payload: " + jsonStr);

  bool parsed = FormulaManager::parseAndPrepare(jsonStr);

  if (parsed) {
    server.send(200, "application/json",
                "{\"status\":\"ok\",\"message\":\"Formula received\"}");
  } else {
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Invalid JSON or "
                "validation failed\"}");
  }
}

void WebServerManager::handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}
