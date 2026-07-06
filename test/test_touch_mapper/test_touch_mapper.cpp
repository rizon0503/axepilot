#include <unity.h>
#include "core/TouchMapper.h"

void setUp(void) {}
void tearDown(void) {}

// Calibration measured on the physical device (see EspDisplay.cpp / #5):
// raw X: 300-3700, raw Y: 550-3650, screen 320x240 (rotation 1).
static constexpr int RAW_X_MIN = 300, RAW_X_MAX = 3700;
static constexpr int RAW_Y_MIN = 550, RAW_Y_MAX = 3650;
static constexpr int SCREEN_W = 320, SCREEN_H = 240;

void test_maps_min_corner_to_origin() {
    int sx, sy;
    TouchMapper::toScreen(RAW_X_MIN, RAW_Y_MIN, RAW_X_MIN, RAW_X_MAX, RAW_Y_MIN, RAW_Y_MAX, SCREEN_W, SCREEN_H, sx, sy);

    TEST_ASSERT_EQUAL(0, sx);
    TEST_ASSERT_EQUAL(0, sy);
}

void test_maps_max_corner_to_bottom_right() {
    int sx, sy;
    TouchMapper::toScreen(RAW_X_MAX, RAW_Y_MAX, RAW_X_MIN, RAW_X_MAX, RAW_Y_MIN, RAW_Y_MAX, SCREEN_W, SCREEN_H, sx, sy);

    TEST_ASSERT_EQUAL(SCREEN_W - 1, sx);
    TEST_ASSERT_EQUAL(SCREEN_H - 1, sy);
}

void test_maps_midpoint_to_center() {
    int rawMidX = (RAW_X_MIN + RAW_X_MAX) / 2;
    int rawMidY = (RAW_Y_MIN + RAW_Y_MAX) / 2;
    int sx, sy;
    TouchMapper::toScreen(rawMidX, rawMidY, RAW_X_MIN, RAW_X_MAX, RAW_Y_MIN, RAW_Y_MAX, SCREEN_W, SCREEN_H, sx, sy);

    TEST_ASSERT_INT_WITHIN(2, SCREEN_W / 2, sx);
    TEST_ASSERT_INT_WITHIN(2, SCREEN_H / 2, sy);
}

void test_clamps_readings_beyond_the_calibrated_range() {
    int sx, sy;
    // A touch right at the physical edge can read slightly past the
    // calibrated min/max — must not produce a negative or overflowing pixel.
    TouchMapper::toScreen(RAW_X_MIN - 200, RAW_Y_MIN - 200, RAW_X_MIN, RAW_X_MAX, RAW_Y_MIN, RAW_Y_MAX, SCREEN_W, SCREEN_H, sx, sy);
    TEST_ASSERT_EQUAL(0, sx);
    TEST_ASSERT_EQUAL(0, sy);

    TouchMapper::toScreen(RAW_X_MAX + 200, RAW_Y_MAX + 200, RAW_X_MIN, RAW_X_MAX, RAW_Y_MIN, RAW_Y_MAX, SCREEN_W, SCREEN_H, sx, sy);
    TEST_ASSERT_EQUAL(SCREEN_W - 1, sx);
    TEST_ASSERT_EQUAL(SCREEN_H - 1, sy);
}

void test_is_within_rect_true_when_inside() {
    // The emergency throttle button: (0, 180, 320, 60)
    TEST_ASSERT_TRUE(TouchMapper::isWithinRect(160, 200, 0, 180, 320, 60));
    TEST_ASSERT_TRUE(TouchMapper::isWithinRect(0, 180, 0, 180, 320, 60));   // top-left corner, inclusive
}

void test_is_within_rect_false_when_outside() {
    TEST_ASSERT_FALSE(TouchMapper::isWithinRect(160, 179, 0, 180, 320, 60)); // just above the button
    TEST_ASSERT_FALSE(TouchMapper::isWithinRect(160, 240, 0, 180, 320, 60)); // exclusive bottom edge
    TEST_ASSERT_FALSE(TouchMapper::isWithinRect(320, 200, 0, 180, 320, 60)); // exclusive right edge
    TEST_ASSERT_FALSE(TouchMapper::isWithinRect(-1, 200, 0, 180, 320, 60));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_maps_min_corner_to_origin);
    RUN_TEST(test_maps_max_corner_to_bottom_right);
    RUN_TEST(test_maps_midpoint_to_center);
    RUN_TEST(test_clamps_readings_beyond_the_calibrated_range);
    RUN_TEST(test_is_within_rect_true_when_inside);
    RUN_TEST(test_is_within_rect_false_when_outside);
    return UNITY_END();
}
