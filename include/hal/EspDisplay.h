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

    int& widthSlotFor(int x, int y);
};
