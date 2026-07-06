#pragma once

// Pure, hardware-independent touch coordinate math extracted from
// EspDisplay/main.cpp so it's testable on the host. The XPT2046 resistive
// controller reports raw ADC readings (roughly 0-4095, but real corners
// rarely reach the extremes) that must be linearly mapped to screen pixel
// coordinates using per-device calibration constants measured empirically
// (see EspDisplay.cpp).
namespace TouchMapper {

// Maps a raw (rawX, rawY) reading to screen pixel coordinates, clamped to
// [0, screenWidth-1] x [0, screenHeight-1] so an out-of-calibration reading
// (finger slightly past a calibrated corner) never produces an out-of-range
// pixel coordinate.
void toScreen(int rawX, int rawY,
              int rawXMin, int rawXMax, int rawYMin, int rawYMax,
              int screenWidth, int screenHeight,
              int& screenX, int& screenY);

// Whether (screenX, screenY) falls within the rectangle at (rectX, rectY)
// sized (rectW, rectH) — used to gate the emergency throttle to the actual
// button area now that touches are calibrated, instead of reacting to any
// touch anywhere on screen.
bool isWithinRect(int screenX, int screenY, int rectX, int rectY, int rectW, int rectH);

} // namespace TouchMapper
