#include "Provisioning.h"
#include <WiFi.h>
#include <WiFiManager.h>

bool Provisioning::begin(const String& apName, uint16_t timeoutSec) {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(timeoutSec);   // don't block forever if unattended
  wm.setConnectTimeout(20);

  bool ok = wm.autoConnect(apName.c_str());
  if (ok) {
    Serial.printf("[wifi] connected: %s  ip=%s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] NOT connected (portal timed out) — running offline");
  }
  return ok;
}
