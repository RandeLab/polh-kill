# Camera evaluation (not built yet)

Question: can the ESP32-S3 send a status photo of the trap with every snap?
Short answer: **yes, but not with this DevKit.** Decision 2026-11-07: no camera in the first
firmware; this page keeps the evaluation for later.

## Options

| option | how | verdict |
|---|---|---|
| DVP camera (OV2640 / OV3660) on the DevKitC-1 | ESP32-S3 has a native camera interface (`esp32-camera`), needs PSRAM (we have 8 MB) and ~14-16 GPIOs plus an FPC adapter | possible, but a lot of wiring on a breadboard and it eats most free pins |
| board with camera connector | XIAO ESP32S3 Sense (tiny, PSRAM, SD) or DFRobot DFR1154 ESP32-S3 AI CAM (OV3660 160°, IR LEDs on GPIO 47) | **recommended**, the MPU still fits on the second I2C bus + 1 interrupt pin |
| USB webcam (UVC) on the native USB-OTG port | Espressif `usb_stream` / `usb_host_uvc` component, MJPEG | works in ESP-IDF, Arduino support is thin, and the USB port is then busy |
| second ESP32-CAM as "eye" | polh-kill sends a trigger over ESP-NOW / HTTP, the cam board takes the photo | flexible, but two devices to power |

## Practical notes

- A trap lives in ze dark: IR LEDs + a NoIR lens (or a white flash LED) are mandatory.
- Take the photo **0.5-1 s after the snap** – before that, ze camera is still shaking like
  a philosopher before ze lecture.
- VGA JPEG is ~20-50 kB, Telegram `sendPhoto` takes it as a multipart HTTPS POST without problems.
- Power: ~100-250 mA while the camera runs → fine on USB, on battery only on trigger.

## If we build it

1. Move to a camera board (DFR1154 or XIAO Sense), keep the MPU on I2C (other pins).
2. Add `src/camera.cpp` behind `-D POLH_CAMERA=1`, capture on snap and on `/photo`.
3. Telegram: `sendPhoto` multipart in the existing telegram task.
