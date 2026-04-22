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
  field_pwm: document.getElementById("field_pwm"),
  requested_pwm: document.getElementById("requested_pwm"),
  alt_temp: document.getElementById("alt_temp"),
  rpm: document.getElementById("rpm"),
  stage: document.getElementById("stage"),
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
  floatVoltage: document.getElementById("floatVoltage"),
  derateTemp: document.getElementById("derateTemp"),
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
  el.field_pwm.textContent = `${data.pwm ?? 0}`;
  el.requested_pwm.textContent = `${data.requested_pwm ?? 0}`;
  el.alt_temp.textContent = formatTemp(data.alt_temp_c, data.alt_temp_f);
  el.rpm.textContent = data.rpm == null ? "—" : `${Number(data.rpm).toFixed(0)}`;
  el.stage.textContent = data.stage || "—";
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
  el.floatVoltage.value = data.floatVoltage ?? "";
  el.derateTemp.value = data.derateTemp ?? "";
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
      floatVoltage: Number(el.floatVoltage.value),
      derateTemp: Number(el.derateTemp.value),
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
