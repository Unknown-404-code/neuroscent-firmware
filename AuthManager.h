#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>

// --------------------------------------------------------
// AuthManager
//
// Implements two-tier authentication for the web UI:
//   - GUEST: read-only access (no login required)
//   - ADMIN: full control (session token required)
//
// Security:
//   - Password stored as SHA-256 hash + salt in NVS
//   - Sessions: random 32-byte hex token, 1-hour lifetime
//   - Rate limiting: max 5 failed logins / 60 seconds
//   - No passwords stored in flash plaintext
//   - No tokens or passwords logged
// --------------------------------------------------------

enum class AuthRole { GUEST, ADMIN };

struct Session {
  String token;
  unsigned long expiresAt;  // millis()
  bool valid;
};

class AuthManager {
public:
  static void init();
  static void update();  // Call from loop (session cleanup)

  // --- Password management ---
  static bool hasPassword();
  static bool checkPassword(const String& password);
  static bool setPassword(const String& newPassword, const String& currentPasswordOrEmpty);
  static void setInitialPassword(const String& password);  // First-time setup only

  // --- Session management ---
  static String createSession();                 // Returns token or "" on failure
  static bool validateSession(const String& token);
  static void invalidateSession(const String& token);
  static void invalidateAllSessions();

  // --- HTTP helpers ---
  static AuthRole getRoleFromRequest(WebServer& server);
  static bool requireAdmin(WebServer& server);  // Sends 401 if not admin, returns false
  static String getTokenFromRequest(WebServer& server);

  // --- Rate limiting ---
  static bool checkRateLimit(const String& clientIp);
  static void recordFailedLogin(const String& clientIp);
  static void clearRateLimit(const String& clientIp);

private:
  static const uint8_t MAX_SESSIONS = 8;
  static Session sessions[MAX_SESSIONS];

  static const uint8_t SESSION_LIFETIME_HOURS = 1;
  static const uint8_t MAX_LOGIN_ATTEMPTS = 5;
  static const unsigned long RATE_LIMIT_WINDOW_MS = 60000UL;

  // Rate limiting (last N attempts per IP, simple approach)
  struct RateLimitEntry {
    String ip;
    uint8_t failCount;
    unsigned long windowStart;
  };
  static const uint8_t MAX_RATE_ENTRIES = 8;
  static RateLimitEntry rateEntries[MAX_RATE_ENTRIES];

  static String generateToken();
  static String hashPassword(const String& password, const String& salt);
  static String generateSalt();
};

#endif // AUTH_MANAGER_H
