#include "web.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include "dataset.h"
#include "features.h"
#include "github.h"
#include "net.h"
#include "pages.h"
#include "settings.h"
#include "status_led.h"
#include "telegram.h"
#include <esp_core_dump.h>
#include <esp_system.h>

namespace web {
namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
bool uiOnFlash = false;
uint32_t rebootAt = 0;
String otaError;

// ---------------- helpers ----------------

String htmlEsc(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

String render(const char* tpl) {
  String s(tpl);
  s.replace("%STYLE%", PAGE_STYLE);
  s.replace("%VERSION%", POLH_VERSION);
  s.replace("%SSID%", htmlEsc(settings.wifiSsid));
  s.replace("%CHAT%", settings.tgChat.length() ? "already paired, manage comrades with /users or in ze web UI" : "after saving: send /start to your bot, ze first one is ze owner");
  s.replace("%NAME%", htmlEsc(settings.deviceName));
  s.replace("%PASS_HINT%", settings.wifiPass.length() ? "(saved, leave empty to keep)" : "");
  s.replace("%TOKEN_HINT%", settings.tgToken.length() ? "(saved, leave empty to keep)" : "123456:ABC-…");
  s.replace("%ADMIN_HINT%", settings.adminPass.length() ? "(set, leave empty to keep)" : "none");
  s.replace("%GH_HINT%", settings.ghToken.length() ? "(saved, leave empty to keep)" : "github_pat_…");
  s.replace("%GH_REPO%", htmlEsc(settings.ghRepo));
  return s;
}

void sendPage(AsyncWebServerRequest* r, const char* tpl) { r->send(200, "text/html; charset=utf-8", render(tpl)); }

void sendJson(AsyncWebServerRequest* r, JsonDocument& d) {
  AsyncResponseStream* res = r->beginResponseStream("application/json");
  serializeJson(d, *res);
  r->send(res);
}

void ok(AsyncWebServerRequest* r) { r->send(200, "application/json", "{\"ok\":true}"); }
void fail(AsyncWebServerRequest* r, const char* why, int code = 400) {
  r->send(code, "application/json", String("{\"ok\":false,\"error\":\"") + why + "\"}");
}

// optional admin password (HTTP basic auth, user "admin")
bool authed(AsyncWebServerRequest* r) {
  if (!settings.adminPass.length()) return true;
  if (r->authenticate("admin", settings.adminPass.c_str())) return true;
  r->requestAuthentication("polh-kill");
  return false;
}

String param(AsyncWebServerRequest* r, const char* name) {
  if (r->hasParam(name, true)) return r->getParam(name, true)->value();
  if (r->hasParam(name)) return r->getParam(name)->value();
  return "";
}

int parseLabel(const String& s) {
  for (int c = 0; c < classCount(); c++)
    if (s.equalsIgnoreCase(className(c))) return c;
  if (s.length() && isDigit(s[0])) return constrain(s.toInt(), 0, classCount() - 1);
  return -1;
}

bool isOurHost(AsyncWebServerRequest* r) {
  const String h = r->host();
  return h == WiFi.softAPIP().toString() || h == WiFi.localIP().toString() ||
         h.startsWith(POLH_MDNS_HOST) || h.length() == 0;
}

void redirectToSetup(AsyncWebServerRequest* r) {
  r->redirect("http://" + WiFi.softAPIP().toString() + "/setup");
}

void streamWindow(AsyncResponseStream* s, uint16_t rate, uint16_t n, uint16_t trig, float thr,
                  const int16_t (*xyz)[3]) {
  s->printf("{\"rate\":%u,\"n\":%u,\"trig\":%u,\"threshold_g\":%.4f", rate, n, trig, thr);
  const char* axes[3] = {"x", "y", "z"};
  for (int a = 0; a < 3; a++) {
    s->printf(",\"%s\":[", axes[a]);
    for (uint16_t i = 0; i < n; i++) s->printf(i ? ",%d" : "%d", xyz[i][a]);
    s->print("]");
  }
  s->print("}");
}

// ---------------- route groups ----------------

void routesStatus() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    app::statusToJson(d.to<JsonObject>());
    sendJson(r, d);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    d["k_sigma"] = settings.kSigma;
    d["min_g"] = settings.minG;
    d["snap_g"] = settings.snapPeakG;
    d["alert_p"] = settings.alertProb;
    d["unsure_p"] = settings.unsureProb;
    d["armed"] = settings.armed;
    d["notify_unsure"] = settings.notifyUnsure;
    d["daily"] = settings.dailyReport;
    d["voice"] = settings.voiceMsgs;
    d["gh_every"] = settings.ghEveryMin;
    sendJson(r, d);
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* r) {
    auto f = [&](const char* k, float& v, float lo, float hi) {
      const String s = param(r, k);
      if (s.length()) v = constrain(s.toFloat(), lo, hi);
    };
    auto b = [&](const char* k, bool& v) {
      const String s = param(r, k);
      if (s.length()) v = (s == "1" || s == "true" || s == "on");
    };
    f("k_sigma", settings.kSigma, 2, 30);
    f("min_g", settings.minG, 0.005f, 5);
    f("snap_g", settings.snapPeakG, 0.1f, 16);
    f("alert_p", settings.alertProb, 0.3f, 0.99f);
    f("unsure_p", settings.unsureProb, 0.05f, 0.95f);
    b("armed", settings.armed);
    b("notify_unsure", settings.notifyUnsure);
    b("daily", settings.dailyReport);
    b("voice", settings.voiceMsgs);
    if (param(r, "gh_every").length()) settings.ghEveryMin = constrain(param(r, "gh_every").toInt(), 1, 1440);
    settings.save();
    app::applySettings();
    ok(r);
  });
}

void routesLab() {
  server.on("/api/lab/arm", HTTP_POST, [](AsyncWebServerRequest* r) {
    const int l = parseLabel(param(r, "label"));
    if (l < 0) return fail(r, "label");
    app::armTest(l);
    notifyLab();
    ok(r);
  });
  server.on("/api/lab/cancel", HTTP_POST, [](AsyncWebServerRequest* r) {
    app::cancelTest();
    notifyLab();
    ok(r);
  });
  server.on("/api/lab/label", HTTP_POST, [](AsyncWebServerRequest* r) {
    const int l = parseLabel(param(r, "label"));
    const uint32_t id = app::labelEvent(param(r, "event").toInt(), l);
    if (!id) return fail(r, "event gone or bad label");
    r->send(200, "application/json", "{\"ok\":true,\"sample_id\":" + String(id) + "}");
  });
  server.on("/api/lab/discard", HTTP_POST, [](AsyncWebServerRequest* r) {
    app::discardEvent(param(r, "event").toInt()) ? ok(r) : fail(r, "event");
  });

  server.on("/api/classes", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    app::Guard g;
    for (int c = 0; c < classCount(); c++) {
      JsonObject o = a.add<JsonObject>();
      o["name"] = className(c);
      o["emoji"] = classEmoji(c);
      o["builtin"] = c < builtinClassCount();
      o["n"] = app::model().count(c);
    }
    sendJson(r, d);
  });
  server.on("/api/classes/add", HTTP_POST, [](AsyncWebServerRequest* r) {
    String err;
    int idx;
    {
      app::Guard g;
      idx = classAdd(param(r, "name"), param(r, "emoji"), err);
    }
    if (idx < 0) return fail(r, err.c_str());
    Serial.printf("[Classes 🏷️] new class %s %s\n", classEmoji(idx), className(idx));
    ok(r);
  });
  server.on("/api/classes/delete", HTTP_POST, [](AsyncWebServerRequest* r) {
    String err;
    if (!app::deleteClass(classFind(param(r, "name")), err)) return fail(r, err.c_str());
    ok(r);
  });

  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    app::Guard g;
    for (int i = app::recentCount() - 1; i >= 0; i--) app::eventToJson(*app::recentAt(i), a.add<JsonObject>());
    sendJson(r, d);
  });

  // raw window of a recent event (?event=) or a stored sample (?sample=)
  server.on("/api/window", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("event")) {
      app::Guard g;
      const RecentEvent* e = app::findEvent(r->getParam("event")->value().toInt());
      if (!e) return fail(r, "event gone", 404);
      AsyncResponseStream* s = r->beginResponseStream("application/json");
      streamWindow(s, e->win->rateHz, e->win->n, e->win->trig, e->win->thresholdG, e->win->xyz);
      r->send(s);
    } else if (r->hasParam("sample")) {
      app::Guard g;
      File f = dataFS.open(dataset::rawPath(r->getParam("sample")->value().toInt()), "r");
      RawHeader h;
      if (!f || f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.n > WIN_MAX_SAMPLES)
        return fail(r, "no raw data", 404);
      std::unique_ptr<int16_t[]> buf(new int16_t[h.n * 3]);
      f.read((uint8_t*)buf.get(), h.n * 3 * sizeof(int16_t));
      f.close();
      AsyncResponseStream* s = r->beginResponseStream("application/json");
      streamWindow(s, h.rateHz, h.n, h.trig, h.thresholdG, reinterpret_cast<const int16_t(*)[3]>(buf.get()));
      r->send(s);
    } else {
      fail(r, "event or sample");
    }
  });

  server.on("/api/samples", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncResponseStream* s = r->beginResponseStream("application/json");
    s->print("[");
    bool first = true;
    app::Guard g;
    dataset::forEach([&](const SampleRec& rec) {
      s->printf("%s{\"id\":%u,\"ts\":%u,\"label\":\"%s\",\"pred\":\"%s\",\"snap_p\":%.3f,\"raw\":%s,"
                "\"published\":%s,\"peak_g\":%.3f}",
                first ? "" : ",", (unsigned)rec.id, (unsigned)rec.ts,
                rec.label < classCount() ? className(rec.label) : "", className(rec.predCls), rec.snapP,
                (rec.flags & FLAG_RAW) ? "true" : "false", (rec.flags & FLAG_PUBLISHED) ? "true" : "false",
                powf(10, rec.feat[F_PEAK_LOG]));
      first = false;
    });
    s->print("]");
    r->send(s);
  });
  server.on("/api/samples/relabel", HTTP_POST, [](AsyncWebServerRequest* r) {
    const int l = parseLabel(param(r, "label"));
    if (l < 0) return fail(r, "label");
    app::relabelSample(param(r, "id").toInt(), l) ? ok(r) : fail(r, "id");
  });
  server.on("/api/samples/delete", HTTP_POST, [](AsyncWebServerRequest* r) {
    app::deleteSample(param(r, "id").toInt()) ? ok(r) : fail(r, "id");
  });
  server.on("/api/model", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    app::modelToJson(d.to<JsonObject>());
    sendJson(r, d);
  });
  server.on("/api/model/wipe", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    app::wipeDataset();
    ok(r);
  });

  server.on("/api/export.csv", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncResponseStream* s = r->beginResponseStream("text/csv");
    s->addHeader("Content-Disposition", "attachment; filename=polh-kill-samples.csv");
    s->print("id,ts,label,pred,snap_p,rate_hz,raw");
    for (int i = 0; i < NFEAT; i++) s->printf(",%s", FEATURE_NAMES[i]);
    s->print("\n");
    app::Guard g;
    dataset::forEach([&](const SampleRec& rec) {
      s->printf("%u,%u,%s,%s,%.3f,%u,%d", (unsigned)rec.id, (unsigned)rec.ts,
                rec.label < classCount() ? className(rec.label) : "", className(rec.predCls), rec.snapP, rec.rateHz,
                rec.flags & FLAG_RAW);
      for (int i = 0; i < NFEAT; i++) s->printf(",%.5f", rec.feat[i]);
      s->print("\n");
    });
    r->send(s);
  });

#ifdef POLH_SIM_SENSOR
  server.on("/api/sim/inject", HTTP_POST, [](AsyncWebServerRequest* r) {
    const int l = parseLabel(param(r, "label"));
    if (l < 0 || !app::accel()) return fail(r, "label");
    app::accel()->inject(l);
    ok(r);
  });
#endif
}

void routesSystem() {
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* r) { sendPage(r, PAGE_SETUP); });

  server.on("/api/setup", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    const String ssid = param(r, "ssid");
    if (!ssid.length()) return fail(r, "ssid");
    const String pass = param(r, "pass"), token = param(r, "token"), admin = param(r, "admin");
    if (pass.length() || ssid != settings.wifiSsid) settings.wifiPass = pass;
    settings.wifiSsid = ssid;
    if (token.length()) settings.tgToken = token;
    const String name = param(r, "name");
    if (name.length()) settings.deviceName = name;
    if (admin.length()) settings.adminPass = admin;
    const String ght = param(r, "gh_token"), ghr = param(r, "gh_repo");
    if (ght.length()) settings.ghToken = ght;
    if (ghr.indexOf('/') > 0) settings.ghRepo = ghr;
    settings.save();
    Serial.printf("[Setup 💾] saved WiFi '%s', rebooting\n", ssid.c_str());
    sendPage(r, PAGE_SAVED);
    rebootAt = millis() + 1500;
  });

  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("start")) net::startScan();
    r->send(200, "application/json", net::scanJson());
  });

  server.on("/api/github/push", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!github::configured()) return fail(r, "no GitHub token");
    github::pushNow();
    ok(r);
  });

  server.on("/api/telegram/test", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!telegram::configured()) return fail(r, "no token");
    telegram::sendTest();
    ok(r);
  });
  server.on("/api/telegram/unpair", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    telegram::unpairAll();
    ok(r);
  });
  server.on("/api/telegram/remove", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    if (!telegram::removeUser(param(r, "chat"))) return fail(r, "no such comrade");
    ok(r);
  });
  // ze black box: why did ze trap die last time? reset reason + backtrace of ze last panic
  // (core dump in flash). Decode: xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf <pc> <bt...>
  server.on("/api/crash", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    static const char* reasons[] = {"unknown", "power-on", "external", "software", "panic", "int-wdt",
                                    "task-wdt", "wdt", "deep-sleep", "brownout", "sdio", "usb", "jtag"};
    const int rr = (int)esp_reset_reason();
    d["reset_reason"] = rr >= 0 && rr < 13 ? reasons[rr] : "?";
    d["heap_free"] = ESP.getFreeHeap();
    d["heap_min"] = ESP.getMinFreeHeap();
    d["heap_max_block"] = ESP.getMaxAllocHeap();
    d["psram_free"] = ESP.getFreePsram();
    esp_core_dump_summary_t* s = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
    if (s && esp_core_dump_image_check() == ESP_OK && esp_core_dump_get_summary(s) == ESP_OK) {
      JsonObject c = d["coredump"].to<JsonObject>();
      c["task"] = s->exc_task;
      c["pc"] = String(s->exc_pc, HEX);
      c["exccause"] = s->ex_info.exc_cause;
      c["excvaddr"] = String(s->ex_info.exc_vaddr, HEX);
      c["corrupted"] = s->exc_bt_info.corrupted;
      JsonArray bt = c["bt"].to<JsonArray>();
      for (uint32_t i = 0; i < s->exc_bt_info.depth; i++) bt.add(String(s->exc_bt_info.bt[i], HEX));
    }
    free(s);
    sendJson(r, d);
  });
  // voice rating rounds (tools/voice_rate.py): send a test voice, read ze emoji votes
  server.on("/api/voice/test", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    const String url = param(r, "url"), sample = param(r, "sample");
    if (!url.startsWith("https://") || !sample.length() || sample.length() > 24) return fail(r, "url + sample needed");
    telegram::sendVoiceRated(url, param(r, "caption"), sample);
    ok(r);
  });
  server.on("/api/voice/ratings", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    telegram::ratingsToJson(d.to<JsonObject>());
    sendJson(r, d);
  });
  server.on("/api/voice/ratings/clear", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    telegram::clearRatings();
    ok(r);
  });
  server.on("/api/telegram/newcode", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    telegram::newCode();
    ok(r);
  });
  server.on("/api/portal", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    net::openPortal();
    ok(r);
  });
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    ok(r);
    rebootAt = millis() + 500;
  });

  // ---- web OTA: firmware or web UI image ----
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authed(r)) return;
    sendPage(r, PAGE_UPDATE);
  });
  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest* r) {
        if (!authed(r)) return;
        const bool good = !Update.hasError() && otaError.isEmpty();
        r->send(good ? 200 : 500, "text/plain", good ? "OK" : otaError);
        if (good) rebootAt = millis() + 800;
        else statusLed::mode = statusLed::ERROR;
      },
      [](AsyncWebServerRequest* r, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (settings.adminPass.length() && !r->authenticate("admin", settings.adminPass.c_str())) return;
        if (index == 0) {
          otaError = "";
          const bool fs = param(r, "type") == "fs";
          Serial.printf("[WebOTA ⚡] %s upload: %s\n", fs ? "web UI" : "firmware", filename.c_str());
          statusLed::mode = statusLed::OTA;
          if (fs) LittleFS.end();
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, fs ? U_SPIFFS : U_FLASH, -1, LOW, fs ? "spiffs" : nullptr))
            otaError = Update.errorString();
        }
        if (otaError.isEmpty() && Update.write(data, len) != len) otaError = Update.errorString();
        if (final) {
          if (otaError.isEmpty() && !Update.end(true)) otaError = Update.errorString();
          Serial.printf("[WebOTA ⚡] %s (%u bytes)\n", otaError.isEmpty() ? "success" : otaError.c_str(),
                        (unsigned)(index + len));
        }
      });

  // ---- captive portal probes (Android, iOS/macOS, Windows) ----
  const char* probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                          "/connecttest.txt", "/ncsi.txt", "/redirect", "/canonical.html", "/success.txt",
                          "/fwlink"};
  for (const char* p : probes) {
    server.on(p, HTTP_ANY, [](AsyncWebServerRequest* r) {
      if (net::portalActive()) return redirectToSetup(r);
      if (r->url().indexOf("204") >= 0) return r->send(204);
      r->send(200, "text/plain", "success");
    });
  }
}

void onWs(AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType type, void*, uint8_t*, size_t) {
  if (type == WS_EVT_CONNECT) Serial.printf("[WS] client #%u connected\n", (unsigned)c->id());
}

}  // namespace

// Live plot in its own task: loop() also stores captures on flash (up to 15 kB each), and while it
// did, ze live stream stood still for up to 0.9 s. Priority 2 > loop task (1): we send between
// ze flash operations. Every 100 ms: drain ze detector queue, one WebSocket message.
void liveTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(50));
    if (!detector.liveQueue) continue;
    if (!ws.count()) {
      xQueueReset(detector.liveQueue);
      continue;
    }
    if (!ws.availableForWriteAll() && uxQueueMessagesWaiting(detector.liveQueue) < 200) continue;
    LivePoint lp;
    String mv, mx, my, mz;
    int n = 0;
    while (xQueueReceive(detector.liveQueue, &lp, 0) == pdTRUE) {
      if (n++) { mv += ","; mx += ","; my += ","; mz += ","; }
      mv += String(lp.mag * 1000.0f, 1);
      mx += String(lp.dx);
      my += String(lp.dy);
      mz += String(lp.dz);
    }
    if (n && ws.availableForWriteAll()) {
      float w[3] = {0, 0, 0};
      float ori[3] = {0, 0, 0};
      float tC = NAN;
      if (app::accel()) {
        app::accel()->gyro(w);
        app::accel()->orientation(ori);
        tC = app::accel()->tempC();
      }
      String pkt = "{\"t\":\"live\",\"v\":[" + mv + "],\"x\":[" + mx + "],\"y\":[" + my + "],\"z\":[" + mz +
                   "],\"thr\":" + String(detector.thresholdG() * 1000, 1) +
                   ",\"base\":" + String(detector.baselineG() * 1000, 1) +
                   ",\"g\":[" + String(detector.gx(), 4) + "," + String(detector.gy(), 4) + "," + String(detector.gz(), 4) + "]" +
                   ",\"w\":[" + String(w[0], 1) + "," + String(w[1], 1) + "," + String(w[2], 1) + "]" +
                   ",\"ori\":[" + String(ori[0], 1) + "," + String(ori[1], 1) + "," + String(ori[2], 1) + "]";
      if (!isnan(tC)) pkt += ",\"temp\":" + String(tC, 1);
      pkt += "}";
      ws.textAll(pkt);
    }
  }
}

void begin() {
  uiOnFlash = LittleFS.begin(false) && LittleFS.exists("/index.html");
  Serial.printf("[Web] UI on flash: %s\n", uiOnFlash ? "yes" : "no (upload littlefs.bin)");

  ws.onEvent(onWs);
  server.addHandler(&ws);
  routesStatus();
  routesLab();
  routesSystem();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (net::portalActive() && !isOurHost(r)) return redirectToSetup(r);
    if (!uiOnFlash) return sendPage(r, PAGE_NO_UI);
    r->send(LittleFS, "/index.html", "text/html");
  });
  // the static handler does not know .mp3 -> Safari refuses octet-stream audio
  server.on("/interview.mp3", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!LittleFS.exists("/interview.mp3")) return r->send(404);
    r->send(LittleFS, "/interview.mp3", "audio/mpeg");
  });
  if (uiOnFlash) server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=60");

  server.onNotFound([](AsyncWebServerRequest* r) {
    if (net::portalActive() && !isOurHost(r)) return redirectToSetup(r);
    if (r->method() == HTTP_OPTIONS) return r->send(204);
    r->send(404, "text/plain", "404 - this page is pure ideology, it does not exist");
  });
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  xTaskCreatePinnedToCore(liveTask, "live", 4096, nullptr, 2, nullptr, 1);
  Serial.println("[Web 🌐] server on port 80");
}

void loop() {
  static uint32_t lastTick = 0, lastClean = 0;
  const uint32_t now = millis();

  if (rebootAt && now > rebootAt) {
    Serial.println("[System] rebooting…");
    delay(100);
    ESP.restart();
  }

  if (now - lastTick >= 1000) {
    lastTick = now;
    if (ws.count()) notifyLab();
  }
  if (now - lastClean >= 2000) {
    lastClean = now;
    ws.cleanupClients();
  }
}

void notifyEvent(const RecentEvent& e) {
  if (!ws.count()) return;
  JsonDocument d;
  d["t"] = "event";
  {
    app::Guard g;
    app::eventToJson(e, d["e"].to<JsonObject>());
  }
  String s;
  serializeJson(d, s);
  ws.textAll(s);
}

void notifyLab() {
  if (!ws.count()) return;
  const int l = app::testLabel();
  String s = "{\"t\":\"lab\",\"armed_label\":";
  s += l >= 0 ? "\"" + String(className(l)) + "\"" : "null";
  s += ",\"remaining_ms\":" + String(app::testRemainingMs()) + ",\"samples\":" + String(app::model().total()) +
       ",\"gravity\":[" + String(detector.gx(), 3) + "," + String(detector.gy(), 3) + "," +
       String(detector.gz(), 3) + "]}";
  ws.textAll(s);
}

}  // namespace web
