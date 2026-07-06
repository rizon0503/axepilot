#include "hal/EspDisplay.h"
#include "core/TouchMapper.h"

EspDisplay::EspDisplay() : ts(33, 36) {} // CS=33, IRQ=36

void EspDisplay::init() {
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH); // Turn on backlight
    
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    // Init touch controller on separate SPI bus
    SPI.begin(25, 39, 32, 33); // SCK=25, MISO=39, MOSI=32, CS=33
    ts.begin();
    ts.setRotation(1);
}

void EspDisplay::clear() {
    tft.fillScreen(TFT_BLACK);
}

int* EspDisplay::widthSlotFor(int x, int y) {
    for (size_t i = 0; i < lineExtentCount; i++) {
        if (lineExtents[i].x == x && lineExtents[i].y == y) {
            return &lineExtents[i].width;
        }
    }
    if (lineExtentCount < MAX_TRACKED_LINES) {
        lineExtents[lineExtentCount] = {x, y, 0};
        return &lineExtents[lineExtentCount++].width;
    }
    return nullptr; // out of tracked slots — caller falls back to full-row clearing
}

void EspDisplay::drawText(int x, int y, const std::string& text, uint16_t color) {
    // setTextColor(color, TFT_BLACK) below already makes drawString() erase
    // behind every glyph of the NEW text as it draws it (an opaque
    // background, one glyph at a time — no separate blanking step, so no
    // visible flash). That alone only covers the pixel columns the new
    // text occupies, though: if the new string is shorter than whatever
    // was drawn here last time, the leftover tail of the old string would
    // stay on screen. So fillRect() only the trailing sliver beyond the
    // new text's width — the minimum needed to avoid stray characters —
    // instead of the whole row. When the new text is the same width or
    // wider (the common case for most telemetry updates), no fillRect runs
    // at all, matching the pre-#4 flicker-free redraw exactly.
    int newWidth = tft.textWidth(text.c_str(), 4);
    int maxWidth = tft.width() - x;
    int oldWidth = maxWidth; // unknown-history fallback: assume worst case

    int* lastWidth = widthSlotFor(x, y);
    if (lastWidth != nullptr) {
        oldWidth = *lastWidth;
        *lastWidth = newWidth;
    }

    if (oldWidth > newWidth) {
        int tailX = x + newWidth;
        int tailWidth = oldWidth - newWidth;
        if (tailX + tailWidth > x + maxWidth) {
            tailWidth = maxWidth - newWidth;
        }
        if (tailWidth > 0) {
            tft.fillRect(tailX, y, tailWidth, tft.fontHeight(4), TFT_BLACK);
        }
    }

    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text.c_str(), x, y, 4); // Font 4
}

void EspDisplay::drawButton(int x, int y, int w, int h, const std::string& label, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
    tft.setTextColor(TFT_WHITE, color);
    // rough centering for font 4
    tft.drawCentreString(label.c_str(), x + w / 2, y + h / 2 - 10, 4);
}

void EspDisplay::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    tft.drawLine(x0, y0, x1, y1, color);
}

void EspDisplay::fillRect(int x, int y, int w, int h, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
}

bool EspDisplay::touched(int& x, int& y) {
    bool isTouched = ts.touched();
    if (isTouched) {
        TS_Point p = ts.getPoint();
        TouchMapper::toScreen(p.x, p.y,
                               TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX,
                               tft.width(), tft.height(),
                               x, y);
    }
    return isTouched;
}

void EspDisplay::setBacklight(bool on) {
    digitalWrite(21, on ? HIGH : LOW);
}
