#pragma once
#include "interfaces/IDisplay.h"
#include <string>
#include <vector>

class MockDisplay : public IDisplay {
public:
    struct TextCall { int x; int y; std::string text; uint16_t color; };
    struct ButtonCall { int x; int y; int w; int h; std::string label; uint16_t color; };

    std::vector<TextCall> textCalls;
    std::vector<ButtonCall> buttonCalls;
    int clearCalls = 0;
    bool backlightOn = true;

    void init() override {}

    // Mirrors the real display: clearing wipes what's currently visible, so
    // only calls since the last clear() reflect what's actually on screen.
    void clear() override {
        clearCalls++;
        textCalls.clear();
        buttonCalls.clear();
    }

    void drawText(int x, int y, const std::string& text, uint16_t color) override {
        textCalls.push_back({x, y, text, color});
    }

    void drawButton(int x, int y, int w, int h, const std::string& label, uint16_t color) override {
        buttonCalls.push_back({x, y, w, h, label, color});
    }

    bool touched(int&, int&) override { return false; }

    void setBacklight(bool on) override { backlightOn = on; }

    // Most recent drawText() call at (x, y), or nullptr if none since the
    // last clear().
    const TextCall* lastTextAt(int x, int y) const {
        for (auto it = textCalls.rbegin(); it != textCalls.rend(); ++it) {
            if (it->x == x && it->y == y) return &(*it);
        }
        return nullptr;
    }

    // Most recent drawButton() call at (x, y), or nullptr if none since the
    // last clear().
    const ButtonCall* lastButtonAt(int x, int y) const {
        for (auto it = buttonCalls.rbegin(); it != buttonCalls.rend(); ++it) {
            if (it->x == x && it->y == y) return &(*it);
        }
        return nullptr;
    }
};
