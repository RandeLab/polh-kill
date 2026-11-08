// WiFi: station mode with saved credentials, fallback hotspot "polh-kill-AP"
// with captive portal (DNS wildcard) when there is no WiFi, mDNS, NTP, ArduinoOTA.
#pragma once
#include <Arduino.h>

namespace net {
enum Mode { CONNECTING, ONLINE, PORTAL };

void begin();
void loop();
void openPortal();      // e.g. long press on BOOT
Mode mode();
const char* modeName();
bool online();          // station connected (internet possible)
bool portalActive();
String ip();
int rssi();
int apClients();
void startScan();
String scanJson();      // [] while scanning
bool otaInProgress();
// one TLS session at a time (Telegram + GitHub share the internal RAM)
void tlsLock();
void tlsUnlock();
}  // namespace net
