#include "core/TouchMapper.h"

void TouchMapper::toScreen(int rawX, int rawY,
                            int rawXMin, int rawXMax, int rawYMin, int rawYMax,
                            int screenWidth, int screenHeight,
                            int& screenX, int& screenY) {
    long sx = (long)(rawX - rawXMin) * (screenWidth - 1) / (rawXMax - rawXMin);
    long sy = (long)(rawY - rawYMin) * (screenHeight - 1) / (rawYMax - rawYMin);

    if (sx < 0) sx = 0;
    if (sx > screenWidth - 1) sx = screenWidth - 1;
    if (sy < 0) sy = 0;
    if (sy > screenHeight - 1) sy = screenHeight - 1;

    screenX = (int)sx;
    screenY = (int)sy;
}

bool TouchMapper::isWithinRect(int screenX, int screenY, int rectX, int rectY, int rectW, int rectH) {
    return screenX >= rectX && screenX < rectX + rectW &&
           screenY >= rectY && screenY < rectY + rectH;
}
