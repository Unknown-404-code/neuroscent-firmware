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
- Pump 1-8 Pins: `1, 2, 42, 41, 40, 39, 38, 21` (Digital HIGH = ON, LOW = OFF)

**Buttons:**
- Up Pin: `47`
- Down Pin: `48`
- Select Pin: `45`
- Back Pin: `0`
*(Configured as INPUT_PULLUP, Pressed = LOW)*

**OLED Display:**
- Type: SSD1306 (128x64)
- I2C Address: `0x3C`
- SDA Pin: `14`
- SCL Pin: `13`

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
2. **Open** `neuroscent-firmware.ino` using the Arduino IDE.
3. **Configure Network settings**:
   - Use the existing NeuroScent Wi-Fi setup access point and setup page to connect the controller to your Wi-Fi.
   - After setup, confirm the local dashboard is reachable at `http://neuroscent.local`.
4. **Compile and Upload**:
   - Select `ESP32S3 Dev Module` in `Tools > Board`. This firmware targets the ESP32-S3 N16R8.
   - Set `Flash Size` to `16MB (128Mb)`, `PSRAM` to `OPI PSRAM` and `Partition Scheme` to `Custom` (this uses the project's `partitions.csv`).
   - Keep `Erase All Flash Before Sketch Upload` disabled so saved calibration, container levels and Wi-Fi are kept.
   - Select the connected port in `Tools > Port`.
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
- **`MachineHeartbeat`**: Sends device status to the secured Supabase heartbeat function over certificate-validated HTTPS.

## Phase 4: Secure Cloud Heartbeat Setup

The heartbeat only reports firmware version, idle/busy state, Wi-Fi connectivity, and the controller-reported pump state. It does not call any pump, purge, calibration, test, or dispense endpoint. The reported pump value comes from the existing firmware state tracker, not a physical sensor.

### Prepare the cloud side when deployment is approved

1. Apply the website project's migration `20261109120000_machine_devices.sql`.
   Review the remote migration list first; `supabase db push` applies every pending local migration, not just this file.
2. Generate a 32-byte device credential locally, for example:
   ```sh
   node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"
   ```
   Keep the 64-character value private; do not put it in source control or screenshots.
3. In the Supabase project's Edge Function secrets, add that value as `MACHINE_HEARTBEAT_TOKEN`.
4. From the website project directory, deploy `machine-heartbeat` with JWT verification disabled for this function only:
   ```sh
   supabase functions deploy machine-heartbeat --no-verify-jwt
   ```
   The function validates the dedicated bearer credential itself and uses the Supabase service-role key only on the server.
5. The Admin Portal reads status through its existing admin-protected endpoint. The machine is online only while its last server-timestamped heartbeat is no older than 180 seconds.

### Provision the ESP32 when firmware upload is approved

1. Build and upload the firmware using the board settings above and the libraries listed above. Compiled against Arduino-ESP32 core 3.3.12; older cores may not provide `useBuiltinCACertBundle()`.
2. Connect USB serial at 115200 baud. Use the existing Wi-Fi provisioning process to connect the ESP32 to a network with internet access.
3. Enter this command in the serial console, replacing both placeholders:
   ```text
   heartbeat configure https://<project-ref>.supabase.co/functions/v1/machine-heartbeat <64-hex-device-credential>
   ```
   The command is not echoed or logged by the firmware. The URL and credential are stored in the ESP32 NVS, not in source files. Keep the USB console physically private while entering the credential.
4. Run `heartbeat status` to confirm only whether cloud configuration is present; the credential is never printed.
5. After the cloud migration and function are deployed, wait for the next heartbeat (normally within 60 seconds), then open Admin Portal → Machine monitoring.

The firmware uses the Arduino-ESP32 built-in trusted certificate bundle to validate the HTTPS certificate chain. There is no insecure TLS fallback. If Wi-Fi is down, the cloud heartbeat stops and the Admin Portal will show the device offline after the freshness timeout.

The device credential is kept out of source and stored in ESP32 NVS. Standard NVS is not encrypted by this change; installations requiring protection against physical flash extraction should enable flash encryption and secure boot using the procedure for the exact ESP32 board before provisioning the credential.

### Test the heartbeat without running pumps

The heartbeat never starts a pump. For this test, do not submit a formula, and do not use purge, calibration or pump-test screens. For extra safety, disconnect the pump power supply.

Open Serial Monitor at **115200 baud** with the line ending set to **New Line**; commands are ignored without it. If nothing appears after a reset, you may be on the board's other USB port: use the port that shows the boot banner, or set `USB CDC On Boot` to `Enabled` when using the native USB port and upload again.

| # | Do this | Expected Serial Monitor output |
|---|---------|--------------------------------|
| 1 | Reset the board (first boot, not configured) | `--- NeuroScent Firmware 2.0.0-web-ui ---` and `[Heartbeat] Cloud heartbeat is not configured. Configure it through the USB serial console.` |
| 2 | Wait for Wi-Fi, or join the setup access point if asked | `[WiFi] Connected! IP: ...` |
| 3 | Send `heartbeat status` | `[Heartbeat] Endpoint and credential configured: no` |
| 4 | Send `heartbeat configure https://example.com/x abc` | `[Heartbeat] Invalid URL or credential; nothing was saved.` |
| 5 | Send the real `heartbeat configure ...` command | `[Heartbeat] Cloud settings saved to device NVS; the credential was not echoed.` then, within seconds, `[Heartbeat] Sent successfully (HTTP 2xx).` |
| 6 | Wait 2–3 minutes | One `Sent successfully` line about every 60 seconds |
| 7 | Open Admin Portal → Machine monitoring | Machine online, state idle, Wi-Fi connected, pump not running, version `2.0.0-web-ui` |
| 8 | Reset the board | No "not configured" line, and a heartbeat is sent soon after Wi-Fi connects |
| 9 | Turn off the Wi-Fi network | `[WiFi] Connection lost, will attempt reconnection.` and no heartbeat lines. OLED and buttons keep working. The portal shows offline after 180 seconds |
| 10 | Restore Wi-Fi, then send a `heartbeat configure` with the real URL and a wrong 64-hex token | `[Heartbeat] HTTPS request failed with status 4xx.` while the local dashboard, OLED and buttons keep working |
| 11 | Send the real `heartbeat configure ...` command again | `Sent successfully` returns |

Serial Monitor shows what you type in its input box, so keep the screen private and do not screenshot it while entering the credential.

| Failure status | Likely cause |
|----------------|--------------|
| Negative number (for example `-1`) | No connection: DNS, internet access, or the TLS certificate check failed. Set `Core Debug Level` to `Error` and upload again to see the reason |
| `401` or `403` | The token does not match `MACHINE_HEARTBEAT_TOKEN` |
| `404` | Wrong project reference, or the function is not deployed |
| `5xx` | Supabase function error; check that the migration is applied and read the function logs |

## License
*Feel free to add the project's license description here.*
