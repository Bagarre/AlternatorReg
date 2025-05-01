# Smart Alternator Regulator + Bow Thruster & Windlass Controller

This project provides coordinated control of a marine 12V system using two CAN-enabled microcontrollers:
- A **Bow Controller** for bow thruster and windlass operation
- A **Helm Controller** (not included here) for remote control and status indication
- A **Smart Alternator Regulator** that dynamically enables alternator charging based on engine RPM and system health

---

## 🛠️ Components Used

### ⚡ Microcontroller
- **Adafruit Feather M4 CAN Express**
  - Built-in CAN controller and transceiver
  - 3.3V logic, USB-C, and FeatherWing support

### 📡 CAN Bus
- NMEA 2000 physical layer (250 kbps CAN)
- PGN 127488 (Engine Speed) decoded from engine network
- Custom PGNs for system control (bow/helm)

### 🔋 Alternator Regulator
- Controls 12V field via GPIO pin
- Charging allowed if:
  - Engine RPM > 1200 for 30s
  - BMS allows charge (flag)
  - Alternator not over temperature (flag)

### 🔌 Sensors & Inputs
- **INA226**: Current sensing for alternator output (external)
- **DS18B20 / TMP36**: For battery and alternator temperature (optional)
- **Voltage divider**: Monitors battery voltage
- **Digital Inputs**: Local control buttons for thruster, windlass

### ⚙️ Output Drivers
- **Solid-State Relays** (SSR-41FDD) or mechanical relays
- MOSFET-based drivers optional
- Flyback protection required if mechanical relays used

---

## 🔄 CAN Message Map

| PGN      | Direction | Purpose                      |
|----------|-----------|------------------------------|
| `0x100`  | IN        | Power arm/disarm             |
| `0x110`  | IN        | Bow thruster direction (L/R) |
| `0x120`  | IN        | Windlass up/down             |
| `0x127488` | IN      | Engine RPM (standard PGN)    |
| `0x200`  | OUT       | Status (Armed, Low Voltage)  |
| `0x210`  | OUT       | Beeper feedback control      |

---

## 🔧 Firmware Summary

### bow_controller.ino
- Merges local and CAN input for bow thruster & windlass
- Manages lockouts, timing limits, and safety logic
- Controls relays and feedback buzzer

### alternator_regulator.ino
- Listens to engine RPM over CAN (PGN 127488)
- Enables field pin if RPM > threshold for duration
- Coordinates with BMS and temperature flags

---

## 📁 Files

- `bow_controller.ino`: Main logic for bow control
- `alternator_regulator.ino`: Alternator charge controller
- `README.md`: This file

---

## 📌 Future Additions

- EEPROM-based config (RPM thresholds, delays)
- NMEA 2000 beeper control feedback
- Helm Feather UI for monitoring

---

## 🧑‍💻 Author

David Ross  
MIT License  
April 2025
