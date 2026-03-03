# Neuroscent Firmware - ESP32 Perfume Dispenser System

An ESP32-based advanced perfume dispensing system that accurately mixes multiple scents based on custom formulas. The system is designed with a strictly non-blocking architecture, enabling a responsive user interface via an OLED display, hardware button inputs, and asynchronous web server operations.

## Features

- **8-Pump Control System**: Accurately control up to 8 distinct liquid pumps.
- **Formula Blending**: Parse dynamic JSON formulas indicating the exact percentage ratio required from each pump.
- **REST API**: Built-in HTTP server to receive dispense commands over Wi-Fi.
- **Persistent Storage**: Utilizes ESP32 NVS (Non-Volatile Storage) to store and retrieve pump calibrations and remaining liquid levels.
- **Non-Blocking Architecture**: A state-machine-driven `loop()` guarantees real-time responsiveness for the OLED UI and hardware buttons while pumps are running.
- **Interactive UI**: Navigate through menus using 4 physical buttons (Up, Down, Select, Back) with an SSD1306 OLED I2C display.

## Hardware Configuration

The hardware configurations are strictly defined in `Config.h`. 

**Pumps:**
- Pump 1-8 Pins: `4, 5, 6, 7, 15, 16, 8, 3` (Digital HIGH = ON, LOW = OFF)

**Buttons:**
- Up Pin: `10`
- Down Pin: `11`
- Select Pin: `12`
- Back Pin: `13`
*(Configured as INPUT_PULLUP, Pressed = LOW)*

**OLED Display:**
- Type: SSD1306 (128x64)
- I2C Address: `0x3C`
- SDA Pin: `17`
- SCL Pin: `18`

## Prerequisites

To compile and run this firmware, you will need the following installed:
1. **Arduino IDE** (or Visual Studio Code with the PlatformIO extension)
2. **ESP32 Board Support Package** in the Arduino Board Manager.
3. Required Libraries (install via Arduino Library Manager):
   - `ArduinoJson` (v6 or higher)
   - `Adafruit_GFX` (for OLED drawing)
   - `Adafruit_SSD1306` (for OLED driver)

## How to Run

1. **Clone or Download** the project to your local machine.
2. **Open** `main.ino` using the Arduino IDE. 
3. **Configure Network settings**:
   - Open `Config.h` and update your Wi-Fi credentials in the STA Mode configuration section:
     ```cpp
     #define WIFI_SSID "your-ssid"
     #define WIFI_PASS "your-password"
     ```
   - Make sure to also check/update the `STATIC_IP`, `STATIC_GATEWAY`, and `STATIC_SUBNET` to match your local network layout.
4. **Compile and Upload**:
   - Select your target ESP32 board in `Tools > Board` (e.g., "ESP32 Dev Module").
   - Select the connected COM Port in `Tools > Port`.
   - Click the **Upload** button.

## API Usage

The system acts as a web server on port `80` (or `HTTP_PORT` as defined in `Config.h`) to listen for dispense commands.

### `POST /dispense`

Send a raw JSON payload to the system to trigger a new perfume mix. 

**Example Payload:**
```json
{
  "id": 123,
  "name": "Rose Garden",
  "container_ml": 50,
  "formula": [
    {
      "pump": 1,
      "percent": 60
    },
    {
      "pump": 3,
      "percent": 40
    }
  ]
}
```

**Workflow:**
1. The firmware parses the JSON payload.
2. The user interface on the OLED display will transition to a **Formula Confirm Screen**.
3. Once the user dynamically confirms the formula using the hardware buttons, the system starts the pumps sequentially based on calculated millisecond timing derived from the target ML and pump calibrations. 
4. The requested ML are finally deducted from the remaining storage variables in Non-Volatile Storage (NVS).

## Project Structure

- **`main.ino`**: Entry point orchestrating all managers.
- **`Config.h`**: Central definitions for hardware pins, network data, and core constants.
- **`FormulaManager`**: Handles JSON parsing logic, formula structure, and sequential dispensing logic.
- **`PumpManager`**: Low-level hardware abstraction for toggling relays/pumps.
- **`DisplayManager` & `UIManager`**: Handles OLED graphics, menu state machine, and progress rendering.
- **`ButtonManager`**: Debounces physical buttons into standardized events.
- **`StorageManager`**: Non-volatile storage tracking for precise liquid volumes.
- **`WebServerManager`**: HTTP REST server initialization and API endpoint handlers.

## License
*Feel free to add the project's license description here.*
