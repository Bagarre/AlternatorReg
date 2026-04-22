# 🧪 ESP32 Alternator Simulator Harness (Phase 2)

## Overview

This project defines a **software-based alternator simulator** used to test the ESP32 alternator regulator without running the engine.

The goal is **not perfect physics**, but a **controllable, repeatable system** that exposes control logic issues such as:

- thermal overshoot
- oscillation / hunting
- slow response
- instability under changing conditions
- bad edge-case handling

---

## 🎯 Design Philosophy

> “Good enough physics + great control stress = useful simulator”

We are simulating:
- trends
- lag
- limits
- interactions

We are NOT simulating:
- exact electromagnetic behavior
- real alternator efficiency curves

---

## 🧱 System Architecture

### Device Under Test (DUT)
- Your alternator regulator ESP32
- Outputs:
  - Field PWM
- Inputs:
  - Voltage
  - Current
  - Temp
  - CAN (RPM, BMS)

---

### Simulator ESP32

Acts as:
- Alternator
- Battery
- Engine
- CAN source
- Sensor provider
- UI controller

---

## 🔌 Signals

### From DUT → Simulator
- Field PWM (GPIO input)
  - Measure duty cycle
  - Primary control input to simulator

---

### From Simulator → DUT

Simulated inputs:
- Alternator current
- Battery voltage
- Alternator temperature
- CAN messages (RPM, etc.)
- BMS permission (optional)

---

## ⚙️ Core Simulation Model

### 1. Current Model

```
available_current = pwm_duty * rpm_factor * alternator_scale
accepted_current  = battery_acceptance_curve
charge_current    = min(available_current, accepted_current)
```

---

### 2. Thermal Model

```
heat_generated = k1 * current
cooling = k2 * (temp - ambient) * rpm_factor
temp_rate = heat_generated - cooling
temp += temp_rate * dt
```

---

### 3. Thermal Lag

```
temp += (target_temp - temp) * lag_factor
```

---

## 🔑 Break-Even Current

```
heat_generated == cooling
```

At that point:
- temp stabilizes
- no runaway
- no cooling

---

## 🎛️ Adjustable Parameters (UI Controls)

### Engine / Environment
- RPM
- Ambient temp
- Engine room temp

### Alternator Behavior
- Max current scale
- Cooling factor
- Thermal lag factor
- Break-even shift

### Battery Behavior
- SOC (0–100%)
- Acceptance curve aggressiveness

### Fault Injection
- CAN timeout
- BMS deny
- Temp sensor failure
- Current sensor failure
- RPM spikes / drops

---

## 🖥️ UI Requirements

### Controls Panel
- RPM slider
- SOC slider
- Ambient temp
- Cooling factor
- Lag factor
- Toggle faults

### Live Status
- Field duty %
- Simulated current
- Simulated voltage
- Simulated temp
- Temp slope
- DUT state

### Scenario Buttons
- Cold start
- Hot restart
- RPM drop
- Overtemp ramp
- CAN loss
- Battery full event

---

## 📈 Logging

Log:
- time
- RPM
- PWM duty
- current
- temp
- temp slope

---

## 🔄 Future Enhancements

### Phase 3
- Record & replay real-world logs

### Phase 4
- Learning model (RPM → current table)

---

## ⚠️ Important Notes

- Simulator should be stable but adjustable
- Avoid overfitting physics
- Prioritize repeatability

---

## 🧠 Key Insight

> You are not simulating an alternator  
> You are testing a control system

---

## ✅ Success Criteria

You should be able to:
- make the controller hunt
- make it overshoot
- reproduce scenarios

---

## 🚀 Development Order

1. Read PWM
2. Current model
3. Thermal model
4. UI sliders
5. Fault injection
6. Logging

---

## 🧩 Minimal First Version

Start with:
- PWM input
- RPM slider
- simple current + temp model
- basic web UI
