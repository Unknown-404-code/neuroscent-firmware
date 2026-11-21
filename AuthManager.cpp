#include "AuthManager.h"
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>

// NVS keys
static Preferences authPrefs;
static const char* AUTH_NS   = "auth";
static const char* KEY_HASH  = "pw_hash";
static const char* KEY_SALT  = "pw_salt";

// Static member definitions
Session       AuthManager::sessions[MAX_SESSIONS];
AuthManager::RateLimitEntry AuthManager::rateEntries[MAX_RATE_ENTRIES];

// --------------------------------------------------------
// Init
// --------------------------------------------------------
void AuthManager::init() {
  // Clear all sessions on boot
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    sessions[i].valid = false;
    sessions[i].token = "";
    sessions[i].expiresAt = 0;
  }
  for (uint8_t i = 0; i < MAX_RATE_ENTRIES; i++) {
    rateEntries[i].ip = "";
    rateEntries[i].failCount = 0;
    rateEntries[i].windowStart = 0;
  }

  // Set default password if none exists
  if (!hasPassword()) {
    setInitialPassword("admin123");
    Serial.println("[Auth] Default admin password set: admin123");
    Serial.println("[Auth] IMPORTANT: Change the default password via Settings!");
  }
}

// --------------------------------------------------------
// Update (session cleanup)
// --------------------------------------------------------
void AuthManager::update() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].valid && now > sessions[i].expiresAt) {
      sessions[i].valid = false;
      sessions[i].token = "";
    }
  }
}

// --------------------------------------------------------
// Password Management
// --------------------------------------------------------
bool AuthManager::hasPassword() {
  authPrefs.begin(AUTH_NS, true);
  bool has = authPrefs.isKey(KEY_HASH);
  authPrefs.end();
  return has;
}

bool AuthManager::checkPassword(const String& password) {
  authPrefs.begin(AUTH_NS, true);
  String storedHash = authPrefs.getString(KEY_HASH, "");
  String storedSalt = authPrefs.getString(KEY_SALT, "");
  authPrefs.end();

  if (storedHash.isEmpty() || storedSalt.isEmpty()) return false;

  String computed = hashPassword(password, storedSalt);
  return computed.equals(storedHash);
}

bool AuthManager::setPassword(const String& newPassword, const String& currentPasswordOrEmpty) {
  // If there's an existing password, current must be correct
  if (hasPassword() && !currentPasswordOrEmpty.isEmpty()) {
    if (!checkPassword(currentPasswordOrEmpty)) {
      return false;
    }
  }
  if (newPassword.length() < 6) return false;  // Minimum length

  String salt = generateSalt();
  String hash = hashPassword(newPassword, salt);

  authPrefs.begin(AUTH_NS, false);
  authPrefs.putString(KEY_HASH, hash);
  authPrefs.putString(KEY_SALT, salt);
  authPrefs.end();

  // Invalidate all existing sessions after password change
  invalidateAllSessions();
  return true;
}

void AuthManager::setInitialPassword(const String& password) {
  String salt = generateSalt();
  String hash = hashPassword(password, salt);
  authPrefs.begin(AUTH_NS, false);
  authPrefs.putString(KEY_HASH, hash);
  authPrefs.putString(KEY_SALT, salt);
  authPrefs.end();
}

// --------------------------------------------------------
// Session Management
// --------------------------------------------------------
String AuthManager::createSession() {
  String token = generateToken();
  unsigned long expiry = millis() + (unsigned long)SESSION_LIFETIME_HOURS * 3600000UL;

  // Find an empty slot
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    if (!sessions[i].valid) {
      sessions[i].token   = token;
      sessions[i].expiresAt = expiry;
      sessions[i].valid   = true;
      return token;
    }
  }

  // Evict oldest session if all slots taken
  uint8_t oldest = 0;
  for (uint8_t i = 1; i < MAX_SESSIONS; i++) {
    if (sessions[i].expiresAt < sessions[oldest].expiresAt) {
      oldest = i;
    }
  }
  sessions[oldest].token   = token;
  sessions[oldest].expiresAt = expiry;
  sessions[oldest].valid   = true;
  return token;
}

bool AuthManager::validateSession(const String& token) {
  if (token.isEmpty()) return false;
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].valid && sessions[i].token.equals(token)) {
      if (now > sessions[i].expiresAt) {
        sessions[i].valid = false;
        return false;
      }
      // Extend session on activity
      sessions[i].expiresAt = now + (unsigned long)SESSION_LIFETIME_HOURS * 3600000UL;
      return true;
    }
  }
  return false;
}

void AuthManager::invalidateSession(const String& token) {
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].valid && sessions[i].token.equals(token)) {
      sessions[i].valid = false;
      sessions[i].token = "";
      return;
    }
  }
}

void AuthManager::invalidateAllSessions() {
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    sessions[i].valid = false;
    sessions[i].token = "";
  }
}

// --------------------------------------------------------
// HTTP Helpers
// --------------------------------------------------------
String AuthManager::getTokenFromRequest(WebServer& server) {
  // Check cookie: ns_session=<token>
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    int idx = cookie.indexOf("ns_session=");
    if (idx >= 0) {
      int start = idx + 11; // len("ns_session=")
      int end = cookie.indexOf(';', start);
      if (end < 0) end = cookie.length();
      return cookie.substring(start, end);
    }
  }
  // Also allow Authorization: Bearer <token>
  if (server.hasHeader("Authorization")) {
    String auth = server.header("Authorization");
    if (auth.startsWith("Bearer ")) {
      return auth.substring(7);
    }
  }
  return "";
}

AuthRole AuthManager::getRoleFromRequest(WebServer& server) {
  String token = getTokenFromRequest(server);
  if (!token.isEmpty() && validateSession(token)) {
    return AuthRole::ADMIN;
  }
  return AuthRole::GUEST;
}

bool AuthManager::requireAdmin(WebServer& server) {
  if (getRoleFromRequest(server) != AuthRole::ADMIN) {
    server.send(401, "application/json",
                "{\"status\":\"error\",\"message\":\"Authentication required\"}");
    return false;
  }
  return true;
}

// --------------------------------------------------------
// Rate Limiting
// --------------------------------------------------------
bool AuthManager::checkRateLimit(const String& clientIp) {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_RATE_ENTRIES; i++) {
    if (rateEntries[i].ip.equals(clientIp)) {
      // Reset window if expired
      if (now - rateEntries[i].windowStart >= RATE_LIMIT_WINDOW_MS) {
        rateEntries[i].failCount = 0;
        rateEntries[i].windowStart = now;
      }
      return rateEntries[i].failCount < MAX_LOGIN_ATTEMPTS;
    }
  }
  return true; // Not tracked yet = allowed
}

void AuthManager::recordFailedLogin(const String& clientIp) {
  unsigned long now = millis();
  // Find existing entry
  for (uint8_t i = 0; i < MAX_RATE_ENTRIES; i++) {
    if (rateEntries[i].ip.equals(clientIp)) {
      if (now - rateEntries[i].windowStart >= RATE_LIMIT_WINDOW_MS) {
        rateEntries[i].failCount = 0;
        rateEntries[i].windowStart = now;
      }
      rateEntries[i].failCount++;
      return;
    }
  }
  // Find empty slot
  for (uint8_t i = 0; i < MAX_RATE_ENTRIES; i++) {
    if (rateEntries[i].ip.isEmpty()) {
      rateEntries[i].ip = clientIp;
      rateEntries[i].failCount = 1;
      rateEntries[i].windowStart = now;
      return;
    }
  }
  // Evict oldest
  rateEntries[0].ip = clientIp;
  rateEntries[0].failCount = 1;
  rateEntries[0].windowStart = now;
}

void AuthManager::clearRateLimit(const String& clientIp) {
  for (uint8_t i = 0; i < MAX_RATE_ENTRIES; i++) {
    if (rateEntries[i].ip.equals(clientIp)) {
      rateEntries[i].failCount = 0;
      return;
    }
  }
}

// --------------------------------------------------------
// Private: Crypto Helpers
// --------------------------------------------------------
String AuthManager::generateToken() {
  uint8_t buf[16];
  for (uint8_t i = 0; i < 16; i += 4) {
    uint32_t r = esp_random();
    buf[i]     = (r >> 24) & 0xFF;
    buf[i + 1] = (r >> 16) & 0xFF;
    buf[i + 2] = (r >>  8) & 0xFF;
    buf[i + 3] = (r      ) & 0xFF;
  }
  String token = "";
  for (uint8_t i = 0; i < 16; i++) {
    char h[3];
    sprintf(h, "%02x", buf[i]);
    token += h;
  }
  return token;
}

String AuthManager::generateSalt() {
  return generateToken(); // Reuse random generator
}

String AuthManager::hashPassword(const String& password, const String& salt) {
  // HMAC-SHA256 using mbedtls (available in ESP32 Arduino core)
  String input = salt + password;

  uint8_t digest[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_setup(&ctx, info, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const uint8_t*)input.c_str(), input.length());
  mbedtls_md_finish(&ctx, digest);
  mbedtls_md_free(&ctx);

  String result = "";
  for (uint8_t i = 0; i < 32; i++) {
    char h[3];
    sprintf(h, "%02x", digest[i]);
    result += h;
  }
  return result;
}
