#pragma once
#include "interfaces/IDisplay.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

class EspDisplay : public IDisplay {
public:
    EspDisplay();
    void init() override;
    void clear() override;
    void drawText(int x, int y, const std::string& text, uint16_t color) override;
    void drawButton(int x, int y, int w, int h, const std::string& label, uint16_t color) override;
    bool touched(int& x, int& y) override;
    void setBacklight(bool on) override;

private:
    TFT_eSPI tft;
    XPT2046_Touchscreen ts;

    // Remembers the pixel width of the last string drawn at each (x, y)
    // call site, so drawText() only needs to fillRect() as wide as the
    // wider of the old/new text — not the full remaining row — shrinking
    // the visible flash for lines whose value changes almost every refresh
    // (e.g. live hashrate, which is naturally noisy sample to sample).
    // Small fixed set: this project only ever draws at a handful of
    // distinct (x, y) positions.
    struct LineExtent { int x; int y; int width; };
    static constexpr size_t MAX_TRACKED_LINES = 8;
    LineExtent lineExtents[MAX_TRACKED_LINES] = {};
    size_t lineExtentCount = 0;

    // Returns the tracked width slot for (x, y), creating one if there's
    // room; nullptr if every slot is already in use by a different
    // position (drawText() then falls back to clearing the full row for
    // that call, so correctness never depends on this limit).
    int* widthSlotFor(int x, int y);

    // Raw XPT2046 ADC calibration, measured on the physical device (#5):
    // touched all four screen corners and recorded the raw (x, y) readings
    // via Serial. See TouchMapper for the mapping math.
    static constexpr int TOUCH_RAW_X_MIN = 300;
    static constexpr int TOUCH_RAW_X_MAX = 3700;
    static constexpr int TOUCH_RAW_Y_MIN = 550;
    static constexpr int TOUCH_RAW_Y_MAX = 3650;
};
