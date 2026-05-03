const api = {
  status: "/api/status",
  config: "/api/config",
  enable: "/api/enable",
  log: "/api/log",
  network: "/api/network",
  networkReset: "/api/network/reset",
  reboot: "/api/reboot",
};

const el = {
  voltage: document.getElementById("voltage"),
  current: document.getElementById("current"),
  shunt_mv: document.getElementById("shunt_mv"),
  field_pwm: document.getElementById("field_pwm"),
  requested_pwm: document.getElementById("requested_pwm"),
  pid_requested_pwm: document.getElementById("pid_requested_pwm"),
  temp_duty_cap: document.getElementById("temp_duty_cap"),
  current_duty_cap: document.getElementById("current_duty_cap"),
  soft_start_duty_cap: document.getElementById("soft_start_duty_cap"),
  alt_temp: document.getElementById("alt_temp"),
  engine_room_temp: document.getElementById("engine_room_temp"),
  rpm: document.getElementById("rpm"),
  stage: document.getElementById("stage"),
  startup_status: document.getElementById("startup_status"),
  ina226_status: document.getElementById("ina226_status"),
  can_status: document.getElementById("can_status"),
  last_can: document.getElementById("last_can"),
  bms_permission: document.getElementById("bms_permission"),
  field_enabled: document.getElementById("field_enabled"),
  last_disable_reason: document.getElementById("last_disable_reason"),
  enableToggle: document.getElementById("enableToggle"),
  enableStatusLabel: document.getElementById("enableStatusLabel"),
  networkSummary: document.getElementById("networkSummary"),
  targetVoltage: document.getElementById("targetVoltage"),
  currentLimit: document.getElementById("currentLimit"),
  pidKp: document.getElementById("pidKp"),
  pidKi: document.getElementById("pidKi"),
  pidKd: document.getElementById("pidKd"),
  derateStartTemp: document.getElementById("derateStartTemp"),
  derateStopTemp: document.getElementById("derateStopTemp"),
  softStartSeconds: document.getElementById("softStartSeconds"),
  minStartRPM: document.getElementById("minStartRPM"),
  rpmHoldSeconds: document.getElementById("rpmHoldSeconds"),
  canInput: document.getElementById("canInput"),
  ssid: document.getElementById("ssid"),
  password: document.getElementById("password"),
  logOutput: document.getElementById("logOutput"),
};

function toast(message, type = "ok") {
  const root = document.getElementById("toastRoot");
  const node = document.createElement("div");
  node.className = `toast ${type}`;
  node.textContent = message;
  root.appendChild(node);
  setTimeout(() => {
    node.classList.add("hide");
    setTimeout(() => node.remove(), 300);
  }, 2400);
}

async function apiJson(url, options = {}) {
  const response = await fetch(url, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });

  if (!response.ok) {
    const text = await response.text();
    throw new Error(text || `HTTP ${response.status}`);
  }

  return response.json();
}

function formatVolts(v) {
  return v == null ? "—" : `${Number(v).toFixed(2)} V`;
}

function formatAmps(a) {
  return a == null ? "—" : `${Number(a).toFixed(1)} A`;
}

function formatTemp(c, f) {
  return c == null ? "—" : `${Number(c).toFixed(1)} °C / ${Number(f).toFixed(0)} °F`;
}

function formatMsAgo(ms) {
  if (ms == null) return "—";
  if (ms < 1000) return `${ms} ms ago`;
  return `${(ms / 1000).toFixed(1)} s ago`;
}

function renderStatus(data) {
  el.voltage.textContent = formatVolts(data.voltage);
  el.current.textContent = formatAmps(data.current);
  el.shunt_mv.textContent = data.shunt_mv == null ? "—" : `${Number(data.shunt_mv).toFixed(4)} mV`;
  el.field_pwm.textContent = `${data.pwm ?? 0}`;
  el.requested_pwm.textContent = `${data.requested_pwm ?? 0}`;
  el.pid_requested_pwm.textContent = `${data.pid_requested_pwm ?? 0}`;
  el.temp_duty_cap.textContent = `${data.temp_duty_cap ?? 0}`;
  el.current_duty_cap.textContent = `${data.current_duty_cap ?? 0}`;
  el.soft_start_duty_cap.textContent = `${data.soft_start_duty_cap ?? 0}`;
  el.alt_temp.textContent = formatTemp(data.alt_temp_c, data.alt_temp_f);
  el.engine_room_temp.textContent = formatTemp(data.engine_room_temp_c, data.engine_room_temp_f);
  el.rpm.textContent = data.rpm == null ? "—" : `${Number(data.rpm).toFixed(0)}`;
  el.stage.textContent = data.stage || "—";
  el.startup_status.textContent = data.startup_status || (data.startup_check_ok ? "OK" : "Fault");
  el.ina226_status.textContent = data.ina226_available ? "OK" : "Missing";
  el.can_status.textContent = data.can_status || "—";
  el.last_can.textContent = formatMsAgo(data.last_can_ms_ago);
  el.bms_permission.textContent = data.bms_permission ? "Allowed" : "Blocked";
  el.field_enabled.textContent = data.field_enabled ? "Yes" : "No";
  el.last_disable_reason.textContent = data.last_disable_reason || "—";
  el.enableToggle.checked = !!data.enabled;
  el.enableStatusLabel.textContent = data.enabled ? "Enabled" : "Disabled";
  el.networkSummary.textContent = `${data.network_mode || "unknown"} • ${data.ip_address || "—"}`;
  document.body.dataset.enabled = data.enabled ? "true" : "false";
}

function renderConfig(data) {
  el.targetVoltage.value = data.targetVoltage ?? "";
  el.currentLimit.value = data.currentLimit ?? "";
  el.pidKp.value = data.pidKp ?? "";
  el.pidKi.value = data.pidKi ?? "";
  el.pidKd.value = data.pidKd ?? "";
  el.derateStartTemp.value = data.derateStartTemp ?? "";
  el.derateStopTemp.value = data.derateStopTemp ?? "";
  el.softStartSeconds.value = data.softStartSeconds ?? "";
  el.minStartRPM.value = data.minStartRPM ?? "";
  el.rpmHoldSeconds.value = data.rpmHoldSeconds ?? "";
  el.canInput.checked = !!data.canInput;
  el.ssid.value = data.ssid ?? "";
  el.password.value = data.password ?? "";
}

function renderLogs(logs) {
  if (!Array.isArray(logs) || logs.length === 0) {
    el.logOutput.textContent = "No logs yet.";
    return;
  }

  el.logOutput.textContent = logs.map((row) => {
    const bits = [`t=${row.time}`, row.event];
    if (row.voltage != null) bits.push(`V=${Number(row.voltage).toFixed(2)}`);
    if (row.current != null) bits.push(`A=${Number(row.current).toFixed(1)}`);
    if (row.temp != null) bits.push(`T=${Number(row.temp).toFixed(1)}C`);
    return bits.join(" | ");
  }).join("\n");
}

async function loadStatus() {
  renderStatus(await apiJson(api.status));
}

async function loadConfig() {
  renderConfig(await apiJson(api.config));
}

async function loadLogs() {
  renderLogs(await apiJson(api.log));
}

async function saveConfig() {
  await apiJson(api.config, {
    method: "POST",
    body: JSON.stringify({
      targetVoltage: Number(el.targetVoltage.value),
      currentLimit: Number(el.currentLimit.value),
      pidKp: Number(el.pidKp.value),
      pidKi: Number(el.pidKi.value),
      pidKd: Number(el.pidKd.value),
      derateStartTemp: Number(el.derateStartTemp.value),
      derateStopTemp: Number(el.derateStopTemp.value),
      softStartSeconds: Number(el.softStartSeconds.value),
      minStartRPM: Number(el.minStartRPM.value),
      rpmHoldSeconds: Number(el.rpmHoldSeconds.value),
      canInput: el.canInput.checked,
    }),
  });
  toast("Configuration saved");
}

async function saveNetwork() {
  await apiJson(api.network, {
    method: "POST",
    body: JSON.stringify({
      wifiSSID: el.ssid.value,
      wifiPassword: el.password.value,
    }),
  });
  toast("Network saved. Rebooting…");
}

async function setEnabled(enabled) {
  await apiJson(api.enable, {
    method: "POST",
    body: JSON.stringify({ enabled }),
  });
  toast(enabled ? "Controller enabled" : "Controller disabled", enabled ? "ok" : "warn");
}

async function rebootController() {
  await apiJson(api.reboot, { method: "POST" });
  toast("Rebooting…", "warn");
}

async function resetNetwork() {
  await apiJson(api.networkReset, { method: "POST" });
  toast("Resetting network…", "warn");
}

function wireEvents() {
  document.getElementById("saveConfig").addEventListener("click", async () => {
    try {
      await saveConfig();
    } catch (err) {
      console.error(err);
      toast("Failed to save configuration", "error");
    }
  });

  document.getElementById("submitNetwork").addEventListener("click", async () => {
    try {
      await saveNetwork();
    } catch (err) {
      console.error(err);
      toast("Failed to save network", "error");
    }
  });

  document.getElementById("resetNetwork").addEventListener("click", async () => {
    try {
      await resetNetwork();
    } catch (err) {
      console.error(err);
      toast("Failed to reset network", "error");
    }
  });

  document.getElementById("rebootBtn").addEventListener("click", async () => {
    try {
      await rebootController();
    } catch (err) {
      console.error(err);
      toast("Failed to reboot", "error");
    }
  });

  document.getElementById("refreshBtn").addEventListener("click", async () => {
    try {
      await loadStatus();
    } catch (err) {
      console.error(err);
      toast("Failed to refresh status", "error");
    }
  });

  document.getElementById("refreshLogs").addEventListener("click", async () => {
    try {
      await loadLogs();
    } catch (err) {
      console.error(err);
      toast("Failed to load logs", "error");
    }
  });

  el.enableToggle.addEventListener("change", async (event) => {
    try {
      await setEnabled(event.target.checked);
      await loadStatus();
    } catch (err) {
      console.error(err);
      toast("Failed to change enable state", "error");
      event.target.checked = !event.target.checked;
    }
  });
}

async function boot() {
  wireEvents();

  try {
    await Promise.all([loadConfig(), loadStatus(), loadLogs()]);
  } catch (err) {
    console.error(err);
    toast("Initial load failed", "error");
  }

  setInterval(() => {
    loadStatus().catch((err) => {
      console.error(err);
    });
  }, 1000);

  setInterval(() => {
    loadLogs().catch((err) => {
      console.error(err);
    });
  }, 5000);
}

window.addEventListener("load", boot);
