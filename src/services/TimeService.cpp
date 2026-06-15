#include "TimeService.h"
#include "../core/EventBus.h"
#include "../hal/Rtc.h"

static const char* kNtp1 = "pool.ntp.org";
static const char* kNtp2 = "time.google.com";
static const char* kNtp3 = "time.nist.gov";

bool TimeService::begin() {
  // UTC to start; applyTzOffset() sets the real local offset once Location
  // resolves. configTime(gmtOffset, dstOffset, servers) also configures TZ.
  configTime(0, 0, kNtp1, kNtp2, kNtp3);

  if (_rtc && _rtc->present()) {
    time_t t;
    if (_rtc->getTime(t)) {
      struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      Serial.println("[time] seeded from RTC");
    }
  }
  return true;
}

void TimeService::applyTzOffset(long offsetSec) {
  _tzOffset = offsetSec;
  configTime(offsetSec, 0, kNtp1, kNtp2, kNtp3);
}

void TimeService::tick() {
  if (_synced) return;
  if (time(nullptr) > 1600000000) {           // ~2020-09 — NTP has landed
    _synced = true;
    Serial.printf("[time] NTP synced: %ld\n", (long)time(nullptr));
    if (_rtc && _rtc->present()) _rtc->setTime(time(nullptr));
    if (_bus) _bus->publish(ProviderId::Time);
  }
}

double TimeService::julianDate() const {
  return (double)time(nullptr) / 86400.0 + 2440587.5;
}
