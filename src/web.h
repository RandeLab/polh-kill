// Web server: UI from LittleFS, JSON API, WebSocket live stream, captive portal,
// setup page and web OTA (both embedded, so they work even with an empty LittleFS).
#pragma once
#include "app.h"

namespace web {
void begin();
void loop();
void notifyEvent(const RecentEvent& e);
void notifyLab();
}  // namespace web
