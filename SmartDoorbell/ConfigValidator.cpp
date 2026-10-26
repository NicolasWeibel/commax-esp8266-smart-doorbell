#include "ConfigValidator.h"

#include <cstring>

#include "Secrets.h"

namespace {
bool missingOrPlaceholder(const char* value) {
  return value == nullptr || value[0] == '\0' ||
         std::strncmp(value, "YOUR_", 5U) == 0;
}

bool startsWithHttps(const char* value) {
  return value != nullptr && std::strncmp(value, "https://", 8U) == 0;
}
}

bool ConfigValidator::valid() {
  return !missingOrPlaceholder(Secrets::kWifiSsid) &&
         !missingOrPlaceholder(Secrets::kWifiPassword) &&
         !missingOrPlaceholder(Secrets::kNtfyTopic) &&
         startsWithHttps(Secrets::kNtfyBaseUrl) &&
         Secrets::kTimeZone[0] != '\0';
}
