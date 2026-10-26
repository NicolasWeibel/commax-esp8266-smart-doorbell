#pragma once

// Copy this file to Secrets.h and replace every placeholder.
// Secrets.h is intentionally ignored by Git.
namespace Secrets {

constexpr char kWifiSsid[] = "YOUR_WIFI_SSID";
constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";
constexpr char kWifiHostname[] = "commax-smart-doorbell";

constexpr char kNtfyBaseUrl[] = "https://ntfy.sh";
constexpr char kNtfyTopic[] = "YOUR_PRIVATE_RANDOM_TOPIC";

// Optional static deep-link/action. They do not call the Imou API and do not
// download an image. Empty strings disable the corresponding ntfy header.
constexpr char kNtfyClickUrl[] = "imoulife://open.webview";
constexpr char kNtfyAction[] =
    "view, Abrir Imou, imoulife://open.webview";

// Argentina: ART3. This is UTC-3 all year.
constexpr char kTimeZone[] = "ART3";

}  // namespace Secrets
