#pragma once
#include <Arduino.h>
#include <time.h>

// hal/Rtc — optional battery-backed RTC (spec §2, §13). Per-target capability
// flag CAP_HAS_RTC. When present it seeds system time at boot (correct time
// survives power loss / offline boots) and NTP corrects its drift later.
//
// NOTE (milestone 1): the concrete chip driver is not implemented yet. The 4"
// CYD has no onboard RTC; the CrowPanel has a PCF8563 (not the DS3231 the spec
// anticipated for the CYD). begin() reports "not usable" for now so the system
// relies on NTP and gates astro on a valid sync (spec §13). The chip driver
// lands with the astro core (milestone 2).
class Rtc {
public:
  bool begin();
  bool present() const { return _present; }
  bool getTime(time_t& out);     // RTC -> epoch
  bool setTime(time_t t);        // system time -> RTC

private:
  bool _present = false;
};
