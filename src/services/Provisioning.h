#pragma once
#include <Arduino.h>

// services/Provisioning — first-boot WiFi via WiFiManager captive portal
// (spec §3.1, §13). No hardcoded creds: if none are stored it opens an AP +
// captive portal and blocks until the user joins, then persists creds in NVS.
// Returns true once connected. This runs BEFORE the async WebPortal starts.
class Provisioning {
public:
  // apName: SoftAP SSID for the config portal. timeoutSec: portal idle timeout.
  bool begin(const String& apName, uint16_t timeoutSec = 180);
};
