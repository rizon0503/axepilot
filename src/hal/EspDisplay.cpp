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

void EspDisplay::drawText(int x, int y, const std::string& text, uint16_t color) {
    // Clear the full row before drawing so a shorter string never leaves
    // stray characters from whatever longer text was there before —
    // padding the string with trailing spaces was the previous approach.
    tft.fillRect(x, y, tft.width() - x, tft.fontHeight(4), TFT_BLACK);
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
