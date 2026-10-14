#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>

enum class LogSeverity { INFO, WARN, ERROR };

enum class LogEvent {
  SYSTEM_BOOT,
  WIFI_CONNECTED,
  WIFI_DISCONNECTED,
  WIFI_AP_STARTED,
  FORMULA_RECEIVED,
  FORMULA_ACCEPTED,
  FORMULA_CANCELED,
  FORMULA_COMPLETED,
  FORMULA_ERROR,
  PUMP_PURGE_START,
  PUMP_PURGE_STOP,
  PUMP_TEST_START,
  PUMP_TEST_COMPLETE,
  CALIB_UPDATED,
  CALIB_RESET,
  CONTAINER_REFILLED,
  CONTAINER_ADJUSTED,
  SETTINGS_CHANGED,
  ADMIN_LOGIN,
  ADMIN_LOGOUT,
  SYSTEM_ERROR
};

struct LogEntry {
  unsigned long uptimeMs;
  LogEvent event;
  LogSeverity severity;
  int8_t pumpIndex;     // -1 if not pump-specific
  int formulaId;        // -1 if not formula-specific
  char description[64];
};

class LogManager {
public:
  static void init();

  // Add a log entry (non-blocking, no flash writes during pump timing)
  static void log(LogEvent event, LogSeverity severity,
                  const String& description,
                  int8_t pumpIndex = -1,
                  int formulaId = -1);

  // Retrieve logs (most recent first)
  static uint16_t getCount();
  static bool getEntry(uint16_t index, LogEntry& out);  // index 0 = newest

  // Clear all logs (admin only)
  static void clear();

  // Serialise entry to JSON string (caller manages buffer)
  static String entryToJson(const LogEntry& entry);

private:
  static const uint16_t MAX_LOG_ENTRIES = 200;

  // Circular buffer in RAM (no per-pump-tick flash writes)
  static LogEntry buffer[MAX_LOG_ENTRIES];
  static uint16_t head;   // Next write position
  static uint16_t count;  // Number of valid entries

  static const char* eventName(LogEvent e);
  static const char* severityName(LogSeverity s);
};

#endif // LOG_MANAGER_H
