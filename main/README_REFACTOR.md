# SVBC Alternator Regulator — ESP32 DevKit V1 + INA226 + TJA1051T/3

## Purpose

This project is a custom ESP32-based alternator regulator intended to replace a Wakespeed WS500-style external regulator.

It controls alternator field current with PWM, while using measured bus voltage, alternator current, alternator case temperature, engine-room ambient temperature, RPM/CAN status, and safety limits.

The Victron Cerbo/DCVV system is expected to own charging policy. This regulator executes control and safety limits; it does **not** implement bulk/absorb/float charging stages locally.

## Current hardware target

Primary target board:

- **ESP32 DevKit V1**
- **TJA1051T/3 CAN transceiver** for NMEA2000/TWAI
- **INA226** for voltage/current measurement
- **2x DS18B20** 3-wire digital temperature probes
- **IRLZ44N** or equivalent logic-level MOSFET for alternator field control
- **SR560** Schottky diode across the alternator field coil
- **MPM3610** buck regulator for power

## Why ESP32 DevKit V1

The DevKit V1 has more usable GPIO than a compact XIAO ESP32S3 and is easier to wire, debug, and repair. For a one-off marine controller, that matters more than board size.

## Main functions

- Read voltage and current from INA226
- Read alternator case and ambient temperatures from DS18B20 probes
- Control alternator field with PWM through a low-side MOSFET
- Apply PID voltage regulation
- Apply temperature derating
- Apply hard alternator temperature cutoff with hysteresis
- Apply soft-start ramp
- Require RPM above threshold for a configured hold time before enabling field
- Disable field when Cerbo SOC is above configured threshold, and re-enable below lower threshold
- Expose status/config through the ESP32 web UI
- Run fallback WiFi AP if no configured WiFi connects
- Listen for NMEA2000/TWAI traffic through TJA1051T/3

## GPIO pin map

Defined in `pins.h`.

| Role | ESP32 GPIO | Notes |
|---|---:|---|
| Field PWM output | GPIO25 | Drives MOSFET gate through resistor |
| DS18B20 OneWire data | GPIO26 | Both temperature probes share this bus |
| Maintenance mode switch | GPIO27 | DIP switch to GND enables verbose serial logging |
| INA226 I2C SDA | GPIO21 | ESP32 default SDA |
| INA226 I2C SCL | GPIO22 | ESP32 default SCL |
| CAN/TWAI TX | GPIO5 | Connect to TJA1051 TXD |
| CAN/TWAI RX | GPIO4 | Connect to TJA1051 RXD |

Important: CAN was intentionally moved off GPIO21/22 so it does not conflict with INA226 I2C.

## TJA1051T/3 CAN transceiver wiring

The TJA1051T/3 is the preferred transceiver over TJA1050 because it is compatible with 3.3V ESP32 logic.

Typical wiring:

```text
ESP32 GPIO5  -> TJA1051 TXD
ESP32 GPIO4  -> TJA1051 RXD
ESP32 GND    -> TJA1051 GND
TJA1051 CANH -> NMEA2000 CAN-H
TJA1051 CANL -> NMEA2000 CAN-L
TJA1051 STB  -> GND for normal operation
```

Check your module pin labels. Some modules expose `STB`, `S`, or `EN`. For a standard TJA1051 standby pin, tie it LOW/GND for normal operation.

Only install a 120 ohm CAN termination resistor if this node is at the physical end of the backbone and the backbone is not already properly terminated. A normal NMEA2000 backbone already has two terminators, one at each end.

## INA226 wiring

The INA226 measures both bus voltage and shunt voltage/current. The old voltage-divider method should not be used.

```text
ESP32 3.3V  -> INA226 VCC
ESP32 GND   -> INA226 GND
ESP32 GPIO21 -> INA226 SDA
ESP32 GPIO22 -> INA226 SCL
```

Shunt wiring:

```text
Battery negative / bus side ----[ SHUNT ]---- alternator/load side
INA226 IN+ -> battery/bus side of shunt
INA226 IN- -> alternator/load side of shunt
```

Use short Kelvin sense leads from the shunt sense screws. Twisted pair is recommended.

Default shunt assumption:

```text
500A / 50mV shunt = 0.0001 ohm
```

This is defined in `sensors.cpp` as `SHUNT_RESISTANCE_OHM`.

## DS18B20 temperature wiring

Both DS18B20 probes share one OneWire bus.

```text
DS18B20 VCC  -> 3.3V
DS18B20 GND  -> GND
DS18B20 DATA -> GPIO26
4.7k resistor from DATA to 3.3V
```

Sensor index convention:

```text
Index 0 = alternator case temperature
Index 1 = engine room ambient temperature
```

For long engine-room runs, use 3-wire mode, not parasite power. 15 ft is acceptable if routed cleanly. Keep DS18B20 wiring away from alternator output and starter cables.

Invalid DS18B20 values are treated as missing/faulted:

```text
-127°C = disconnected
85°C   = power-on/default conversion value, ignored
```

## Field control hardware

The regulator uses low-side MOSFET field control.

```text
Switched +12V -> Alternator Field+
Alternator Field- -> MOSFET Drain
MOSFET Source -> Ground
```

Gate network:

```text
ESP32 GPIO25 -> 100 ohm resistor -> MOSFET Gate
MOSFET Gate -> 10k resistor -> Ground
```

Flyback diode across the field coil:

```text
SR560 cathode/stripe -> Field+
SR560 anode          -> MOSFET drain / Field-
```

MOSFET recommendation:

- IRLZ44N is acceptable and available
- IRLB3034 or similar lower-Rds MOSFET is better if buying parts

MOSFET mounting warning:

- TO-220 tab is usually connected to Drain
- Do not bolt the tab directly to the aluminum case unless using an insulating thermal pad and shoulder washer

## Power

Use the MPM3610 or similar buck converter to power the ESP32 from boat 12V.

Recommended:

```text
12V input -> small fuse -> MPM3610 -> ESP32 VIN/5V
GND common between ESP32, INA226, field driver, alternator/battery negative, and CAN transceiver
```

Recommended capacitors:

- 100uF electrolytic on 12V input
- 0.1uF ceramic near power input
- 10uF + 0.1uF near INA226 if wiring is noisy

Recommended protection:

- TVS diode such as SMBJ24A on 12V input

## Startup check routine

On boot the firmware performs a one-time startup check and prints clear serial output.

Checks:

- INA226 voltage/current monitor present
- Alternator DS18B20 case temperature valid
- Engine-room DS18B20 ambient temperature valid
- N2K/CAN traffic present, if `canInput` is enabled

Example failure output:

```text
=== Alternator Regulator Startup Check ===
Checking INA226 voltage/current monitor... ERROR / not present
Checking alternator DS18B20 case temp... ERROR / not present
Checking engine room DS18B20 ambient temp... ERROR / not present
Checking N2K/CAN input... ERROR / not present
------------------------------------------
System check complete: unable to continue normal regulation.
Maintenance mode: disabled.
==========================================
```

The startup check is intentionally quiet after initial report unless maintenance mode is enabled.

## Maintenance mode

GPIO27 uses `INPUT_PULLUP`.

```text
DIP switch open        = normal quiet mode
DIP switch to GND      = maintenance / verbose serial logging
```

Use this when the case is open and a laptop is plugged into USB.

## NMEA2000 / CAN usage currently

The code uses ESP32 TWAI directly through `driver/twai.h`.

Current N2K functions:

- Detect bus traffic for startup sanity checks
- Read engine RPM from PGN `127488` where available
- Read battery SOC from PGN `127506` for Cerbo SOC inhibit

SOC inhibit logic:

- Default enabled
- Disable field at or above 95% SOC
- Re-enable only at or below 90% SOC
- Uses hysteresis to prevent rapid cycling

This is a protective fallback. Long term, proper Cerbo/DCVV control should provide voltage target/current limit/charge permission explicitly.

## Web UI

Fallback AP defaults:

```text
SSID: SVBC_Alternator
Password: Nitt4agm2
```

The UI is served from `data/` uploaded to the ESP32 filesystem. If the filesystem page is missing, the firmware includes a fallback page.

Common endpoints:

```text
GET  /api/status
GET  /api/config
POST /api/config
GET  /api/enable
POST /api/enable
GET  /api/log
POST /api/network
POST /api/network/reset
POST /api/reboot
```

## Web UI organization

The UI is organized like the charger-control UI:

1. Operational data at top
2. Settings below
3. Network settings
4. Verbose/debug/raw status at bottom

## Config persistence

Configuration is stored in ESP32 flash through the config store.

Expected persistent settings include:

- Target voltage
- Current limit
- PID constants
- Derate start/stop temps
- Hard cutoff/reset temps
- Soft-start seconds
- Min start RPM
- RPM hold seconds
- SOC inhibit enable/high/low thresholds
- CAN input enable
- WiFi credentials

Runtime state does not persist, including:

- Field enabled state
- PID integral
- active latches/faults
- CAN online state

## Safety behavior

Field is forced off if:

- INA226 is missing or invalid
- Startup check fails
- System is disabled
- Alternator temperature sensor fails
- Alternator hard temperature cutoff is latched
- Cerbo SOC inhibit is latched
- Required RPM gate has not been satisfied

## Temperature behavior

Derating:

- Reduces field output linearly between configured derate start/stop temperatures

Hard cutoff:

- Forces field off at configured hard cutoff temperature
- Re-enables only after alternator temp falls below reset temperature

This prevents chatter near the cutoff threshold.

## Bench testing notes

Minimum useful bench setup:

- ESP32 DevKit V1
- INA226 connected over I2C
- DS18B20 probes connected to GPIO26
- MOSFET output driving LED/test load instead of real alternator field
- Serial monitor open at 115200
- Web UI open through fallback AP or configured WiFi

Before alternator testing, verify:

- INA226 reports realistic bus voltage
- Current reads near zero at rest
- DS18B20 sensors report sane values
- Removing a temp sensor causes fault/warning
- Field PWM goes to zero when INA is missing
- Field PWM goes to zero on hard temp cutoff
- SOC inhibit latches/re-enables correctly when simulated

## Known next steps

- Tune PID gains on the actual alternator
- Verify field PWM frequency and MOSFET heating under real field load
- Verify RPM PGN parsing from the actual engine/Cerbo/N2K source
- Decide whether to implement explicit Cerbo/DCVV voltage/current/permission control instead of SOC-only fallback
- Add proper sensor addressing for DS18B20 if probe order ever swaps

## Current board support summary

This package is now configured for:

```text
Board: ESP32 DevKit V1
CAN: external TJA1051T/3 transceiver
Current/Voltage: INA226 over I2C
Temps: DS18B20 OneWire
Field: MOSFET low-side PWM
```
