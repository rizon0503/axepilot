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
    virtual bool touched(int& x, int& y) = 0;
    virtual void setBacklight(bool on) = 0;
};
