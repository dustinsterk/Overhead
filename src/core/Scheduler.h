#pragma once
#include <functional>
#include <vector>
#include <stdint.h>

// core/Scheduler — cooperative interval runner (spec §4). Ticked from the UI
// loop; runs each registered callback no more often than its interval. Not a
// preemptive timer — callbacks must be short and non-blocking (network work
// goes through NetClient, never here).
class Scheduler {
public:
  // Register a periodic callback. runNow=true fires it on the first tick.
  void every(uint32_t intervalMs, std::function<void()> fn, bool runNow = true) {
    _tasks.push_back({intervalMs, runNow ? 0 : (uint32_t)-1, std::move(fn)});
  }

  void tick(uint32_t nowMs) {
    for (auto& t : _tasks) {
      if (t.last == (uint32_t)-1) { t.last = nowMs; continue; }
      if (nowMs - t.last >= t.interval) { t.last = nowMs; t.fn(); }
    }
  }

private:
  struct Task { uint32_t interval; uint32_t last; std::function<void()> fn; };
  std::vector<Task> _tasks;
};
