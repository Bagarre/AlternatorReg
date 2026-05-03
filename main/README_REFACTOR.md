# Alternator Regulator Refactor Notes

This project is the ESP32 alternator regulator web UI/controller intended to replace the basic behavior needed from a Wakespeed-style regulator while remaining locally inspectable and hackable.

## Current Architecture

The controller now uses:

- **INA226** for charge voltage, shunt voltage, and alternator/output current.
- **DS18B20 3-wire digital temperature sensors** for alternator case temperature and engine-room ambient temperature.
- **PWM field control** through the field driver output.
- **CAN/N2K input** for RPM/BMS/DCVV-style upstream control data.
- **Web UI + JSON API** for status, tuning, and network configuration.
- **Python simulator** for exercising the web UI without the ESP32 hardware.

The old voltage-divider measurement path has been removed. Voltage feedback for regulation comes from the INA226 bus-voltage reading.

## What This Controller Does Not Do

It does **not** run its own bulk/absorb/float charge stages. Charge target selection should be owned by DCVV/Cerbo/BMS logic. This regulator simply tries to regulate field output to the current requested target voltage/current limits while obeying RPM, temperature, and permission constraints.

## Control Logic

The regulator loop now includes:

### PID Voltage Regulation

The field request is based on PID error between:

```text
configured / externally supplied target voltage - INA226 bus voltage
```

Tunable values:

```text
PID Kp
PID Ki
PID Kd
```

Start conservative. Too much gain will cause hunting.

### Temperature Derating Curve

Alternator case temperature derates field output linearly:

```text
below Derate Start Temp  -> 100% duty cap
between start and stop   -> linear reduction
at/above Derate Stop Temp -> 0% duty cap
```

This is intentionally a curve, not a single cutoff.

### Soft Start Ramp

Once all enable conditions are satisfied, field output is capped by a soft-start ramp:

```text
0% -> 100% over Soft Start Seconds
```

This avoids slamming the belt/alternator immediately after the RPM gate clears.

### RPM Start Gate

The alternator will not begin regulation until engine RPM has been at or above the configured threshold for the configured hold time.

Defaults:

```text
Minimum Start RPM: 2200
RPM Hold Time: 300 seconds / 5 minutes
```

If RPM drops below the threshold, the hold timer and soft-start timer reset.

### Safety Stops

Field output is disabled if:

```text
system disabled
INA226 missing
alternator case temp sensor missing
BMS denies charge
CAN required but timed out
RPM holdoff not satisfied
voltage unavailable
```

## Wiring Summary

### INA226

```text
ESP32 3.3V -> INA226 VCC
ESP32 GND  -> INA226 GND
ESP32 SDA  -> INA226 SDA
ESP32 SCL  -> INA226 SCL

INA226 VIN+ -> battery/alternator side of shunt
INA226 VIN- -> load/battery side of shunt
```

Keep the shunt sense leads short and twisted. The regulator should live near the shunt.

### DS18B20 Sensors

Both sensors share the OneWire bus.

```text
DS18B20 red/yellow/black typical wiring:
Red    -> 3.3V
Black  -> GND
Yellow -> DATA

DATA -> 4.7k pullup -> 3.3V
```

Project default:

```text
OneWire pin: see pins.h
Index 0: alternator case temperature
Index 1: engine room ambient temperature
```

A 15 ft DS18B20 run is fine if wired cleanly. Use 3-wire mode, not parasitic power. Keep the cable away from alternator output and starter wiring where practical.

### Maintenance Mode

A DIP switch can pull the maintenance GPIO low. When low, the controller prints verbose status over USB serial.

```text
open = normal quiet mode
closed to GND = verbose maintenance mode
```

## JSON API

### `GET /api/status`

Important fields:

```json
{
  "voltage": 14.21,
  "current": 52.3,
  "shunt_mv": 5.23,
  "pwm": 71,
  "requested_pwm": 74,
  "pid_requested_pwm": 90,
  "temp_duty_cap": 255,
  "current_duty_cap": 255,
  "soft_start_duty_cap": 180,
  "alt_temp_c": 68.2,
  "engine_room_temp_c": 38.4,
  "rpm": 2400,
  "stage": "Regulating",
  "can_status": "OK",
  "bms_permission": true,
  "enabled": true,
  "field_enabled": true,
  "ina226_available": true,
  "startup_check_ok": true,
  "last_disable_reason": "None",
  "target_voltage": 14.4,
  "current_limit": 100,
  "pid_kp": 45,
  "pid_ki": 4,
  "pid_kd": 0,
  "derate_start_temp_c": 82,
  "derate_stop_temp_c": 96,
  "soft_start_seconds": 60,
  "min_start_rpm": 2200,
  "rpm_hold_seconds": 300
}
```

### `GET /api/config`

Returns the editable configuration.

### `POST /api/config`

Accepts:

```json
{
  "targetVoltage": 14.4,
  "currentLimit": 100,
  "pidKp": 45,
  "pidKi": 4,
  "pidKd": 0,
  "derateStartTemp": 82,
  "derateStopTemp": 96,
  "softStartSeconds": 60,
  "minStartRPM": 2200,
  "rpmHoldSeconds": 300,
  "canInput": true
}
```

## Simulator

Run:

```bash
python3 sim_web_ui.py
```

Open:

```text
http://127.0.0.1:8765/
```

The simulator serves the same web files and API shape as the ESP32 so the UI can be exercised without hardware.


## Wi-Fi / Web UI notes

Default fallback access point:

```text
SSID: SVBC_Alternator
Password: Nitt4agm2
URL: http://192.168.4.1/
```

The firmware now includes a small embedded fallback page at `/`, so the root page should not 404 even if the filesystem has not been uploaded. For the full phone-friendly UI, upload the contents of the `data/` folder to SPIFFS/LittleFS using the Arduino ESP32 filesystem uploader.

If `/api/status` works but CSS/JS/image files return 404, the sketch is running correctly but the filesystem data has not been uploaded.

## Hard Alternator Temperature Cutoff

In addition to the normal linear thermal derating curve, the control loop now has a latched hard cutoff:

- `hardCutoffTempC` default: 105°C
- `hardCutoffResetTempC` default: 95°C

If alternator case temperature reaches the cutoff, field output is forced to zero and the field remains disabled until the temperature falls below the reset threshold. This prevents rapid on/off flapping around the cutoff point.

If the alternator temperature sensor is missing or invalid, field output is also forced off. DS18B20 invalid readings such as `85°C` and `-127°C` should be treated as sensor failures.

The web API exposes:

- `hard_temp_cutoff_latched`
- `alternator_over_temp`
- `hard_temp_duty_cap`
- `hard_cutoff_temp_c`
- `hard_cutoff_reset_temp_c`

The web settings API accepts:

- `hardCutoffTemp`
- `hardCutoffResetTemp`

## Cerbo SOC Inhibit

The controller can optionally inhibit alternator field output based on battery SOC received from the Cerbo/GX over NMEA2000/CAN.

Default behavior:

```text
SOC inhibit enabled: yes
Disable field at or above: 95%
Re-enable field at or below: 90%
```

This is not a charge-stage controller. The Cerbo/DCVV system still owns charging policy. This feature is only a safety/anti-overcharge guard that prevents the alternator from continuing to push current into an already-full bank.

### CAN Source

The firmware listens for:

```text
PGN 127506 - DC Detailed Status
```

The firmware reads State of Charge from byte 3 when the value is 0-100. Values outside that range are ignored as unavailable/invalid.

### Runtime Behavior

When SOC inhibit is enabled:

```text
SOC >= high threshold -> latch SOC inhibit and disable field
SOC <= low threshold  -> clear SOC inhibit and allow regulation again
```

The latch prevents rapid on/off cycling near the threshold.

If SOC data is not available, the SOC inhibit does not trip. Other safety checks still apply: INA226 presence, alternator temp, CAN/RPM requirement, current limit, and hard temp cutoff.

### Web UI / API Fields

`GET /api/status` includes:

```json
{
  "cerbo_soc": 94,
  "cerbo_soc_valid": true,
  "soc_inhibit_latched": false,
  "last_soc_ms_ago": 250
}
```

`GET /api/config` includes:

```json
{
  "socInhibitEnabled": true,
  "socInhibitHighPercent": 95,
  "socInhibitLowPercent": 90
}
```

`POST /api/config` accepts those same fields.
