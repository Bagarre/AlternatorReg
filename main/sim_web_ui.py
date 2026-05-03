#!/usr/bin/env python3
"""
Local simulator for the Alternator Regulator web UI.

Run from the project directory:
  python3 sim_web_ui.py

Then open:
  http://127.0.0.1:8765/
"""

from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
import json
import math
import mimetypes
import random
import time

ROOT = Path(__file__).resolve().parent
DATA = ROOT / "data"
PORT = 8765

config = {
    "targetVoltage": 14.4,
    "currentLimit": 100.0,
    "pidKp": 45.0,
    "pidKi": 4.0,
    "pidKd": 0.0,
    "derateStartTemp": 82.0,
    "derateStopTemp": 96.0,
    "softStartSeconds": 60.0,
    "minStartRPM": 2200.0,
    "rpmHoldSeconds": 300.0,
    "canInput": True,
    "ssid": "",
    "password": "",
}

state = {
    "enabled": True,
    "field_enabled": True,
    "pwm": 37,
    "requested_pwm": 40,
    "pid_requested_pwm": 40,
    "temp_duty_cap": 255,
    "current_duty_cap": 255,
    "soft_start_duty_cap": 180,
    "voltage": 14.05,
    "current": 42.0,
    "shunt_mv": 4.2,
    "alt_temp_c": 61.0,
    "engine_room_temp_c": 36.0,
    "rpm": 1850,
    "stage": "Bulk",
    "can_status": "OK",
    "last_can_ms_ago": 120,
    "bms_permission": True,
    "last_disable_reason": "None",
    "network_mode": "Simulator",
    "ip_address": "127.0.0.1:8765",
    "ina226_available": True,
    "alt_temp_sensor_ok": True,
    "engine_room_temp_sensor_ok": True,
    "startup_check_complete": True,
    "startup_check_ok": True,
    "startup_status": "OK",
    "maintenance_mode": False,
}

logs = [
    {"time": 0, "event": "Simulator boot"},
    {"time": 50, "event": "Startup check OK", "voltage": 13.8, "current": 0, "temp": 35.0},
]

start = time.time()

def update_state():
    t = time.time() - start

    if state["enabled"] and state["ina226_available"] and state["alt_temp_sensor_ok"]:
        target = config["targetVoltage"]
        state["voltage"] = target - 0.25 + 0.08 * math.sin(t / 6.0) + random.uniform(-0.015, 0.015)
        state["current"] = max(0.0, min(config["currentLimit"] + 8, 45 + 20 * math.sin(t / 11.0) + random.uniform(-1, 1)))
        state["shunt_mv"] = state["current"] * 0.1
        state["pid_requested_pwm"] = int(max(0, min(255, state["pwm"] + (target - state["voltage"]) * config["pidKp"])))

        # Simulate current cap.
        if state["current"] <= config["currentLimit"]:
            state["current_duty_cap"] = 255
        else:
            state["current_duty_cap"] = int(max(0, min(255, 255 * (1 - ((state["current"] - config["currentLimit"]) / 25.0)))))

        # Simulate thermal derating curve.
        alt_temp = 62 + 4 * math.sin(t / 20.0)
        if alt_temp <= config["derateStartTemp"]:
            state["temp_duty_cap"] = 255
        elif alt_temp >= config["derateStopTemp"]:
            state["temp_duty_cap"] = 0
        else:
            ratio = (config["derateStopTemp"] - alt_temp) / (config["derateStopTemp"] - config["derateStartTemp"])
            state["temp_duty_cap"] = int(max(0, min(255, 255 * ratio)))

        state["soft_start_duty_cap"] = int(min(255, 255 * (t / max(1, config["softStartSeconds"]))))
        state["requested_pwm"] = min(state["pid_requested_pwm"], state["current_duty_cap"], state["temp_duty_cap"], state["soft_start_duty_cap"])
        state["pwm"] += int(max(-8, min(8, state["requested_pwm"] - state["pwm"])))
        state["field_enabled"] = state["pwm"] > 0
        if state["temp_duty_cap"] < 255:
            state["stage"] = "Thermal derate"
        elif state["current_duty_cap"] < 255:
            state["stage"] = "Current limit"
        elif state["soft_start_duty_cap"] < 255:
            state["stage"] = "Soft start"
        else:
            state["stage"] = "Regulating"
        state["last_disable_reason"] = "None"
    else:
        state["current"] = 0.0
        state["shunt_mv"] = 0.0
        state["requested_pwm"] = 0
        state["pwm"] = 0
        state["field_enabled"] = False
        state["stage"] = "Idle"
        if not state["enabled"]:
            state["last_disable_reason"] = "User disabled"
        elif not state["ina226_available"]:
            state["last_disable_reason"] = "INA226 missing"
        elif not state["alt_temp_sensor_ok"]:
            state["last_disable_reason"] = "Alternator temp missing"

    state["alt_temp_c"] = None if not state["alt_temp_sensor_ok"] else 62 + 4 * math.sin(t / 20.0)
    state["engine_room_temp_c"] = None if not state["engine_room_temp_sensor_ok"] else 36 + 2 * math.sin(t / 30.0)
    state["alt_temp_f"] = None if state["alt_temp_c"] is None else state["alt_temp_c"] * 1.8 + 32
    state["engine_room_temp_f"] = None if state["engine_room_temp_c"] is None else state["engine_room_temp_c"] * 1.8 + 32
    state["rpm"] = 1850 + 40 * math.sin(t / 9.0)
    state["last_can_ms_ago"] = int(90 + 30 * abs(math.sin(t))) if state["can_status"] == "OK" else None
    state["target_voltage"] = config["targetVoltage"]
    state["current_limit"] = config["currentLimit"]


def read_body(handler):
    length = int(handler.headers.get("Content-Length", "0"))
    if length <= 0:
        return {}
    return json.loads(handler.rfile.read(length).decode("utf-8"))

class Handler(BaseHTTPRequestHandler):
    def send_json(self, obj, code=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        update_state()
        if self.path == "/api/status":
            self.send_json(state)
            return
        if self.path == "/api/config":
            self.send_json(config)
            return
        if self.path == "/api/enable":
            self.send_json({"enabled": state["enabled"]})
            return
        if self.path == "/api/log":
            self.send_json(logs[-50:])
            return

        rel = self.path.split("?", 1)[0].lstrip("/") or "index.html"
        path = (DATA / rel).resolve()
        if not str(path).startswith(str(DATA.resolve())) or not path.exists():
            self.send_response(404)
            self.end_headers()
            return
        body = path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        now_ms = int((time.time() - start) * 1000)
        try:
            body = read_body(self)
        except Exception:
            self.send_json({"error": "Invalid JSON"}, 400)
            return

        if self.path == "/api/config":
            for key in ["targetVoltage", "currentLimit", "pidKp", "pidKi", "pidKd", "derateStartTemp", "derateStopTemp", "softStartSeconds", "minStartRPM", "rpmHoldSeconds", "canInput"]:
                if key in body:
                    config[key] = body[key]
            logs.append({"time": now_ms, "event": "Simulator config saved"})
            self.send_json({"status": "saved"})
            return

        if self.path == "/api/enable":
            state["enabled"] = bool(body.get("enabled", state["enabled"]))
            logs.append({"time": now_ms, "event": "Controller enabled" if state["enabled"] else "Controller disabled"})
            self.send_json({"status": "updated"})
            return

        if self.path == "/api/network":
            config["ssid"] = body.get("wifiSSID", "")
            config["password"] = body.get("wifiPassword", "")
            logs.append({"time": now_ms, "event": "Simulator network saved"})
            self.send_json({"status": "saved", "rebooting": False})
            return

        if self.path == "/api/network/reset":
            config["ssid"] = ""
            config["password"] = ""
            logs.append({"time": now_ms, "event": "Simulator network reset"})
            self.send_json({"status": "network_reset", "rebooting": False})
            return

        if self.path == "/api/reboot":
            logs.append({"time": now_ms, "event": "Simulator reboot requested"})
            self.send_json({"status": "rebooting"})
            return

        self.send_json({"error": "Not found"}, 404)

if __name__ == "__main__":
    print(f"Alternator UI simulator: http://127.0.0.1:{PORT}/")
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
