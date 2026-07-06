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
};
