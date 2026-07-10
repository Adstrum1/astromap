#pragma once
#include <Arduino.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_SDA      21
#define OLED_SCL      22
#define OLED_ADDR     0x3C

static const uint8_t ROW_PINS[4] = {13, 14, 27, 26};
static const uint8_t COL_PINS[4] = {25, 33, 32, 4};

#define KEY_UP       0  // S1
#define KEY_DOWN     1  // S2
#define KEY_LEFT     2  // S3
#define KEY_RIGHT    3  // S4
#define KEY_ZOOM_IN  4  // S5
#define KEY_ZOOM_OUT 5  // S6
#define KEY_BACK     6  // S7
#define KEY_OK       7  // S8

#define KEY_DEBOUNCE_MS 30UL

#define FOV_MIN  20.0
#define FOV_MAX  360.0
#define FOV_STEP 10.0
#define STEP_DEG 4.0

#define MOON_VISIBILITY_FOV 60.0
