#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// SYSTEM STATES
// ==========================================
enum class AppState { IDLE, BUSY };

// ==========================================
// BUTTON EVENTS
// ==========================================
enum class ButtonEvent { NONE, UP, DOWN, SELECT, BACK };

// ==========================================
// HARDWARE PINS
// ==========================================

// PUMPS (Digital ON/OFF - HIGH = ON, LOW = OFF)
constexpr uint8_t PUMP_PINS[8] = {4, 5, 6, 7, 15, 16, 8, 3};
constexpr uint8_t NUM_PUMPS = 8;

// BUTTONS (INPUT_PULLUP - Pressed = LOW)
constexpr uint8_t BTN_UP_PIN = 10;
constexpr uint8_t BTN_DOWN_PIN = 11;
constexpr uint8_t BTN_SELECT_PIN = 12;
constexpr uint8_t BTN_BACK_PIN = 13;

// OLED DISPLAY (SSD1306 128x64 I2C)
constexpr uint8_t OLED_SDA_PIN = 17;
constexpr uint8_t OLED_SCL_PIN = 18;
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;

// ==========================================
// DEFAULTS & CONSTANTS
// ==========================================
constexpr float DEFAULT_CONTAINER_CAPACITY_ML = 100.0f;
constexpr float DEFAULT_ML_PER_MS = 0.005f; // Reasonable default
constexpr uint8_t DEBOUNCE_DELAY_MS = 50;

// PWM CONFIGURATION
// PWM is used to reduce average voltage/speed while keeping torque, preventing
// the pumps from running too fast at 12V without wasting power as heat (like
// resistors would). The hardware driver board (YYNMOS-4 or similar) supports up
// to 1 kHz PWM.
constexpr uint32_t PWM_FREQ_HZ = 1000;
constexpr uint8_t PWM_RES_BITS = 8;            // 8-bit resolution (0..255)
constexpr uint8_t DEFAULT_PWM_RESOLUTION = 80; // Default duty cycle (~31%)

// ==========================================
// NETWORK CONFIGURATION (STA Mode)
// ==========================================
#define WIFI_SSID "ppm-2.4G"
#define WIFI_PASS "0509974470"

// Static IP Configuration
#define STATIC_IP "192.168.1.200"
#define STATIC_GATEWAY "192.168.1.1"
#define STATIC_SUBNET "255.255.255.0"
#define STATIC_DNS "8.8.8.8"

// Web Server Port
constexpr uint16_t HTTP_PORT = 80;

#endif // CONFIG_H
