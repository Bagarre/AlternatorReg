![Alternator Icon](icon.png)

# Smart Alternator Regulator

An ESP32-based smart alternator field controller with CANbus integration, web interface, temperature/current derating, and persistent configuration.

---

## 🚀 Features

* Real-time PWM control of alternator field via MOSFET
* Dynamic regulation based on:

  * Target voltage
  * Current limit (via INA260 sensor)
  * Alternator temperature
* CANbus input for engine RPM (PGN 127488)
* NMEA2000 compatible
* Web interface with live status, configuration, and log
* Wi-Fi Access Point fallback ("Alt\_Reg")
* mDNS support: access via `http://alternator.local`
* Config persistence via NVS/Preferences

---

## 📦 Hardware Requirements

| Component                    | Notes                                |
| ---------------------------- | ------------------------------------ |
| ESP32 Dev Module             | Any common dev board                 |
| Adafruit INA260              | Current + Voltage monitor            |
| Logic-level N-channel MOSFET | FQP30N06L or similar                 |
| 10k + 3.3k Resistors         | Voltage divider for sensing          |
| Flyback diode                | Protect MOSFET from inductive spikes |
| 5V Buck converter            | For ESP32 power from NMEA2000        |
| CAN transceiver              | TJA1051T/3 (Adafruit CAN Pal)        |

---

## 🔌 Pinout

### 🧲 MOSFET Gate Drive

To control the alternator field, a logic-level N-channel MOSFET (e.g., FQP30N06L) is used with PWM on the gate.

| Pin           | Connects To         | Notes                                   |
| ------------- | ------------------- | --------------------------------------- |
| MOSFET Gate   | ESP32 GPIO 25       | Via 100Ω resistor, with 10kΩ pull-down  |
| MOSFET Drain  | Field negative lead | High-current path                       |
| MOSFET Source | System Ground       | Shared with ESP32 and alternator system |

Include a flyback diode (e.g., 1N5819) across the field coil: cathode to 12V, anode to MOSFET drain.

### 📟 INA260 Connections (to ESP32 via I²C)

The Adafruit INA260 provides both current and voltage measurement over I²C.

| INA260 Pin | Connects To         | Description                       |
| ---------- | ------------------- | --------------------------------- |
| VIN+       | Alternator Output + | High-side current sense (to load) |
| VIN−       | Alternator Output − | Return side of shunt/load         |
| SDA        | ESP32 GPIO 18       | I²C Data                          |
| SCL        | ESP32 GPIO 19       | I²C Clock                         |
| GND        | ESP32 GND           | Common Ground                     |
| VCC        | ESP32 3.3V or 5V    | Module Power                      |

Ensure pull-up resistors (4.7k–10k) are present on SDA and SCL lines if not already on the board.

### 🧷 NMEA 2000 Connector Pinout

NMEA 2000 uses a 5-pin M12 A-coded (DeviceNet Mini) connector:

| Pin | Signal | Description       |
| --- | ------ | ----------------- |
| 1   | SHLD   | Cable Shield      |
| 2   | NET-S  | CAN Low           |
| 3   | NET-C  | Ground            |
| 4   | NET-H  | CAN High          |
| 5   | NET-P  | +12V Power Supply |

**Connection to ESP32 via CAN transceiver (e.g., TJA1051T/3):**

* Pin 4 (CAN-H) → CAN\_H on transceiver
* Pin 2 (CAN-L) → CAN\_L on transceiver
* Pin 3 (NET-C) → GND (shared ground with ESP32)
* Pin 5 (NET-P) → 12V input to 5V buck converter → ESP32 VIN/5V
* Transceiver TX/RX ↔ ESP32 GPIO 22 (TX), GPIO 21 (RX)

| Signal       | ESP32 Pin |
| ------------ | --------- |
| Field PWM    | GPIO 25   |
| Alt Temp ADC | GPIO 34   |
| CAN TX       | GPIO 22   |
| CAN RX       | GPIO 21   |

---

## 🌐 Web Interface

* Accessible via `http://alternator.local` or IP address
* Three panels:

  * **Status**: live voltage, amps, temp, stage, CAN
  * **Configuration**: editable fields
  * **Network**: WiFi credentials + fallback
  * **Logs**: system events

### Live UI Preview

![UI Screenshot](ui.png)

---

## 🔧 API Reference

### `GET /status`

Returns current system metrics.

```json
{
  "voltage": "14.3 V",
  "current": "83.5 A",
  "pwm": "62%",
  "alt_temp": "161°F",
  "batt_temp": "85°F",
  "rpm": "1450",
  "stage": "Bulk",
  "can_status": "OK",
  "last_can": "12:45:12",
  "bms_permission": true,
  "enabled": true
}
```

### `GET /config`

Returns stored configuration.

### `POST /config`

Saves configuration. Example body:

```json
{
  "targetVoltage": "14.4",
  "currentLimit": "90",
  "floatVoltage": "13.5",
  "derateTemp": "90",
  "canInput": true
}
```

### `GET /enable`

Returns `{ "enabled": true/false }`

### `POST /enable`

Sets system enable state:

```json
{
  "enabled": false
}
```

### `POST /network`

Updates Wi-Fi credentials.

```json
{
  "wifiSSID": "MyNetwork",
  "wifiPassword": "MyPassword"
}
```

---

## 📢 NMEA 2000 Messages

### ↓ Received

| PGN    | Description               | Usage                       |
| ------ | ------------------------- | --------------------------- |
| 127488 | Engine Parameters (Rapid) | Reads engine RPM (byte 3–4) |

Used to determine if the engine is running above the configured RPM threshold for enabling charge.

### ↑ Transmitted *(planned)*

| PGN | Description            | Notes                       |
| --- | ---------------------- | --------------------------- |
| TBD | Alternator Temperature | Broadcast temp from sensor  |
| TBD | Charging Status        | (Planned) Field PWM %, mode |

## ⚙️ Configuration Defaults (`config.h`)

* Target Voltage: `14.4`
* Current Limit: `90`
* Float Voltage: `13.5`
* Derate Temp: `90`
* CAN Input: `true`

---

## 🔄 Integration with Bow Thruster & Windlass Controller

This regulator can be paired with a separate bow controller for integrated marine power management.

### Coordinated Subsystems

* **Smart Alternator Regulator** (this project)

  * Regulates alternator field based on RPM, temp, and current
  * Transmits alternator temperature over CAN (PGN `0x220`)
* **Bow Controller** (optional)

  * Listens for helm commands to control bow thruster and windlass
  * Shares system power state and status via custom PGNs

### Example CAN Message Map

| PGN        | Direction | Purpose                         |
| ---------- | --------- | ------------------------------- |
| `0x100`    | IN        | Power arm/disarm                |
| `0x110`    | IN        | Bow thruster direction (L/R)    |
| `0x120`    | IN        | Windlass up/down                |
| `0x127488` | IN        | Engine RPM (standard PGN)       |
| `0x200`    | OUT       | Status (Armed, Low Voltage)     |
| `0x210`    | OUT       | Beeper feedback control         |
| `0x220`    | OUT       | Alternator temperature (0.01°C) |

This makes the system suitable for distributed marine automation.

## 📄 Circular Log Feature

A circular log buffer (similar to the bow thruster controller) is planned for tracking:

* Overcurrent incidents (INA260 measured)
* Alternator over-temperature events
* CAN timeouts or field disable triggers
* Manual disables via web UI

Logs will be exposed via `/log` API and persist in RAM (or optionally in SPIFFS for diagnostic dumps). Each entry will include:

* Timestamp
* Event type (e.g., overtemp, field disabled)
* Key data (current, voltage, temp)

## 🧰 Future Improvements

* Dynamic stage transitions (bulk, absorb, float)
* Overcurrent & overtemp logging
* Bluetooth PWA option
* Field soft-off / precharge ramp

---

## 🧑‍💻 Author

**David Ross**
MIT License
