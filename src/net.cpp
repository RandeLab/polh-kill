#include "net.h"
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>
#include "config.h"
#include "settings.h"
#include "status_led.h"

namespace net {
namespace {

constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;   // first connect before the hotspot opens
constexpr uint32_t LOST_TO_PORTAL_MS = 90000;    // WiFi gone this long -> hotspot
constexpr uint32_t PORTAL_RETRY_MS = 60000;      // retry STA while nobody uses the hotspot
constexpr uint32_t PORTAL_CLOSE_IDLE_MS = 15000; // STA back + hotspot unused -> close

DNSServer dns;
Mode state = CONNECTING;
bool portal = false, otaStarted = false, mdnsStarted = false, timeStarted = false;
volatile bool otaRunning = false;
uint32_t stateSince = 0, lostAt = 0, lastRetry = 0, apIdleSince = 0;
String scanCache = "[]";

void startOta() {
  if (otaStarted) return;
  ArduinoOTA.setHostname(POLH_MDNS_HOST);
  if (settings.adminPass.length()) ArduinoOTA.setPassword(settings.adminPass.c_str());
  ArduinoOTA.onStart([]() {
    otaRunning = true;
    statusLed::mode = statusLed::OTA;
    if (ArduinoOTA.getCommand() == U_SPIFFS) LittleFS.end();
    Serial.println("\n[ArduinoOTA ⚡] update starts, hold on to your cheese");
  });
  ArduinoOTA.onEnd([]() { Serial.println("\n[ArduinoOTA ⚡] done, rebooting"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("[ArduinoOTA] %u%%\r", (unsigned)(p * 100 / (t ? t : 1)));
  });
  ArduinoOTA.onError([](ota_error_t e) {
    otaRunning = false;
    Serial.printf("\n[ArduinoOTA ❌] error %u\n", (unsigned)e);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.printf("[OTA ⚡] ArduinoOTA ready (%s.local:3232), web upload at /update\n", POLH_MDNS_HOST);
}

void onOnline() {
  state = ONLINE;
  stateSince = millis();
  lostAt = 0;
  Serial.printf("[WiFi ✅] connected to '%s', IP %s, RSSI %d dBm\n", settings.wifiSsid.c_str(),
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!mdnsStarted && MDNS.begin(POLH_MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.printf("[mDNS] http://%s.local\n", POLH_MDNS_HOST);
  }
  if (!timeStarted) {
    configTzTime(POLH_TZ, "pool.ntp.org", "time.google.com");
    timeStarted = true;
  }
  startOta();
}

void closePortal() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portal = false;
  Serial.println("[Portal] hotspot closed, back in the normal WiFi");
}

void beginSta() {
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
  lastRetry = millis();
}

}  // namespace

void begin() {
  WiFi.persistent(false);
  WiFi.setHostname(POLH_MDNS_HOST);
  if (settings.wifiSsid.length()) {
    WiFi.mode(WIFI_STA);
    beginSta();
    state = CONNECTING;
    stateSince = millis();
    Serial.printf("[WiFi] connecting to '%s' ...\n", settings.wifiSsid.c_str());
  } else {
    Serial.println("[WiFi] no credentials yet");
    openPortal();
  }
}

void openPortal() {
  if (portal) return;
  WiFi.mode(settings.wifiSsid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(POLH_AP_SSID);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  portal = true;
  state = PORTAL;
  stateSince = millis();
  apIdleSince = millis();
  startOta();
  startScan();
  Serial.printf("[Portal 📶] hotspot '%s' open -> http://%s\n", POLH_AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

void loop() {
  if (portal) dns.processNextRequest();
  if (otaStarted) ArduinoOTA.handle();
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  switch (state) {
    case CONNECTING:
      if (connected) onOnline();
      else if (now - stateSince > CONNECT_TIMEOUT_MS) {
        Serial.println("[WiFi ❌] no connection, opening the hotspot");
        openPortal();
      }
      break;

    case ONLINE:
      if (connected) {
        lostAt = 0;
      } else {
        if (!lostAt) {
          lostAt = now;
          Serial.println("[WiFi ⚠️] connection lost, retrying");
        }
        if (now - lostAt > LOST_TO_PORTAL_MS) openPortal();
      }
      break;

    case PORTAL: {
      const int clients = WiFi.softAPgetStationNum();
      if (clients > 0) apIdleSince = now;
      if (!settings.wifiSsid.length()) break;
      if (connected) {
        if (now - apIdleSince > PORTAL_CLOSE_IDLE_MS) {
          closePortal();
          onOnline();
        }
      } else if (clients > 0) {
        // somebody is configuring: stop channel-hopping STA retries
        if (WiFi.getAutoReconnect()) {
          WiFi.setAutoReconnect(false);
          WiFi.disconnect(false);
        }
      } else if (now - lastRetry > PORTAL_RETRY_MS) {
        beginSta();
      }
      break;
    }
  }
}

Mode mode() { return state; }

const char* modeName() {
  switch (state) {
    case CONNECTING: return "connecting";
    case ONLINE: return "online";
    default: return "portal";
  }
}

bool online() { return WiFi.status() == WL_CONNECTED; }
bool portalActive() { return portal; }

String ip() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  if (portal) return WiFi.softAPIP().toString();
  return "";
}

int rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }
int apClients() { return portal ? WiFi.softAPgetStationNum() : 0; }

void startScan() {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  WiFi.scanNetworks(true);
}

String scanJson() {
  const int n = WiFi.scanComplete();
  if (n >= 0) {
    // de-duplicate SSIDs, keep the strongest; results come sorted by RSSI
    String out = "[";
    std::vector<String> seen;
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (!s.length() || std::find(seen.begin(), seen.end(), s) != seen.end()) continue;
      seen.push_back(s);
      s.replace("\\", "\\\\");
      s.replace("\"", "\\\"");
      if (out.length() > 1) out += ",";
      out += "{\"ssid\":\"" + s + "\",\"rssi\":" + WiFi.RSSI(i) +
             ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    out += "]";
    scanCache = out;
    WiFi.scanDelete();
  }
  return scanCache;
}

bool otaInProgress() { return otaRunning; }

static SemaphoreHandle_t tlsMtx = xSemaphoreCreateMutex();
void tlsLock() { xSemaphoreTake(tlsMtx, portMAX_DELAY); }
void tlsUnlock() { xSemaphoreGive(tlsMtx); }

}  // namespace net
