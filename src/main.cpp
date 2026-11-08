/**
 * polh-kill - mousetrap snap detector for the RandeLab
 *
 *  - MPU-6500/9250 accelerometer sampled at 1 kHz through its FIFO
 *  - recursive noise floor + impulse trigger, 1 s windows around each event
 *  - online learning (naive Bayes + recursive least squares) from labelled
 *    tests in the web "learning lab" or from Telegram buttons
 *  - Telegram bot alerts, fallback hotspot "polh-kill-AP" with captive portal
 *  - OTA: ArduinoOTA (pio -e polh-ota) and web upload at /update
 *
 * Target: ESP32-S3-WROOM-1 N16R8 (DevKitC-1 clone). GPL-3.0.
 */
#include <Arduino.h>
#include "app.h"
#include "github.h"
#include "config.h"
#include "net.h"
#include "settings.h"
#include "status_led.h"
#include "telegram.h"
#include "web.h"

namespace {

void handleBootButton() {
  static uint32_t pressedAt = 0;
  static bool fired = false;
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    if (!pressedAt) pressedAt = millis();
    if (!fired && millis() - pressedAt > 3000) {
      fired = true;
      Serial.println("[Button] long press -> opening the setup hotspot");
      net::openPortal();
    }
  } else {
    pressedAt = 0;
    fired = false;
  }
}

void updateLedMode() {
  using namespace statusLed;
  if (net::otaInProgress()) mode = OTA;
  else if (!app::sensorOk()) mode = ERROR;
  else if (app::testLabel() >= 0) mode = LAB;
  else if (net::portalActive()) mode = PORTAL;
  else mode = settings.armed ? ONLINE_ARMED : ONLINE_DISARMED;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);  // give USB CDC a moment so the boot log is visible
  Serial.printf("\n\n🐭 polh-kill v%s booting. And so on, and so on.\n", POLH_VERSION);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  statusLed::mode = statusLed::BOOT;
  statusLed::set(10, 10, 10);

  settings.load();
  app::begin();
  net::begin();
  web::begin();
  telegram::begin();
  github::begin();
}

void loop() {
  net::loop();
  app::loop();
  web::loop();
  handleBootButton();
  updateLedMode();
  statusLed::loop();
  delay(1);
}
