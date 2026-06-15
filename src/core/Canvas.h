#pragma once
#include <stdint.h>
// core/Canvas.h — the abstract draw interface pages render against (spec §1
// "rendering-agnostic pages", §4). The concrete core/Renderer implements this
// over the chosen graphics library (LovyanGFX), so the gfx lib stays a swappable
// decision and pages never touch a display register directly.
//
// PLACEHOLDER for milestone 0. The bring-up firmware draws directly via
// hal/Display::gfx() to validate the panel; this interface is filled in and
// Renderer added at milestone 0's UI-shell step / milestone 1. The methods
// below sketch the intended surface so the seam is visible from the start.

using Color = uint16_t;   // RGB565, matching the panel's native format

class Canvas {
public:
  virtual ~Canvas() = default;

  virtual int width()  const = 0;
  virtual int height() const = 0;

  virtual void fill(Color c) = 0;
  virtual void fillRect(int x, int y, int w, int h, Color c) = 0;
  virtual void drawRect(int x, int y, int w, int h, Color c) = 0;
  virtual void drawLine(int x0, int y0, int x1, int y1, Color c) = 0;
  virtual void drawText(int x, int y, const char* s, Color fg) = 0;

  // Dirty-rect redraws are first-class (spec §4: full 480x320 repaints are slow).
  virtual void markDirty(int x, int y, int w, int h) = 0;
};
