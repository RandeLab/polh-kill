#include "telegram.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <vector>
#include <Preferences.h>
#include "features.h"
#include "github.h"
#include "net.h"
#include "settings.h"

namespace telegram {
namespace {

struct Outgoing {
  String text, markup, voice;  // voice = URL of an OGG/Opus file -> sendVoice with text as caption
};

QueueHandle_t outQ = nullptr;
int64_t lastUpdateId = 0;
String bot, lastErr;
bool bootSent = false;
int lastReportDay = -1;

// ---------------- ze comrades (subscribers) ----------------
// One bot, many humans. subs[0] is ze owner: ze first one who said /start.
// Everybody else gets in by ze owner's approval button or with ze join code.
struct Sub {
  String chat, name;
};
std::vector<Sub> subs, pending;
SemaphoreHandle_t subsMx = nullptr;
struct SubsLock {
  SubsLock() { xSemaphoreTakeRecursive(subsMx, portMAX_DELAY); }
  ~SubsLock() { xSemaphoreGiveRecursive(subsMx); }
};

void loadSubs() {
  SubsLock l;
  subs.clear();
  String ids = settings.tgChat, names = settings.tgNames;
  while (ids.length()) {
    const int c = ids.indexOf(',');
    String id = c < 0 ? ids : ids.substring(0, c);
    ids = c < 0 ? String() : ids.substring(c + 1);
    const int n = names.indexOf('\n');
    String name = n < 0 ? names : names.substring(0, n);
    names = n < 0 ? String() : names.substring(n + 1);
    id.trim();
    if (name == "null") name = "";
    if (id.length()) subs.push_back({id, name.length() ? name : (subs.empty() ? String("owner") : id)});
  }
}

void saveSubs() {
  SubsLock l;
  String ids, names;
  for (size_t i = 0; i < subs.size(); i++) {
    if (i) {
      ids += ",";
      names += "\n";
    }
    ids += subs[i].chat;
    names += subs[i].name;
  }
  settings.tgChat = ids;
  settings.tgNames = names;
  settings.save();
}

int findSub(const String& chat) {
  SubsLock l;
  for (size_t i = 0; i < subs.size(); i++)
    if (subs[i].chat == chat) return i;
  return -1;
}
bool isOwner(const String& chat) {
  SubsLock l;
  return !subs.empty() && subs[0].chat == chat;
}
String ownerChat() {
  SubsLock l;
  return subs.empty() ? String() : subs[0].chat;
}
std::vector<String> allChats() {
  SubsLock l;
  std::vector<String> v;
  for (auto& s : subs) v.push_back(s.chat);
  return v;
}

void ensureCode() {
  if (settings.tgCode.length() == 6) return;
  settings.tgCode = String(100000 + esp_random() % 900000);
  settings.save();
}

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

int postLocked(const String& method, const String& body, String* response);
int post(const String& method, const String& body, String* response = nullptr) {
  net::tlsLock();
  const int code = postLocked(method, body, response);
  net::tlsUnlock();
  return code;
}

int postLocked(const String& method, const String& body, String* response) {
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

void sendAll(const String& text, const String& markup) {
  for (auto& c : allChats()) sendNow(c, text, markup);
}

void sendVoiceNow(const String& chat, const String& url, const String& caption) {
  const String body = "{\"chat_id\":" + jsonStr(chat) + ",\"voice\":" + jsonStr(url) + ",\"caption\":" +
                      jsonStr(caption) + ",\"parse_mode\":\"HTML\"}";
  if (post("sendVoice", body) == 200) Serial.println("[Telegram 🎙] voice delivered");
}

// ---------------- ze voice of ze commits ----------------
// .github/workflows/voice.yml renders every commit on main as OGG onto GitHub Pages and lists
// them in voice/index.json (newest first). We look every 10 min and send ze new ones.
int getLocked(const String& url, String* response) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return -1;
  const int code = http.GET();
  if (code == 200) *response = http.getString();
  http.end();
  return code;
}

void checkCommitVoices() {
  const String base = github::pagesUrl();
  if (!base.length() || !settings.voiceMsgs || allChats().empty()) return;
  String resp;
  net::tlsLock();
  const int code = getLocked(base + "voice/index.json?t=" + String(millis()), &resp);
  net::tlsUnlock();
  if (code != 200) {
    Serial.printf("[Telegram 🎙] voice index HTTP %d\n", code);
    return;
  }
  JsonDocument filter;
  filter["commits"][0]["sha"] = true;
  filter["commits"][0]["subject"] = true;
  filter["commits"][0]["file"] = true;
  JsonDocument d;
  if (deserializeJson(d, resp, DeserializationOption::Filter(filter))) return;
  JsonArray list = d["commits"].as<JsonArray>();
  if (list.size() == 0) return;

  Preferences p;
  p.begin("polh", false);
  const String last = p.getString("tg_voice", "");
  // collect everything newer than ze last one we spoke (list is newest first); first time: only ze newest
  std::vector<int> todo;
  for (size_t i = 0; i < list.size() && todo.size() < 3; i++) {
    if (list[i]["sha"].as<String>() == last) break;
    todo.push_back(i);
    if (!last.length()) break;
  }
  for (int k = (int)todo.size() - 1; k >= 0; k--) {
    JsonObject c = list[todo[k]];
    const String sha = c["sha"].as<String>();
    const String cap = "🎙 <b>" + esc(c["subject"].as<String>()) + "</b>\n<a href=\"https://github.com/" +
                       settings.ghRepo + "/commit/" + sha + "\">" + sha + "</a> · ze philosopher reads ze commit";
    for (auto& chat : allChats()) sendVoiceNow(chat, base + c["file"].as<String>(), cap);
  }
  p.putString("tg_voice", list[0]["sha"].as<String>());
  p.end();
}

bool addSub(const String& chat, const String& name) {
  SubsLock l;
  if (findSub(chat) >= 0) return true;
  if ((int)subs.size() >= MAX_TG_USERS) return false;
  subs.push_back({chat, name});
  for (size_t i = 0; i < pending.size(); i++)
    if (pending[i].chat == chat) {
      pending.erase(pending.begin() + i);
      break;
    }
  saveSubs();
  return true;
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
         "/ping - are you alive?\n"
         "/users - who listens to ze trap\n"
         "/code - ze join code for new comrades (owner)\n"
         "/leave - stop listening";
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
  } else if (cmd.startsWith("/users")) {
    String t = "👥 <b>Ze comrades of ze trap</b>\n";
    String kb = "{\"inline_keyboard\":[";
    bool any = false;
    {
      SubsLock l;
      const bool owner = isOwner(chat);
      for (size_t i = 0; i < subs.size(); i++) {
        t += String(i == 0 ? "👑 " : "• ") + esc(subs[i].name) + (subs[i].chat == chat ? " (you)" : "") + "\n";
        if (i > 0 && owner) {
          if (any) kb += ",";
          kb += "[{\"text\":" + jsonStr("🚪 remove " + subs[i].name.substring(0, 24)) +
                ",\"callback_data\":\"r:" + subs[i].chat + "\"}]";
          any = true;
        }
      }
      if (!pending.empty()) t += "⏳ knocking: " + String(pending.size()) + "\n";
    }
    t += String(MAX_TG_USERS) + " places in total. Like a commune, but with a waiting list.";
    sendNow(chat, t, any ? kb + "]}" : String());
  } else if (cmd.startsWith("/code")) {
    if (!isOwner(chat)) {
      sendNow(chat, "🔒 Only ze owner knows ze code. Hierarchy, my friend, is also an ideology.", "");
    } else {
      ensureCode();
      const String link =
          bot.length() ? "\nOr simply send them this link:\nhttps://t.me/" + bot + "?start=" + settings.tgCode : String();
      sendNow(chat, "🔑 Ze join code is <code>" + settings.tgCode + "</code>.\nA new comrade sends <code>/start " +
                        settings.tgCode + "</code> to ze bot and is in, without asking you." + link, "");
    }
  } else if (cmd.startsWith("/leave")) {
    if (isOwner(chat)) {
      sendNow(chat, "👑 Ze owner cannot leave. Unpair in ze web UI, if you really must destroy ze commune.", "");
    } else {
      {
        SubsLock l;
        const int i = findSub(chat);
        if (i > 0) subs.erase(subs.begin() + i);
        saveSubs();
      }
      sendNow(chat, "👋 You are free now. Which is, of course, ze most terrible thing. /start to come back.", "");
    }
  } else if (cmd.startsWith("/ping")) {
    sendNow(chat, "🏓 <b>Pong!</b> I am here. Uptime " + app::uptimeText(millis()) + ".", "");
  } else {
    sendNow(chat, "🤔 I do not understand. But not understanding is ze beginning of philosophy. /help", "");
  }
}

void handleCallback(const String& chat, const String& cbId, const String& data) {
  String answer = "?";
  // ze owner decides about strangers: "j:<chat>:1|0" approve/deny, "r:<chat>" remove
  if ((data.startsWith("j:") || data.startsWith("r:")) && !isOwner(chat)) {
    answer = "Only ze owner decides";
  } else if (data.startsWith("j:")) {
    const int b = data.lastIndexOf(':');
    const String who = data.substring(2, b);
    const bool yes = data.substring(b + 1) == "1";
    String name = who;
    {
      SubsLock l;
      for (size_t i = 0; i < pending.size(); i++)
        if (pending[i].chat == who) {
          name = pending[i].name;
          if (!yes) pending.erase(pending.begin() + i);
          break;
        }
    }
    if (yes) {
      if (addSub(who, name)) {
        answer = "Let in ✅";
        sendNow(who, "🤝 <b>Welcome, comrade.</b> Ze owner has let you in. From now on you get every snap. /help", "");
      } else {
        answer = "Full! " + String(MAX_TG_USERS) + " places";
      }
    } else {
      answer = "Rejected";
      sendNow(who, "🚫 Ze owner said no. Do not take it personally, it is only ze structure.", "");
    }
  } else if (data.startsWith("r:")) {
    const String who = data.substring(2);
    String gone;
    {
      SubsLock l;
      const int i = findSub(who);
      if (i > 0) {
        gone = subs[i].name;
        subs.erase(subs.begin() + i);
        saveSubs();
      }
    }
    if (gone.length()) {
      answer = "Removed " + gone;
      sendNow(who, "🚪 Ze owner has removed you from ze trap. And so on, and so on.", "");
    } else {
      answer = "Not here anymore";
    }
  }
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
  filter["result"][0]["message"]["chat"]["first_name"] = true;
  filter["result"][0]["message"]["chat"]["username"] = true;
  filter["result"][0]["message"]["chat"]["title"] = true;
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

    const String text = u["message"]["text"].as<String>();
    // as<String>() of a missing field is "null" - ze nothingness has a name, but not this one
    String name = u["message"]["chat"]["title"] | "";  // groups have a title
    if (!name.length()) name = u["message"]["chat"]["first_name"] | "";
    if (!name.length()) name = u["message"]["chat"]["username"] | "";
    if (!name.length()) name = chat;
    const bool start = text.startsWith("/start") || text.startsWith("/join");

    // pairing: the first chat that says /start owns the trap
    if (findSub(chat) < 0 && start && !ownerChat().length()) {
      addSub(chat, name);
      ensureCode();
      sendNow(chat, "🤝 <b>Paired.</b> You are ze owner now. Others can join: they send /start and you decide, "
                    "or they use ze join code (/code). This is not love, this is a protocol.", "");
      continue;
    }
    if (findSub(chat) < 0) {
      if (!start) {
        Serial.printf("[Telegram 🚫] ignoring chat %s\n", chat.c_str());
        continue;
      }
      const int sp = text.indexOf(' ');
      String code = sp > 0 ? text.substring(sp + 1) : String();
      code.trim();
      if (code.length() && code == settings.tgCode) {
        if (addSub(chat, name)) {
          sendNow(chat, "🤝 <b>Ze code is correct, welcome comrade.</b> From now on you get every snap. /help", "");
          sendNow(ownerChat(), "👋 <b>" + esc(name) + "</b> joined with ze code. /users", "");
        } else {
          sendNow(chat, "😬 Ze trap is full, " + String(MAX_TG_USERS) + " comrades already. Ask ze owner.", "");
        }
        continue;
      }
      bool known = false;
      {
        SubsLock l;
        for (auto& pq : pending) known |= pq.chat == chat;
        if (!known) {
          if (pending.size() >= 6) pending.erase(pending.begin());
          pending.push_back({chat, name});
        }
      }
      sendNow(chat, "🚪 <b>Knock knock.</b> I have asked ze owner of this trap. Wait a little, "
                    "or ask them for ze join code and send <code>/start CODE</code>.", "");
      if (!known)
        sendNow(ownerChat(), "🙋 <b>" + esc(name) + "</b> wants to listen to ze trap. Let them in?",
                "{\"inline_keyboard\":[[{\"text\":\"✅ let in\",\"callback_data\":\"j:" + chat +
                    ":1\"},{\"text\":\"🚫 no\",\"callback_data\":\"j:" + chat + ":0\"}]]}");
      continue;
    }
    if (u["callback_query"].is<JsonObject>()) {
      handleCallback(chat, u["callback_query"]["id"].as<String>(), u["callback_query"]["data"].as<String>());
    } else if (text.startsWith("/")) {
      handleCommand(chat, text);
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
    sendAll(diaryText(), "");
  }
}

void task(void*) {
  uint32_t lastPoll = 0, lastVoice = 0, onlineSince = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!configured() || !net::online() || net::otaInProgress()) continue;
    if (!bot.length()) fetchBotName();
    if (!bootSent && ownerChat().length()) {
      bootSent = true;
      // after a crash we reboot quietly: a crash loop must not become a message loop
      const esp_reset_reason_t rr = esp_reset_reason();
      if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT ||
          rr == ESP_RST_BROWNOUT)
        continue;
      sendNow(ownerChat(), bootText(), "");  // only ze owner: ze others do not need every reboot
    }
    Outgoing* m = nullptr;
    while (xQueueReceive(outQ, &m, 0) == pdTRUE) {
      if (m->voice.length())
        for (auto& c : allChats()) sendVoiceNow(c, m->voice, m->text);
      else
        sendAll(m->text, m->markup);
      delete m;
    }
    if (!onlineSince) onlineSince = millis();
    // first look 2 min after we are online (ze Action needs ~1 min after a push), then every 10 min
    if (millis() - onlineSince > 120000 && (!lastVoice || millis() - lastVoice > 600000)) {
      lastVoice = millis();
      checkCommitVoices();
    }
    if (millis() - lastPoll > 3500) {
      lastPoll = millis();
      pollUpdates();
      dailyReport();
    }
  }
}

}  // namespace

void begin() {
  subsMx = xSemaphoreCreateRecursiveMutex();
  loadSubs();
  ensureCode();
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
  // not every vibration deserves a message: snap alerts max 1 per minute, questions max 1 per 10 min
  static uint32_t lastAlert = 0, lastAsk = 0;
  const uint32_t now = millis();
  const float p = e.pred.snapProb;
  if (p >= settings.alertProb) {
    if (lastAlert && now - lastAlert < 60000) return;
    lastAlert = now;
  } else if (settings.notifyUnsure && p >= settings.unsureProb) {
    if (lastAsk && now - lastAsk < 600000) return;
    lastAsk = now;
  }
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

void sendVoice(const String& oggUrl, const String& captionHtml) {
  if (!configured()) return;
  Outgoing* m = new Outgoing{captionHtml, "", oggUrl};
  if (xQueueSend(outQ, &m, 0) != pdTRUE) delete m;
}

void sendTest() {
  send("🧪 <b>Test message.</b> If you can read this, ze whole chain works: ESP32, WiFi, Telegram, you. "
       "Which is already more than most ideologies can say.");
}

String botName() { return bot; }

void usersToJson(JsonObject t) {
  SubsLock l;
  t["paired"] = !subs.empty();
  t["join_code"] = settings.tgCode;
  t["max_users"] = MAX_TG_USERS;
  t["pending"] = pending.size();
  JsonArray a = t["users"].to<JsonArray>();
  for (size_t i = 0; i < subs.size(); i++) {
    JsonObject u = a.add<JsonObject>();
    u["chat"] = subs[i].chat;
    u["name"] = subs[i].name;
    u["owner"] = i == 0;
  }
}

bool removeUser(const String& chat) {
  SubsLock l;
  const int i = findSub(chat);
  if (i < 0) return false;
  subs.erase(subs.begin() + i);
  saveSubs();
  return true;
}

void unpairAll() {
  SubsLock l;
  subs.clear();
  pending.clear();
  saveSubs();
}

void newCode() {
  settings.tgCode = "";
  ensureCode();
}
String lastError() { return lastErr; }

}  // namespace telegram
