#pragma once
#include <Arduino.h>
#include <time.h>

class EventBus;
class Rtc;

// services/TimeService — NTP sync + local time + Julian date (spec §4, §5, §13).
//
// All astro compute must be gated on synced() — SGP4/ephemeris are garbage with
// a bad clock (spec §13). System epoch is UTC; applyTzOffset() reconfigures the
// libc TZ so localtime_r() returns local time everywhere (status clock, agenda).
class TimeService {
public:
  void setBus(EventBus* bus) { _bus = bus; }
  void setRtc(Rtc* rtc)      { _rtc = rtc; }

  bool begin();                       // start SNTP (UTC), seed from RTC if usable
  void applyTzOffset(long offsetSec); // re-apply local offset (from Location)
  void tick();                        // detect the sync edge -> publish Time

  bool   synced() const { return _synced; }
  time_t nowUtc() const { return time(nullptr); }
  double julianDate() const;          // JD(UT) for the astro core

private:
  bool      _synced   = false;
  long      _tzOffset = 0;
  EventBus* _bus      = nullptr;
  Rtc*      _rtc      = nullptr;
};
