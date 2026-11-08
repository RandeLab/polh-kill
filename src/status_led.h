// On-board WS2812 (GPIO 48): ze trap's face. Meanings are documented in docs/led.md.
//
// Steady state (what ze trap is doing):
//   white dim        booting
//   blue breathing   setup hotspot "polh-kill-AP" open
//   green dim        online, armed
//   yellow dim       online, disarmed (no Telegram alerts)
//   magenta breath   learning test armed - do ze thing now
//   magenta solid    OTA update running
//   orange blinking  no accelerometer found
//
// Short flashes on top (what just happened):
//   red blinking 3 s   SNAP detected
//   class colour 0.4 s any other capture: bump orange, footstep blue, mouse yellow,
//                      other white, custom classes violet
//   green double blink a labelled sample was learned
//   cyan double blink  captures pushed to GitHub Pages
#pragma once
#include <Arduino.h>
#include "config.h"

namespace statusLed {

enum Mode { BOOT, PORTAL, ONLINE_ARMED, ONLINE_DISARMED, LAB, OTA, ERROR };

inline Mode mode = BOOT;

struct Flash {
  uint8_t r = 0, g = 0, b = 0;
  uint32_t until = 0;
  uint16_t blinkMs = 0;  // 0 = solid
};
inline volatile bool flashNew = false;
inline Flash pendingFlash, flash;

inline void set(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_RGB_LED, r, g, b); }

// may be called from any task; the LED itself is only written in loop()
inline void flashColor(uint8_t r, uint8_t g, uint8_t b, uint32_t ms, uint16_t blinkMs = 0) {
  pendingFlash.r = r; pendingFlash.g = g; pendingFlash.b = b;
  pendingFlash.until = millis() + ms;
  pendingFlash.blinkMs = blinkMs;
  flashNew = true;
}

inline void flashEvent(int cls, bool snap) {
  if (snap) return flashColor(60, 0, 0, 3000, 150);
  // snap, bump, footstep, mouse, other, armed-trap (pink)
  static const uint8_t col[6][3] = {{60, 0, 0}, {50, 18, 0}, {0, 10, 50}, {45, 35, 0}, {30, 30, 30}, {60, 8, 22}};
  if (cls >= 0 && cls < 6) flashColor(col[cls][0], col[cls][1], col[cls][2], 400);
  else flashColor(25, 0, 45, 400);  // custom class
}
inline void flashLearned() { flashColor(0, 45, 0, 600, 150); }
inline void flashPushed() { flashColor(0, 35, 40, 600, 150); }

inline void loop() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last < 30) return;
  last = now;
  if (flashNew) {
    flashNew = false;
    flash = pendingFlash;
  }
  if ((int32_t)(flash.until - now) > 0) {
    const bool on = !flash.blinkMs || (now / flash.blinkMs) % 2;
    on ? set(flash.r, flash.g, flash.b) : set(0, 0, 0);
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
