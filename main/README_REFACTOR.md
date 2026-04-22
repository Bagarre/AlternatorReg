# Alternator Regulator Refactor

This is a drop-in project layout intended to replace the monolithic sketch with a cleaner structure:

- `main.ino` becomes thin orchestration only
- `/data` continues serving static HTML/CSS/JS from SPIFFS
- JSON endpoints are standardized under `/api/*`
- status values come from live state instead of simulated values by default
- field PWM control is centralized in `control_loop.cpp`

## Notes
- This rewrite assumes the same basic hardware:
  - ESP32
  - INA260
  - TWAI/CAN transceiver
  - AsyncWebServer
- Current control is conservative and intentionally simple.
- CAN parsing now extracts PGN from the 29-bit identifier instead of comparing the raw identifier directly.
- `analogWrite()` was replaced with ESP32 LEDC PWM.
- Reboot / network reset endpoints were added.
- Logs now serialize with ArduinoJson instead of hand-building JSON.

## SPIFFS
Upload everything in `/data` as usual.

## API
- `GET /api/status`
- `GET /api/config`
- `POST /api/config`
- `GET /api/enable`
- `POST /api/enable`
- `GET /api/log`
- `POST /api/network`
- `POST /api/reboot`
- `POST /api/network/reset`
