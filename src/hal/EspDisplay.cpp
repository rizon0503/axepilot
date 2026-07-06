#include "hal/EspDisplay.h"

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
    // Clear only as wide as the wider of the old and new text — enough to
    // guarantee a shorter string never leaves stray characters from
    // whatever longer text was there before (padding with trailing spaces
    // was the previous approach), without blanking the full remaining row
    // every time, which visibly flickers for lines that change often.
    int newWidth = tft.textWidth(text.c_str(), 4);
    int maxWidth = tft.width() - x;
    int clearWidth;

    int* lastWidth = widthSlotFor(x, y);
    if (lastWidth != nullptr) {
        clearWidth = (*lastWidth > newWidth) ? *lastWidth : newWidth;
        *lastWidth = newWidth;
    } else {
        clearWidth = maxWidth; // no free tracking slot — stay correct, just not minimal
    }
    if (clearWidth > maxWidth) clearWidth = maxWidth;

    tft.fillRect(x, y, clearWidth, tft.fontHeight(4), TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text.c_str(), x, y, 4); // Font 4
}

void EspDisplay::drawButton(int x, int y, int w, int h, const std::string& label, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
    tft.setTextColor(TFT_WHITE, color);
    // rough centering for font 4
    tft.drawCentreString(label.c_str(), x + w / 2, y + h / 2 - 10, 4);
}

bool EspDisplay::touched(int& x, int& y) {
    bool isTouched = ts.touched();
    if (isTouched) {
        TS_Point p = ts.getPoint();
        x = p.x; y = p.y;
    }
    return isTouched;
}

void EspDisplay::setBacklight(bool on) {
    digitalWrite(21, on ? HIGH : LOW);
}
