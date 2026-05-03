const api = {
  status: "/api/status",
  config: "/api/config",
  enable: "/api/enable",
  log: "/api/log",
  network: "/api/network",
  networkReset: "/api/network/reset",
  reboot: "/api/reboot",
};

const $ = (id) => document.getElementById(id);

const el = {
  voltage: $("voltage"), current: $("current"), shunt_mv: $("shunt_mv"),
  field_pwm: $("field_pwm"), requested_pwm: $("requested_pwm"), pid_requested_pwm: $("pid_requested_pwm"),
  temp_duty_cap: $("temp_duty_cap"), current_duty_cap: $("current_duty_cap"), soft_start_duty_cap: $("soft_start_duty_cap"), hard_temp_cutoff_latched: $("hard_temp_cutoff_latched"),
  alt_temp: $("alt_temp"), engine_room_temp: $("engine_room_temp"), rpm: $("rpm"), stage: $("stage"),
  startup_status: $("startup_status"), ina226_status: $("ina226_status"), can_status: $("can_status"), last_can: $("last_can"),
  bms_permission: $("bms_permission"), cerbo_soc: $("cerbo_soc"), soc_inhibit_latched: $("soc_inhibit_latched"), field_enabled: $("field_enabled"), last_disable_reason: $("last_disable_reason"),
  enabledText: $("enabledText"), enableButton: $("enableButton"), enableBadge: $("enableBadge"),
  networkSummary: $("networkSummary"), overallStatus: $("overallStatus"), canPill: $("canPill"), startupBadge: $("startupBadge"),
  inaIndicator: $("inaIndicator"), altTempIndicator: $("altTempIndicator"), engineTempIndicator: $("engineTempIndicator"), canIndicator: $("canIndicator"), startupIndicator: $("startupIndicator"),
  maintenance_mode: $("maintenance_mode"), network_mode: $("network_mode"), ip_address: $("ip_address"), last_soc: $("last_soc"),
  targetVoltage: $("targetVoltage"), currentLimit: $("currentLimit"), pidKp: $("pidKp"), pidKi: $("pidKi"), pidKd: $("pidKd"),
  derateStartTemp: $("derateStartTemp"), derateStopTemp: $("derateStopTemp"), hardCutoffTemp: $("hardCutoffTemp"), hardCutoffResetTemp: $("hardCutoffResetTemp"), softStartSeconds: $("softStartSeconds"),
  minStartRPM: $("minStartRPM"), rpmHoldSeconds: $("rpmHoldSeconds"), socInhibitEnabled: $("socInhibitEnabled"), socInhibitHighPercent: $("socInhibitHighPercent"), socInhibitLowPercent: $("socInhibitLowPercent"), canInput: $("canInput"),
  ssid: $("ssid"), password: $("password"), logOutput: $("logOutput"),
};

let lastStatus = null;

function toast(message, type = "ok") {
  const root = $("toastRoot");
  const node = document.createElement("div");
  node.className = `toast ${type}`;
  node.textContent = message;
  root.appendChild(node);
  setTimeout(() => { node.classList.add("hide"); setTimeout(() => node.remove(), 300); }, 2400);
}

async function apiJson(url, options = {}) {
  const response = await fetch(url, { headers: { "Content-Type": "application/json" }, ...options });
  if (!response.ok) throw new Error(await response.text() || `HTTP ${response.status}`);
  return response.json();
}

const dash = (v) => (v === null || v === undefined || Number.isNaN(v) ? "--" : v);
const fmt = (v, digits, unit="") => (v === null || v === undefined || Number.isNaN(Number(v)) ? "--" : `${Number(v).toFixed(digits)}${unit}`);
const boolText = (v, yes="yes", no="no") => v ? yes : no;

function formatMsAgo(ms) {
  if (ms == null) return "--";
  if (ms < 1000) return `${ms} ms ago`;
  return `${(ms / 1000).toFixed(1)} s ago`;
}

function setClass(node, state) {
  node.classList.remove("ok", "warn", "bad");
  if (state) node.classList.add(state);
}

function tempText(c, f) {
  if (c == null) return "--";
  return `${Number(c).toFixed(1)}°C`;
}

function renderStatus(data) {
  lastStatus = data;
  const n2kOk = (data.can_status || "").toLowerCase().includes("ok") || data.last_can_ms_ago != null;
  const startupOk = !!data.startup_check_ok;
  const anyHardFault = !data.ina226_available || !n2kOk || data.startup_check_ok === false || !!data.hard_temp_cutoff_latched || !!data.soc_inhibit_latched;
  const anyWarn = !data.alt_temp_sensor_ok || !data.engine_room_temp_sensor_ok;

  el.voltage.textContent = fmt(data.voltage, 2, "v");
  el.current.textContent = fmt(data.current, 1, "a");
  el.shunt_mv.textContent = fmt(data.shunt_mv, 4, " mV");
  el.field_pwm.textContent = `${data.pwm ?? 0}`;
  el.requested_pwm.textContent = `${data.requested_pwm ?? 0}`;
  el.pid_requested_pwm.textContent = `${data.pid_requested_pwm ?? 0}`;
  el.temp_duty_cap.textContent = `${data.temp_duty_cap ?? 0}`;
  el.current_duty_cap.textContent = `${data.current_duty_cap ?? 0}`;
  el.soft_start_duty_cap.textContent = `${data.soft_start_duty_cap ?? 0}`;
  if (el.hard_temp_cutoff_latched) el.hard_temp_cutoff_latched.textContent = data.hard_temp_cutoff_latched ? "TRIPPED" : "clear";
  el.alt_temp.textContent = tempText(data.alt_temp_c, data.alt_temp_f);
  el.engine_room_temp.textContent = tempText(data.engine_room_temp_c, data.engine_room_temp_f);
  el.rpm.textContent = data.rpm == null ? "--" : `${Number(data.rpm).toFixed(0)}`;
  el.stage.textContent = data.stage || "--";
  el.startup_status.textContent = data.startup_status || (startupOk ? "OK" : "Fault");
  el.ina226_status.textContent = data.ina226_available ? "OK" : "Missing";
  el.can_status.textContent = data.can_status || "--";
  el.last_can.textContent = formatMsAgo(data.last_can_ms_ago);
  el.bms_permission.textContent = boolText(data.bms_permission, "allowed", "blocked");
  if (el.cerbo_soc) el.cerbo_soc.textContent = data.cerbo_soc_valid ? fmt(data.cerbo_soc, 0, "%") : "--";
  if (el.soc_inhibit_latched) el.soc_inhibit_latched.textContent = data.soc_inhibit_latched ? "ACTIVE" : "clear";
  el.field_enabled.textContent = boolText(data.field_enabled, "yes", "no");
  el.last_disable_reason.textContent = data.last_disable_reason || "--";
  el.enabledText.textContent = boolText(data.enabled, "enabled", "disabled");
  el.maintenance_mode.textContent = boolText(data.maintenance_mode, "on", "off");
  el.network_mode.textContent = data.network_mode || "--";
  el.ip_address.textContent = data.ip_address || "--";
  if (el.last_soc) el.last_soc.textContent = formatMsAgo(data.last_soc_ms_ago);

  el.enableButton.textContent = data.enabled ? "Enabled" : "Disabled";
  el.enableButton.classList.toggle("disabled", !data.enabled);
  el.enableBadge.textContent = data.enabled ? "ON" : "OFF";
  setClass(el.enableBadge, data.enabled ? "ok" : "");

  const overall = anyHardFault ? "error" : anyWarn ? "warning" : data.enabled ? "online" : "disabled";
  el.overallStatus.textContent = overall;
  setClass(el.overallStatus, anyHardFault ? "bad" : anyWarn ? "warn" : "ok");

  el.canPill.textContent = n2kOk ? "CAN online" : "CAN offline";
  setClass(el.canPill, n2kOk ? "ok" : "bad");
  el.startupBadge.textContent = startupOk ? "startup OK" : "startup fault";
  setClass(el.startupBadge, startupOk ? "ok" : "bad");
  el.networkSummary.textContent = `${data.network_mode || "unknown"} · ${data.ip_address || "no IP"}`;

  setClass(el.inaIndicator, data.ina226_available ? "ok" : "bad");
  setClass(el.altTempIndicator, data.alt_temp_sensor_ok ? "ok" : "warn");
  setClass(el.engineTempIndicator, data.engine_room_temp_sensor_ok ? "ok" : "warn");
  setClass(el.canIndicator, n2kOk ? "ok" : "bad");
  setClass(el.startupIndicator, startupOk ? "ok" : "bad");
}

function renderConfig(data) {
  el.targetVoltage.value = data.targetVoltage ?? "";
  el.currentLimit.value = data.currentLimit ?? "";
  el.pidKp.value = data.pidKp ?? "";
  el.pidKi.value = data.pidKi ?? "";
  el.pidKd.value = data.pidKd ?? "";
  el.derateStartTemp.value = data.derateStartTemp ?? "";
  el.derateStopTemp.value = data.derateStopTemp ?? "";
  if (el.hardCutoffTemp) el.hardCutoffTemp.value = data.hardCutoffTemp ?? "";
  if (el.hardCutoffResetTemp) el.hardCutoffResetTemp.value = data.hardCutoffResetTemp ?? "";
  if (el.socInhibitEnabled) el.socInhibitEnabled.checked = !!data.socInhibitEnabled;
  if (el.socInhibitHighPercent) el.socInhibitHighPercent.value = data.socInhibitHighPercent ?? "";
  if (el.socInhibitLowPercent) el.socInhibitLowPercent.value = data.socInhibitLowPercent ?? "";
  el.softStartSeconds.value = data.softStartSeconds ?? "";
  el.minStartRPM.value = data.minStartRPM ?? "";
  el.rpmHoldSeconds.value = data.rpmHoldSeconds ?? "";
  el.canInput.checked = !!data.canInput;
  el.ssid.value = data.ssid ?? "";
  el.password.value = data.password ?? "";
}

function renderLogs(logs) {
  if (!Array.isArray(logs) || logs.length === 0) { el.logOutput.textContent = "No logs yet."; return; }
  el.logOutput.textContent = logs.map((row) => {
    const bits = [`t=${row.time}`, row.event];
    if (row.voltage != null) bits.push(`V=${Number(row.voltage).toFixed(2)}`);
    if (row.current != null) bits.push(`A=${Number(row.current).toFixed(1)}`);
    if (row.temp != null) bits.push(`T=${Number(row.temp).toFixed(1)}C`);
    return bits.join(" | ");
  }).join("\n");
}

async function loadStatus() { renderStatus(await apiJson(api.status)); }
async function loadConfig() { renderConfig(await apiJson(api.config)); }
async function loadLogs() { renderLogs(await apiJson(api.log)); }

async function saveConfig() {
  await apiJson(api.config, { method: "POST", body: JSON.stringify({
    targetVoltage: Number(el.targetVoltage.value), currentLimit: Number(el.currentLimit.value),
    pidKp: Number(el.pidKp.value), pidKi: Number(el.pidKi.value), pidKd: Number(el.pidKd.value),
    derateStartTemp: Number(el.derateStartTemp.value), derateStopTemp: Number(el.derateStopTemp.value),
    hardCutoffTemp: Number(el.hardCutoffTemp?.value ?? 105), hardCutoffResetTemp: Number(el.hardCutoffResetTemp?.value ?? 95),
    socInhibitEnabled: !!el.socInhibitEnabled?.checked, socInhibitHighPercent: Number(el.socInhibitHighPercent?.value ?? 95), socInhibitLowPercent: Number(el.socInhibitLowPercent?.value ?? 90),
    softStartSeconds: Number(el.softStartSeconds.value), minStartRPM: Number(el.minStartRPM.value),
    rpmHoldSeconds: Number(el.rpmHoldSeconds.value), canInput: el.canInput.checked,
  })});
  toast("Settings saved");
}

async function saveNetwork() {
  await apiJson(api.network, { method: "POST", body: JSON.stringify({ wifiSSID: el.ssid.value, wifiPassword: el.password.value }) });
  toast("Network saved. Rebooting…", "warn");
}

async function setEnabled(enabled) {
  await apiJson(api.enable, { method: "POST", body: JSON.stringify({ enabled }) });
  toast(enabled ? "Controller enabled" : "Controller disabled", enabled ? "ok" : "warn");
}

async function rebootController() { await apiJson(api.reboot, { method: "POST" }); toast("Rebooting…", "warn"); }
async function resetNetwork() { await apiJson(api.networkReset, { method: "POST" }); toast("Resetting network…", "warn"); }

function bind(id, event, fn) { const n = $(id); if (n) n.addEventListener(event, fn); }
function wireEvents() {
  bind("saveConfig", "click", async () => { try { await saveConfig(); } catch(e) { console.error(e); toast("Failed to save settings", "error"); } });
  bind("submitNetwork", "click", async () => { try { await saveNetwork(); } catch(e) { console.error(e); toast("Failed to save network", "error"); } });
  bind("resetNetwork", "click", async () => { try { await resetNetwork(); } catch(e) { console.error(e); toast("Failed to reset network", "error"); } });
  bind("rebootBtn", "click", async () => { try { await rebootController(); } catch(e) { console.error(e); toast("Failed to reboot", "error"); } });
  bind("refreshBtn", "click", async () => { try { await loadStatus(); } catch(e) { console.error(e); toast("Failed to refresh", "error"); } });
  bind("refreshLogs", "click", async () => { try { await loadLogs(); } catch(e) { console.error(e); toast("Failed to load logs", "error"); } });
  bind("enableButton", "click", async () => { try { await setEnabled(!lastStatus?.enabled); await loadStatus(); } catch(e) { console.error(e); toast("Failed to change enable state", "error"); } });
}

async function boot() {
  wireEvents();
  try { await Promise.all([loadConfig(), loadStatus(), loadLogs()]); } catch(e) { console.error(e); toast("Initial load failed", "error"); }
  setInterval(() => loadStatus().catch(console.error), 1000);
  setInterval(() => loadLogs().catch(console.error), 5000);
}

window.addEventListener("load", boot);
