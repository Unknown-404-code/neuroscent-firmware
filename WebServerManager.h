#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include "Config.h"
#include <WebServer.h>
#include <WiFi.h>

class WebServerManager {
public:
  static void init();
  static void update();

  // Allow checking if we have network
  static bool isConnected();

private:
  static WebServer server;

  // Route Handlers
  static void handleRoot();
  static void handleDispense();
  static void handleNotFound();
};

#endif // WEBSERVER_MANAGER_H
