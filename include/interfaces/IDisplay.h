#pragma once
#include <string>
#include <stdint.h>

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual void init() = 0;
    virtual void clear() = 0;
    virtual void drawText(int x, int y, const std::string& text, uint16_t color) = 0;
    virtual void drawButton(int x, int y, int w, int h, const std::string& label, uint16_t color) = 0;
    virtual void drawLine(int x0, int y0, int x1, int y1, uint16_t color) = 0;
    // Filled rectangle, no border — used to clear a small region (e.g. a
    // sparkline) before redrawing it, without a full-screen clear().
    virtual void fillRect(int x, int y, int w, int h, uint16_t color) = 0;
    // x, y are calibrated screen pixel coordinates (0..width-1, 0..height-1),
    // not raw touch-controller ADC readings.
    virtual bool touched(int& x, int& y) = 0;
    virtual void setBacklight(bool on) = 0;
};
