// polh-kill — compile-time configuration.
// Pins are provisional until the real board/sensor is wired; override them
// per environment with -D in platformio.ini.
#pragma once
#include <stdint.h>

#define POLH_NAME      "polh-kill"
#define POLH_VERSION   "0.2.0"
#define POLH_AP_SSID   "polh-kill-AP"
#define POLH_MDNS_HOST "polh-kill"
#define POLH_TZ        "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Zurich

// ---------------- pins (ESP32-S3-DevKitC-1 defaults) ----------------
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 8
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 9
#endif
#ifndef PIN_ACC_INT          // accelerometer activity / data-ready interrupt
#define PIN_ACC_INT 7
#endif
#ifndef PIN_RGB_LED          // on-board WS2812 (48 on DevKitC-1 v1.0, 38 on v1.1)
#define PIN_RGB_LED 48
#endif
#ifndef PIN_BOOT_BUTTON      // long press (3 s) = open the setup hotspot
#define PIN_BOOT_BUTTON 0
#endif

// ---------------- detection ----------------
constexpr int   WIN_PRE_MS        = 200;    // kept before the trigger
constexpr int   WIN_POST_MS       = 800;    // recorded after the trigger at least ...
constexpr int   WIN_MAX_MS        = 2500;   // ... and while it keeps moving, but never more than this in total
constexpr int   WIN_HANGOVER_MS   = 400;    // window ends this long after ze signal went quiet
constexpr int   WIN_MAX_SAMPLES   = 2500;   // 2.5 s at 1 kHz (~15 kB raw, 4 flash blocks)
constexpr int   RING_SAMPLES      = 4096;   // power of two, >= WIN_MAX_SAMPLES (in PSRAM)
constexpr int   REFRACTORY_MS     = 300;    // dead time after a window
constexpr int   WARMUP_MS         = 2000;   // learn the noise floor before triggering
constexpr int   LIVE_RATE_HZ      = 50;     // live plot points per second

// ---------------- learning ----------------
constexpr int   MAX_CLASSES       = 12;     // 5 built-in + custom ones (classes.cpp)
constexpr int   MAX_TG_USERS      = 10;     // Telegram subscribers of one bot (owner + 9 comrades)
constexpr int   NFEAT             = 15;     // 12 accel + gyroPeak + deltaTilt + axisZRatio
constexpr int   RECENT_EVENTS     = 6;      // raw windows kept in RAM
constexpr int   RAW_BUDGET_BLOCKS = 640;    // raw windows on flash, counted in 4 kB blocks (~2.6 of 6 MB):
                                            // 1 s window = 2 blocks, 2.5 s = 4. A nearly full LittleFS walks ze
                                            // whole flash to find a free block on every write - seconds, both
                                            // cores stalled. Older waveforms live on GitHub Pages anyway.
constexpr uint32_t TEST_ARM_TIMEOUT_MS = 60000;
