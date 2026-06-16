#pragma once
#include <stdint.h>
#include "Ids.h"

class App;

// core/Page — the interface every content page implements (spec §4).
//
// Only the active page renders/ticks. Providers keep updating in the background
// and notify via the EventBus; the App forwards those to the active page's
// onData(), which marks itself dirty for the next tick. Pages draw through the
// App (which exposes the display/renderer) and read colours from gTheme — never
// hardcoded.
//
// PreFocus (Director-supplied entry hints, e.g. {selectSatId:25544}) arrives
// with the Director in milestone 7; onEnter is parameterless for now.
class Page {
public:
  virtual ~Page() = default;

  virtual const char* title() const = 0;

  virtual void onEnter(App& app) {}
  virtual void onExit(App& app) {}
  virtual void tick(App& app, uint32_t nowMs) {}   // dirty-rect redraws
  virtual void onTouch(App& app, int x, int y) {}
  virtual void onData(App& app, ProviderId id) {}  // EventBus delivery

  // Attract-mode step (spec §7). While the Director is resting in AUTO with no
  // specific item to highlight, it calls this on a dwell timer so the page tours
  // its selectable objects and then its alternate views, repeating. Pages with
  // nothing to cycle leave it a no-op.
  virtual void autoAdvance(App& app) {}
};
