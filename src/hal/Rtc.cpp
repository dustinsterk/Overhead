#include "Rtc.h"
#include "Board.h"

bool Rtc::begin() {
#if CAP_HAS_RTC
  // The board exposes an RTC, but its driver isn't wired up yet (milestone 2).
  Serial.println("[rtc] board has an RTC; driver not yet implemented — using NTP");
  _present = false;
#else
  _present = false;
#endif
  return _present;
}

bool Rtc::getTime(time_t& out) { (void)out; return false; }
bool Rtc::setTime(time_t t)    { (void)t;   return false; }
