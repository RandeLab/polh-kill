// On-board WS2812 status colours (RandeSolar style NeoPixel moods):
//   blue breathing = setup hotspot open      dim green = online, armed
//   dim yellow     = online, disarmed        magenta   = OTA / learning test armed
//   red flash      = snap detected           white blip = any other event
#pragma once
#include <Arduino.h>
#include "config.h"

namespace statusLed {

enum Mode { BOOT, PORTAL, ONLINE_ARMED, ONLINE_DISARMED, LAB, OTA, ERROR };

inline Mode mode = BOOT;
inline uint32_t flashUntil = 0;
inline bool flashSnap = false;

inline void set(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_RGB_LED, r, g, b); }

inline void flashEvent(bool snap) {
  flashSnap = snap;
  flashUntil = millis() + (snap ? 3000 : 150);
}

inline void loop() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last < 30) return;
  last = now;
  if (now < flashUntil) {
    if (flashSnap) ((now / 150) % 2) ? set(60, 0, 0) : set(0, 0, 0);
    else set(30, 30, 30);
    return;
  }
  const float breath = (sinf(now / 600.0f) + 1.0f) * 0.5f;
  switch (mode) {
    case BOOT: set(10, 10, 10); break;
    case PORTAL: set(0, 0, 4 + (uint8_t)(30 * breath)); break;
    case ONLINE_ARMED: set(0, 6, 0); break;
    case ONLINE_DISARMED: set(6, 4, 0); break;
    case LAB: set(4 + (uint8_t)(25 * breath), 0, 4 + (uint8_t)(25 * breath)); break;
    case OTA: set(25, 0, 25); break;
    case ERROR: ((now / 500) % 2) ? set(30, 8, 0) : set(0, 0, 0); break;
  }
}

}  // namespace statusLed
