function showBanner(message, type = "info") {
  const banner = document.createElement("div");
  banner.textContent = message;
  banner.style.position = "fixed";
  banner.style.top = "1rem";
  banner.style.left = "50%";
  banner.style.transform = "translateX(-50%)";
  banner.style.fontSize = "1rem";
  banner.style.textAlign = "center";
  banner.style.maxWidth = "90%";
  banner.style.padding = "0.5rem 1rem";
  banner.style.borderRadius = "0.5rem";
  banner.style.boxShadow = "0 2px 4px rgba(0, 0, 0, 0.2)";
  banner.style.color = "white";
  banner.style.zIndex = 9999;
  banner.style.transition = "opacity 0.3s ease";
  banner.style.opacity = 1;
  banner.style.backgroundColor = type === "error" ? "#dc2626" : "#16a34a";

  document.body.appendChild(banner);

  setTimeout(() => {
    banner.style.opacity = 0;
    setTimeout(() => banner.remove(), 1000);
  }, 3000);
};

function updateHeaderState(enabled) {
  const header = document.querySelector("header");
  if (header) {
    header.style.backgroundColor = enabled ? "#2563eb" : "#4b5563"; // blue or gray
    document.getElementById("enableStatus").textContent = enabled ? "Status: Enabled" : "Status: Disabled";
  }
}

function fetchStatus() {
  fetch("/status")
  .then(res => res.json())
  .then(data => {
    document.getElementById("voltage").textContent = data.voltage || "—";
    document.getElementById("current").textContent = data.current || "—";
    document.getElementById("field_pwm").textContent = data.pwm || "—";
    document.getElementById("alt_temp").textContent = data.alt_temp || "—";
    document.getElementById("batt_temp").textContent = data.batt_temp || "—";
    document.getElementById("rpm").textContent = data.rpm || "—";
    document.getElementById("stage").textContent = data.stage || "—";
    document.getElementById("can_status").textContent = data.can_status || "—";
    document.getElementById("last_can").textContent = data.last_can || "—";
    document.getElementById("bms_permission").textContent = data.bms_permission ? "Yes" : "No";
  })
  .catch(err => {
    console.error("Failed to fetch status:", err);
    showBanner("Failed to fetch status", "error");
  });
}

window.onload = () => {
  setInterval(fetchStatus, 5000);

  fetchStatus();

  fetch("/config")
    .then(res => res.json())
    .then(cfg => {
      document.getElementById("targetVoltage").value = cfg.targetVoltage || "";
      document.getElementById("currentLimit").value = cfg.currentLimit || "";
      document.getElementById("floatVoltage").value = cfg.floatVoltage || "";
      document.getElementById("derateTemp").value = cfg.derateTemp || "";
      document.getElementById("canInput").checked = cfg.canInput === true;
      document.getElementById("ssid").value = cfg.ssid || "";
      document.getElementById("password").value = cfg.password || "";

    })
    .catch(err => {
      console.error("Failed to load config:", err);
      showBanner("Failed to load configuration", "error");
    });

  fetch("/log")
    .then(res => res.text())
    .then(log => {
      document.getElementById("logOutput").textContent = log;
    })
    .catch(err => {
      document.getElementById("logOutput").textContent = "Error loading logs.";
      console.error("Log fetch failed", err);
      showBanner("Error loading logs", "error");
    });

  fetch("/enable")
    .then(res => res.json())
    .then(data => {
      const enabled = data.enabled === true;
      document.getElementById("enableToggle").checked = enabled;
      updateHeaderState(enabled);
    })
    .catch(err => {
      console.error("Failed to fetch enable state:", err);
      showBanner("Failed to fetch enable state", "error");
    });

  document.getElementById("enableToggle").addEventListener("change", (e) => {
    const newState = e.target.checked;
    fetch("/enable", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: newState })
    })
      .then(() => {
        updateHeaderState(newState);
        showBanner(newState ? "✅ Enabled" : "⚠️ Disabled", newState ? "success" : "error");
      })
      .catch(err => {
        console.error("Enable toggle failed", err);
        showBanner("Failed to change enable state", "error");
      });
  });



  document.getElementById("saveConfig").addEventListener("click", () => {
    const config = {
      targetVoltage: document.getElementById("targetVoltage").value,
      currentLimit: document.getElementById("currentLimit").value,
      floatVoltage: document.getElementById("floatVoltage").value,
      derateTemp: document.getElementById("derateTemp").value,
      canInput: document.getElementById("canInput").checked
    };

    fetch("/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(config)
    })
      .then(res => res.text())
      .then(text => {
        console.log("Config saved:", text);
        showBanner("✅ Config saved successfully", "success");
      })
      .catch(err => {
        console.error("Save failed:", err);
        showBanner("❌ Failed to save config", "error");
      });
  });

  document.getElementById("submitNetwork").addEventListener("click", () => {
    const network = {
      wifiSSID: document.getElementById("ssid").value,
      wifiPassword: document.getElementById("password").value
    };

    fetch("/network", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(network)
    })
      .then(() => {
        showBanner("✅ Network settings saved — rebooting...", "success");
      })
      .catch(err => {
        console.error("Network save failed:", err);
        showBanner("❌ Failed to save network settings", "error");
      });
  });
};
