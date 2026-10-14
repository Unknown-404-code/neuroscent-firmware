#include "LogManager.h"

LogEntry LogManager::buffer[MAX_LOG_ENTRIES];
uint16_t LogManager::head  = 0;
uint16_t LogManager::count = 0;

void LogManager::init() {
  head  = 0;
  count = 0;
  log(LogEvent::SYSTEM_BOOT, LogSeverity::INFO, "NeuroScent system boot");
}

void LogManager::log(LogEvent event, LogSeverity severity,
                     const String& description,
                     int8_t pumpIndex, int formulaId) {
  LogEntry& e = buffer[head];
  e.uptimeMs   = millis();
  e.event      = event;
  e.severity   = severity;
  e.pumpIndex  = pumpIndex;
  e.formulaId  = formulaId;

  // Safe string copy
  strncpy(e.description, description.c_str(), 63);
  e.description[63] = '\0';

  head = (head + 1) % MAX_LOG_ENTRIES;
  if (count < MAX_LOG_ENTRIES) count++;
}

uint16_t LogManager::getCount() { return count; }

bool LogManager::getEntry(uint16_t index, LogEntry& out) {
  if (index >= count) return false;
  // Convert logical index (0=newest) to physical ring position
  int16_t phys = (int16_t)head - 1 - (int16_t)index;
  if (phys < 0) phys += MAX_LOG_ENTRIES;
  out = buffer[phys];
  return true;
}

void LogManager::clear() {
  head  = 0;
  count = 0;
}

String LogManager::entryToJson(const LogEntry& entry) {
  char buf[256];
  unsigned long sec = entry.uptimeMs / 1000;
  unsigned long ms  = entry.uptimeMs % 1000;

  // Format uptime as "HH:MM:SS.mmm"
  char ts[16];
  snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu.%03lu",
           sec / 3600, (sec % 3600) / 60, sec % 60, ms);

  snprintf(buf, sizeof(buf),
           "{\"uptime\":\"%s\",\"event\":\"%s\",\"severity\":\"%s\","
           "\"pump\":%d,\"formula_id\":%d,\"description\":\"%s\"}",
           ts,
           eventName(entry.event),
           severityName(entry.severity),
           entry.pumpIndex,
           entry.formulaId,
           entry.description);
  return String(buf);
}

const char* LogManager::eventName(LogEvent e) {
  switch (e) {
    case LogEvent::SYSTEM_BOOT:        return "SYSTEM_BOOT";
    case LogEvent::WIFI_CONNECTED:     return "WIFI_CONNECTED";
    case LogEvent::WIFI_DISCONNECTED:  return "WIFI_DISCONNECTED";
    case LogEvent::WIFI_AP_STARTED:    return "WIFI_AP_STARTED";
    case LogEvent::FORMULA_RECEIVED:   return "FORMULA_RECEIVED";
    case LogEvent::FORMULA_ACCEPTED:   return "FORMULA_ACCEPTED";
    case LogEvent::FORMULA_CANCELED:   return "FORMULA_CANCELED";
    case LogEvent::FORMULA_COMPLETED:  return "FORMULA_COMPLETED";
    case LogEvent::FORMULA_ERROR:      return "FORMULA_ERROR";
    case LogEvent::PUMP_PURGE_START:   return "PUMP_PURGE_START";
    case LogEvent::PUMP_PURGE_STOP:    return "PUMP_PURGE_STOP";
    case LogEvent::PUMP_TEST_START:    return "PUMP_TEST_START";
    case LogEvent::PUMP_TEST_COMPLETE: return "PUMP_TEST_COMPLETE";
    case LogEvent::CALIB_UPDATED:      return "CALIB_UPDATED";
    case LogEvent::CALIB_RESET:        return "CALIB_RESET";
    case LogEvent::CONTAINER_REFILLED: return "CONTAINER_REFILLED";
    case LogEvent::CONTAINER_ADJUSTED: return "CONTAINER_ADJUSTED";
    case LogEvent::SETTINGS_CHANGED:   return "SETTINGS_CHANGED";
    case LogEvent::ADMIN_LOGIN:        return "ADMIN_LOGIN";
    case LogEvent::ADMIN_LOGOUT:       return "ADMIN_LOGOUT";
    case LogEvent::SYSTEM_ERROR:       return "SYSTEM_ERROR";
    default:                           return "UNKNOWN";
  }
}

const char* LogManager::severityName(LogSeverity s) {
  switch (s) {
    case LogSeverity::INFO:  return "INFO";
    case LogSeverity::WARN:  return "WARN";
    case LogSeverity::ERROR: return "ERROR";
    default:                 return "INFO";
  }
}
