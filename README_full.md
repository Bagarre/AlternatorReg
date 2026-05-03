# SVBC Alternator Regulator (ESP32 + INA226)

## Overview

This project is a custom alternator regulator intended to replace a Wakespeed WS500.  
It controls alternator field current using PWM based on system voltage, current, and temperature.

Design goals:
- Deterministic startup and fail-safe behavior
- Simple, robust hardware
- External system (Victron Cerbo / DCVV) controls charging strategy
- Regulator only enforces limits and executes commands

---

## Features

- INA226 for voltage and current sensing (no voltage divider)
- DS18B20 sensors for alternator case and ambient temp
- MOSFET low-side field control
- PID voltage regulation
- Temperature derating (linear)
- Soft-start ramp
- RPM gating (startup protection)
- NMEA2000 integration
- Optional WiFi web UI + fallback AP
- Maintenance mode via GPIO

---

## System Architecture

Battery → INA226 → ESP32 → PWM → MOSFET → Field Coil

Sensors:
- DS18B20 (Alt case)
- DS18B20 (Ambient)

Interfaces:
- NMEA2000 (CAN)
- Web UI (WiFi AP fallback)

---

## Hardware Components

### Core
- ESP32 Dev Board
- INA226 module
- 500A shunt
- 2x DS18B20

### Field Control
- IRLZ44N MOSFET
- SR560 diode (preferred)
- 100Ω resistor (gate)
- 10k resistor (pulldown)

### Power
- MPM3610 buck
- 100µF electrolytic
- 0.1µF ceramic

### Protection
- TVS diode (SMBJ24A recommended)

---

## GPIO Mapping

| Function | GPIO |
|---------|------|
| Field PWM | 9 |
| DS18B20 | 5 |
| Status LED | 8 |
| Maintenance mode | 12 |
| I2C SDA | 21 |
| I2C SCL | 22 |

---

## Wiring

### Field Control

Field+ → +12V (switched)  
Field- → MOSFET Drain  
Source → Ground  

### Flyback Diode

Stripe → Field+  
Non-stripe → Drain  

### Gate

ESP32 → 100Ω → Gate  
Gate → 10k → Ground  

---

## Startup Sequence

1. LED RGB cycle
2. Sensor checks:
   - INA226
   - DS18B20 (2x)
   - CAN traffic
3. Print result once
4. Enter RUN or FAIL state

---

## LED Behavior

| State | LED |
|------|-----|
| Boot | RGB cycle |
| Checking | Blue |
| OK | Green |
| Warning | Slow red |
| Error | Fast red |

---

## Control Loop

Runs every ~100ms

Error = Target - Measured Voltage  
PID → PWM  

Limits applied:
- Temp derate
- Current limit
- Soft start

---

## Temperature Derating

Start: 80°C  
Stop: 100°C  

Linear scaling

---

## Soft Start

Ramp PWM from 0 → max over configured time

---

## RPM Gate

Min RPM: 2200  
Hold time: 5 minutes  

---

## Safety

Disable field if:
- INA missing
- system disabled
- startup failed

---

## Web UI

Fallback AP:

SSID: SVBC_Alternator  
Password: Nitt4agm2  

Endpoints:
- /api/status
- /api/config
- /api/enable
- /api/network

---

## Notes

- MOSFET tab = Drain (isolate from case)
- DS18B20 invalid: 85C or -127C
- All grounds must be common
- Keep sensor wires away from alternator wiring

---

## Status

Functional prototype  
Needs tuning per alternator

---

## Future

- PID tuning
- RPM from CAN
- Current limiting curves
