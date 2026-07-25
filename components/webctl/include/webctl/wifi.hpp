#pragma once

namespace webctl {

// Joins the configured WiFi as a station; if that fails (or no credentials
// are configured) it starts a fallback access point instead. Credentials
// saved from the web UI (NVS) take priority over the Kconfig defaults.
// Blocks until one of the two is up.
void wifi_start();

bool wifi_in_ap_mode();
const char* wifi_current_ssid();
const char* wifi_ip();

// Persists new credentials to NVS and restarts the device ~1.5 s later
// (so the HTTP response can be delivered first). An empty SSID clears the
// saved credentials.
void wifi_save_credentials_and_restart(const char* ssid, const char* pass);

}  // namespace webctl
