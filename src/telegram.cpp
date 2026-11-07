#include "telegram.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "features.h"
#include "net.h"
#include "settings.h"

namespace telegram {
namespace {

struct Outgoing {
  String text, markup;
};

QueueHandle_t outQ = nullptr;
int64_t lastUpdateId = 0;
String bot, lastErr;
bool bootSent = false;
int lastReportDay = -1;

String esc(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}

String jsonStr(const String& s) {
  String o;
  JsonDocument d;
  d.set(s);
  serializeJson(d, o);
  return o;
}

String api(const String& method) { return "https://api.telegram.org/bot" + settings.tgToken + "/" + method; }

int post(const String& method, const String& body, String* response = nullptr) {
  WiFiClientSecure client;
  client.setInsecure();  // no CA pinning: Telegram rotates certs, the token is the secret
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, api(method))) return -1;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  if (response && code > 0) *response = http.getString();
  if (code != 200) {
    lastErr = String(method) + " HTTP " + code;
    Serial.printf("[Telegram ❌] %s\n", lastErr.c_str());
  } else {
    lastErr = "";
  }
  http.end();
  return code;
}

void sendNow(const String& chat, const String& text, const String& markup) {
  String body = "{\"chat_id\":" + jsonStr(chat) + ",\"text\":" + jsonStr(text) +
                ",\"parse_mode\":\"HTML\",\"disable_web_page_preview\":true";
  if (markup.length()) body += ",\"reply_markup\":" + markup;
  body += "}";
  if (post("sendMessage", body) == 200) Serial.println("[Telegram ✈️] message delivered");
}

String labelKeyboard(uint32_t eventId) {
  String k = "{\"inline_keyboard\":[[";
  for (int c = 0; c < classCount(); c++) {
    if (c > 0 && c % 3 == 0) k += "],[";
    else if (c > 0) k += ",";
    k += "{\"text\":\"" + String(classEmoji(c)) + " " + className(c) + "\",\"callback_data\":\"l:" +
         eventId + ":" + c + "\"}";
  }
  return k + "]]}";
}

String fmtPct(float p) { return String((int)roundf(p * 100)) + " %"; }

String webLink() {
  const String ip = net::ip();
  return ip.length() ? "http://" + ip + "/" : String("(no IP)");
}

// ---------------- the voice ----------------

String statusText() {
  const Stats s = app::stats();
  String t = "📊 <b>" + esc(settings.deviceName) + " status</b>\n";
  t += settings.armed ? "🎯 armed, waiting\n" : "😴 disarmed, no alerts\n";
  t += "⏱ uptime " + app::uptimeText(millis()) + "\n";
  t += "🧲 sensor: " + String(app::sensorOk() ? app::accel()->name() : "❌ not found") + "\n";
  t += "〰️ noise floor " + String(detector.baselineG() * 1000, 1) + " mg, trigger " +
       String(detector.thresholdG() * 1000, 0) + " mg\n";
  t += "📈 events since boot: " + String(s.events) + ", snaps total: " + String(s.snapsTotal) + "\n";
  if (s.lastSnapMs) t += "💥 last snap " + app::uptimeText(millis() - s.lastSnapMs) + " ago\n";
  t += "🧠 model: " + String(app::model().total()) + " labelled samples\n";
  t += "📶 WiFi " + esc(settings.wifiSsid) + " (" + String(net::rssi()) + " dBm)\n";
  t += "🌐 " + webLink();
  return t;
}

String diaryText() {
  const Stats s = app::stats();
  const uint32_t now = millis();
  String t = "🐭 <b>polh-kill diary • how am I</b>\n━━━━━━━━━━━━━━━━━━━━\n";
  if (!settings.armed) {
    t += "I am disarmed. Imagine: a trap which is not allowed to be a trap. This is, how you say, "
         "ze trap without ze Real of ze trap. Like decaffeinated coffee, like beer without alcohol. ☕\n";
  } else if (s.lastSnapMs && now - s.lastSnapMs < 6UL * 3600 * 1000) {
    t += "Something happened, my friend. " + app::uptimeText(now - s.lastSnapMs) +
         " ago, ze spring went <b>CLACK</b>. And now there is this silence after ze event, "
         "which is much more terrible than ze event itself. Please, go and look. 💥\n";
  } else if (!s.lastEventMs || now - s.lastEventMs > 12UL * 3600 * 1000) {
    t += "Nothing happens. Absolutely nothing. And you know, this nothing is not empty, "
         "it is full of potential mice. I am waiting, like ze subject waits for ze big Other. *sniff* 🧀\n";
  } else {
    t += "Some small vibrations, " + String(s.events) + " events since I woke up. Footsteps, bumps, "
         "maybe a mouse who is only pretending to be interested in ze cheese. Typical ideology. 🤔\n";
  }
  t += "\n〰️ My noise floor is " + String(detector.baselineG() * 1000, 1) +
       " mg. This is ze sound of ze house breathing.";
  t += "\n🧠 I know " + String(app::model().total()) + " labelled vibrations. ";
  t += app::model().total() < 10 ? "Not enough! Teach me in ze learning lab." : "And so on, and so on.";
  t += "\n💥 Snaps total: " + String(s.snapsTotal);
  t += "\n━━━━━━━━━━━━━━━━━━━━\n<i>Directly from ze ESP32-S3 silicon brain</i>";
  return t;
}

String helpText() {
  return "🐭 <b>polh-kill commands</b>\n"
         "/status - what is going on\n"
         "/howareyou - my diary (also /wiegehts)\n"
         "/arm - send alerts\n"
         "/disarm - be quiet (e.g. while resetting ze trap)\n"
         "/stats - numbers, pure numbers\n"
         "/lab - link to ze learning lab\n"
         "/ping - are you alive?";
}

String bootText() {
  return "🐭⚡ <b>" + esc(settings.deviceName) + " is awake</b> (v" POLH_VERSION ")\n" +
         "🧲 " + String(app::sensorOk() ? app::accel()->name() : "sensor missing!") + " · 🧠 " +
         String(app::model().total()) + " samples learned\n" + "🌐 " + webLink() +
         "\nI am sitting here, waiting. Like a mouse, but on ze other side of ideology. /help";
}

// ---------------- incoming ----------------

void handleCommand(const String& chat, String cmd) {
  cmd.trim();
  const int at = cmd.indexOf('@');  // /status@PolhKillBot
  const int sp = cmd.indexOf(' ');
  if (at > 0 && (sp < 0 || at < sp)) cmd = cmd.substring(0, at);
  cmd.toLowerCase();
  Serial.printf("[Telegram 📩] %s\n", cmd.c_str());

  if (cmd.startsWith("/start") || cmd.startsWith("/help")) {
    sendNow(chat, helpText(), "");
  } else if (cmd.startsWith("/status")) {
    sendNow(chat, statusText(), "");
  } else if (cmd.startsWith("/howareyou") || cmd.startsWith("/wiegehts") || cmd.startsWith("/diary")) {
    sendNow(chat, diaryText(), "");
  } else if (cmd.startsWith("/arm")) {
    settings.armed = true;
    settings.save();
    sendNow(chat, "🎯 <b>Armed.</b> Now I am again a real trap, not only ze idea of a trap.", "");
  } else if (cmd.startsWith("/disarm")) {
    settings.armed = false;
    settings.save();
    sendNow(chat, "😴 <b>Disarmed.</b> I will not say anything. Which is also a statement, of course.", "");
  } else if (cmd.startsWith("/stats")) {
    const Stats s = app::stats();
    String t = "📈 <b>pure numbers</b>\nevents since boot: " + String(s.events) +
               "\nsnaps since boot: " + String(s.snapsBoot) + "\nsnaps total: " + String(s.snapsTotal) +
               "\nlabelled samples: " + String(app::model().total());
    for (int c = 0; c < classCount(); c++)
      t += "\n  " + String(classEmoji(c)) + " " + className(c) + ": " + app::model().count(c);
    sendNow(chat, t, "");
  } else if (cmd.startsWith("/lab")) {
    sendNow(chat, "🧪 Ze learning lab: " + webLink() + "#lab\n(only from inside ze same WiFi)", "");
  } else if (cmd.startsWith("/ping")) {
    sendNow(chat, "🏓 <b>Pong!</b> I am here. Uptime " + app::uptimeText(millis()) + ".", "");
  } else {
    sendNow(chat, "🤔 I do not understand. But not understanding is ze beginning of philosophy. /help", "");
  }
}

void handleCallback(const String& chat, const String& cbId, const String& data) {
  String answer = "?";
  // data = "l:<eventId>:<class>"
  const int a = data.indexOf(':'), b = data.lastIndexOf(':');
  if (data.startsWith("l:") && b > a) {
    const uint32_t eventId = data.substring(a + 1, b).toInt();
    const int cls = data.substring(b + 1).toInt();
    const uint32_t sid = app::labelEvent(eventId, cls);
    if (sid) {
      answer = String("Learned: ") + className(cls) + " ✅";
      sendNow(chat, "🧠 Thank you. Event #" + String(eventId) + " is now officially a <b>" +
                        className(cls) + "</b>. I have " + String(app::model().total()) +
                        " samples now, my worldview is updated.", "");
    } else {
      answer = "Event is too old, gone from memory";
    }
  }
  post("answerCallbackQuery", "{\"callback_query_id\":" + jsonStr(cbId) + ",\"text\":" + jsonStr(answer) + "}");
}

void pollUpdates() {
  String resp;
  const String body = "{\"offset\":" + String((long long)(lastUpdateId + 1)) +
                      ",\"timeout\":0,\"allowed_updates\":[\"message\",\"callback_query\"]}";
  if (post("getUpdates", body, &resp) != 200) return;

  JsonDocument filter;
  filter["result"][0]["update_id"] = true;
  filter["result"][0]["message"]["chat"]["id"] = true;
  filter["result"][0]["message"]["text"] = true;
  filter["result"][0]["callback_query"]["id"] = true;
  filter["result"][0]["callback_query"]["data"] = true;
  filter["result"][0]["callback_query"]["message"]["chat"]["id"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, resp, DeserializationOption::Filter(filter))) return;

  for (JsonObject u : doc["result"].as<JsonArray>()) {
    const int64_t uid = u["update_id"].as<int64_t>();
    if (uid > lastUpdateId) lastUpdateId = uid;
    String chat;
    if (u["message"].is<JsonObject>()) chat = u["message"]["chat"]["id"].as<String>();
    else if (u["callback_query"].is<JsonObject>()) chat = u["callback_query"]["message"]["chat"]["id"].as<String>();
    if (!chat.length()) continue;

    // pairing: the first chat that says /start owns the trap
    if (!settings.tgChat.length() && u["message"]["text"].as<String>().startsWith("/start")) {
      settings.tgChat = chat;
      settings.save();
      sendNow(chat, "🤝 <b>Paired.</b> From now on I talk only to you. This is not love, this is a protocol.", "");
    }
    if (chat != settings.tgChat) {
      Serial.printf("[Telegram 🚫] ignoring chat %s\n", chat.c_str());
      continue;
    }
    if (u["callback_query"].is<JsonObject>()) {
      handleCallback(chat, u["callback_query"]["id"].as<String>(), u["callback_query"]["data"].as<String>());
    } else {
      const String text = u["message"]["text"].as<String>();
      if (text.startsWith("/")) handleCommand(chat, text);
    }
  }
}

void fetchBotName() {
  String resp;
  if (post("getMe", "{}", &resp) != 200) return;
  JsonDocument d;
  if (!deserializeJson(d, resp)) bot = d["result"]["username"].as<String>();
  Serial.printf("[Telegram 🤖] bot @%s\n", bot.c_str());
}

void dailyReport() {
  if (!settings.dailyReport) return;
  struct tm t;
  if (!getLocalTime(&t, 0)) return;
  if (t.tm_hour == 20 && t.tm_mday != lastReportDay) {
    lastReportDay = t.tm_mday;
    sendNow(settings.tgChat, diaryText(), "");
  }
}

void task(void*) {
  uint32_t lastPoll = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!configured() || !net::online() || net::otaInProgress()) continue;
    if (!bot.length()) fetchBotName();
    if (!bootSent && settings.tgChat.length()) {
      bootSent = true;
      sendNow(settings.tgChat, bootText(), "");
    }
    Outgoing* m = nullptr;
    while (xQueueReceive(outQ, &m, 0) == pdTRUE) {
      if (settings.tgChat.length()) sendNow(settings.tgChat, m->text, m->markup);
      delete m;
    }
    if (millis() - lastPoll > 3500) {
      lastPoll = millis();
      pollUpdates();
      if (settings.tgChat.length()) dailyReport();
    }
  }
}

}  // namespace

void begin() {
  outQ = xQueueCreate(8, sizeof(Outgoing*));
  xTaskCreatePinnedToCore(task, "telegram", 12288, nullptr, 1, nullptr, 0);
}

bool configured() { return settings.tgToken.length() > 20; }

void send(const String& html, const String& markup) {
  if (!configured()) return;
  Outgoing* m = new Outgoing{html, markup};
  if (xQueueSend(outQ, &m, 0) != pdTRUE) delete m;
}

void onEvent(const RecentEvent& e) {
  if (!settings.armed) return;
  const float p = e.pred.snapProb;
  const float peak = powf(10, e.feat[F_PEAK_LOG]);
  if (p >= settings.alertProb) {
    send("💥 <b>TRAP SNAPPED!</b>\n\nAnd so on, and so on... ze trap has spoken. Confidence <b>" + fmtPct(p) +
             "</b>, peak " + String(peak, 2) + " g.\n\nThis is not a tragedy, my friend, this is ze pure "
             "ideology of ze cheese. 🧀\n\n<i>Was it really a snap? Teach me:</i>",
         labelKeyboard(e.win->id));
  } else if (settings.notifyUnsure && p >= settings.unsureProb) {
    send("🤔 <b>Something happened.</b> *sniff*\nPeak " + String(peak, 2) + " g. I think it was <b>" +
             className(e.pred.cls) + "</b> (snap " + fmtPct(p) + "). But what is a " +
             className(e.pred.cls) + ", really? Tell me, so I learn:",
         labelKeyboard(e.win->id));
  }
}

void sendTest() {
  send("🧪 <b>Test message.</b> If you can read this, ze whole chain works: ESP32, WiFi, Telegram, you. "
       "Which is already more than most ideologies can say.");
}

String botName() { return bot; }
String lastError() { return lastErr; }

}  // namespace telegram
