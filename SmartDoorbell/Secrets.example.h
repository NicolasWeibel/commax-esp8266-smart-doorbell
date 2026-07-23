#pragma once

// Template for future stages. Stage 2 does not include Secrets.h and does not
// use any credentials. Copy this template to Secrets.h only when a later stage requires
// private configuration. Secrets.h is excluded by .gitignore.

#define WIFI_SSID "YOUR_WIFI_NETWORK"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define NTFY_SERVER "https://ntfy.sh"
#define NTFY_TOPIC "YOUR_PRIVATE_TOPIC"

#define IMOU_APP_ID "YOUR_APP_ID"
#define IMOU_APP_SECRET "YOUR_APP_SECRET"
#define IMOU_REGION "or"
#define IMOU_DEVICE_ID "YOUR_DEVICE_ID"
#define IMOU_CHANNEL_ID "0"
