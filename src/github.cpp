#include "github.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <time.h>
#include <vector>
#include "app.h"
#include "dataset.h"
#include "features.h"
#include "net.h"
#include <algorithm>
#include "settings.h"
#include "telegram.h"
#include "status_led.h"

namespace github {
namespace {

constexpr int BATCH_MAX = 20;              // capture files per commit
constexpr uint32_t RETRY_MS = 5 * 60000;   // after an error
constexpr uint32_t BOOT_DELAY_MS = 60000;  // first push after boot

volatile bool pending = true, snapSoon = false, manual = false;
uint32_t lastTry = 0, lastOk = 0;
time_t lastOkTs = 0;
String lastErr, lastMsg;
uint32_t pushedTotal = 0;
int backlog = -1;  // captures not yet published (-1 = unknown)

struct FileOut {
  String path, sha;
};

String api(const String& path) { return "https://api.github.com/repos/" + settings.ghRepo + path; }

int callOnce(const char* method, const String& path, const String& body, String* resp) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(20000);
  if (!http.begin(client, api(path))) return -1;
  http.addHeader("Authorization", "Bearer " + settings.ghToken);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  http.addHeader("User-Agent", "polh-kill");
  if (body.length()) http.addHeader("Content-Type", "application/json");
  const int code = http.sendRequest(method, body);
  if (resp && code > 0) *resp = http.getString();
  http.end();
  return code;
}

// Ten TLS handshakes in a row: sometimes one simply does not happen (code < 0),
// like a revolution that was scheduled but nobody came. So we try again.
// GitHub's 5xx get a second chance too; 4xx are final, that is ze law.
int call(const char* method, const String& path, const String& body, String* resp) {
  int code = -1;
  for (int attempt = 0; attempt < 3; attempt++) {
    code = callOnce(method, path, body, resp);
    if (code > 0 && code < 500) return code;
    Serial.printf("[GitHub ⚠️] %s %s -> %d, retry %d\n", method, path.c_str(), code, attempt + 1);
    vTaskDelay(pdMS_TO_TICKS(1500 * (attempt + 1)));
  }
  return code;
}

String shaOf(const String& json, const char* key = "sha") {
  JsonDocument d;
  JsonDocument filter;
  filter[key] = true;
  filter["object"]["sha"] = true;
  filter["tree"]["sha"] = true;
  if (deserializeJson(d, json, DeserializationOption::Filter(filter))) return "";
  if (!strcmp(key, "object")) return d["object"]["sha"].as<String>();
  if (!strcmp(key, "tree")) return d["tree"]["sha"].as<String>();
  return d["sha"].as<String>();
}

// upload one file content as a blob, returns its sha
String blob(const String& content) {
  size_t outLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outLen, (const uint8_t*)content.c_str(), content.length());
  std::unique_ptr<char[]> b64(new (std::nothrow) char[outLen + 1]);
  if (!b64) return "";
  mbedtls_base64_encode((uint8_t*)b64.get(), outLen + 1, &outLen, (const uint8_t*)content.c_str(), content.length());
  b64[outLen] = 0;
  String body;
  body.reserve(outLen + 48);
  body += "{\"encoding\":\"base64\",\"content\":\"";
  body += b64.get();
  body += "\"}";
  String resp;
  const int code = call("POST", "/git/blobs", body, &resp);
  if (code != 201) { lastErr = "blob HTTP " + String(code); return ""; }
  return shaOf(resp);
}

String captureJson(const SampleRec& r, const RawHeader& h, const int16_t* xyz) {
  String s;
  s.reserve(h.n * 3 * 6 + 1024);
  s += "{\"id\":" + String(r.id) + ",\"ts\":" + String(r.ts) + ",\"label\":\"" +
       (r.label < classCount() ? className(r.label) : "") + "\",\"pred\":\"" + className(r.predCls) +
       "\",\"snap_p\":" + String(r.snapP, 3) + ",\"method\":\"" + METHOD_NAMES[r.method % 3] +
       "\",\"rate\":" + h.rateHz + ",\"trig\":" + h.trig + ",\"threshold_g\":" + String(h.thresholdG, 4) +
       ",\"features\":{";
  for (int i = 0; i < NFEAT; i++) s += String(i ? "," : "") + "\"" + FEATURE_NAMES[i] + "\":" + String(r.feat[i], 4);
  s += "}";
  const char* axes[3] = {"x", "y", "z"};
  for (int a = 0; a < 3; a++) {
    s += ",\"" + String(axes[a]) + "\":[";
    for (int i = 0; i < h.n; i++) {
      if (i) s += ',';
      s += xyz[i * 3 + a];
    }
    s += "]";
  }
  return s + "}";
}

// voiceId: which fixed line in tools/voice_lines.json fits this message (cap_<n> = quiet[n])
String commitMessage(int newCaptures, bool hasSnap, int labelled, String& voiceId) {
  static const char* quiet[] = {
      "Ze trap has been listening, and so on, and so on.",
      "Nothing happened, but it happened very precisely.",
      "More vibrations for ze big Other to interpret.",
      "Ze data does not lie. It only shows you what you wanted to see."};
  String m;
  const int q = esp_random() % 4;
  voiceId = hasSnap ? "snap_" + String(esp_random() % 3) : newCaptures ? "cap_" + String(q) : String("labels");
  if (hasSnap) m = "💥 Snap! " + String(newCaptures) + " new captures - one of them maybe ze Real";
  else if (newCaptures) m = "🐭 " + String(newCaptures) + " new captures from ze trap";
  else m = "🧠 Ze model changed its mind about some vibrations";
  m += "\n\n";
  m += newCaptures ? quiet[q] : "Labels changed. This is how ideology works: ze past is rewritten.";
  m += "\n\n" + String(dataset::count()) + " captures on ze device, " + labelled + " labelled.";
  m += "\nPushed by polh-kill v" POLH_VERSION " directly from ze ESP32-S3.";
  return m;
}

// ze philosopher comments on ze ESP's own pushes in Telegram - but not every 15 minutes, nobody
// can bear so much philosophy: ze first push after boot, every snap, otherwise at most every 6 h
void speakAfterPush(String voiceId, bool hasSnap) {
  static bool helloDone = false;
  static uint32_t lastVoice = 0;
  if (!settings.voiceMsgs) return;
  const esp_reset_reason_t rr = esp_reset_reason();
  const bool crashed = rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT;
  if (!helloDone && crashed) helloDone = true;  // no cheerful hello after a crash
  else if (!helloDone) voiceId = "hello";
  else if (!hasSnap && lastVoice && millis() - lastVoice < 6UL * 3600 * 1000) return;
  helloDone = true;
  lastVoice = millis();
  telegram::sendVoice(github::pagesUrl() + "voice/esp/" + voiceId + ".ogg",
                      "🎙 " + lastMsg + "\n<a href=\"" + github::pagesUrl() + "\">ze captures</a>");
}

bool push() {
  lastErr = "";
  // ---- 1. snapshot of the dataset (under the app lock, no network here)
  std::vector<SampleRec> all, todo;
  {
    app::Guard g;
    dataset::forEach([&](const SampleRec& r) {
      all.push_back(r);
      if (!(r.flags & FLAG_PUBLISHED) && (int)todo.size() < BATCH_MAX) todo.push_back(r);
    });
  }
  int unpublished = 0;
  bool hasSnap = false;
  for (auto& r : all) unpublished += !(r.flags & FLAG_PUBLISHED);

  // ---- 2. current head of the branch
  String resp;
  int code = call("GET", "/git/ref/heads/" + settings.ghBranch, "", &resp);
  if (code == 404) { lastErr = "branch " + settings.ghBranch + " not found"; return false; }
  if (code == 401 || code == 403) { lastErr = "token rejected (HTTP " + String(code) + ")"; return false; }
  if (code != 200) { lastErr = "ref HTTP " + String(code); return false; }
  const String head = shaOf(resp, "object");
  code = call("GET", "/git/commits/" + head, "", &resp);
  if (code != 200) { lastErr = "commit HTTP " + String(code); return false; }
  const String baseTree = shaOf(resp, "tree");

  // ---- 3. blobs: capture files
  std::vector<FileOut> files;
  std::vector<uint32_t> done;
  std::unique_ptr<int16_t[]> xyz(new (std::nothrow) int16_t[WIN_MAX_SAMPLES * 3]);
  if (!xyz) { lastErr = "out of memory"; return false; }
  uint32_t batchSamples = 0;
  for (auto& r : todo) {
    // long windows (up to 2.5 s) make big JSON files: max ~25 s of waveform per push
    if (batchSamples > 25000) break;
    RawHeader h;
    bool raw;
    {
      app::Guard g;
      raw = (r.flags & FLAG_RAW) && dataset::readRaw(r.id, h, xyz.get());
    }
    if (raw) {
      batchSamples += h.n;
      const String sha = blob(captureJson(r, h, xyz.get()));
      if (!sha.length()) return false;
      files.push_back({"data/captures/" + String(r.id) + ".json", sha});
    }
    if (r.predCls == C_SNAP && r.snapP >= settings.alertProb) hasSnap = true;
    done.push_back(r.id);
  }

  // ---- 4. blobs: index, csv, model, status
  int labelled = 0;
  {
    // Arduino's String grows to ze exact size on every +=, so without reserve() building these
    // files is quadratic: at ~400 captures it copied megabytes in PSRAM for seconds, ze idle task
    // on core 0 starved and ze task watchdog shot ze trap. Reserve, look up fast, and yield.
    std::vector<uint32_t> waveIds;
    for (auto& f : files) waveIds.push_back(f.path.substring(14).toInt());  // "data/captures/<id>.json"
    std::sort(waveIds.begin(), waveIds.end());
    String idx;
    idx.reserve(all.size() * 96 + 1024);
    idx += "{\"generated\":" + String((uint32_t)time(nullptr)) + ",\"device\":\"" + settings.deviceName +
           "\",\"version\":\"" POLH_VERSION "\",\"classes\":[";
    for (int c = 0; c < classCount(); c++)
      idx += String(c ? "," : "") + "{\"name\":\"" + className(c) + "\",\"emoji\":\"" + classEmoji(c) +
             "\",\"builtin\":" + (c < builtinClassCount() ? "true" : "false") + "}";
    idx += "],\"columns\":[\"id\",\"ts\",\"label\",\"pred\",\"snap_p\",\"peak_g\",\"waveform\"],\"captures\":[";
    String csv;
    csv.reserve(all.size() * (40 + NFEAT * 11) + 256);
    csv += "id,ts,label,pred,snap_p,rate_hz";
    for (int i = 0; i < NFEAT; i++) csv += String(",") + FEATURE_NAMES[i];
    csv += "\n";
    bool first = true;
    uint32_t n = 0;
    for (auto& r : all) {
      if ((++n & 31) == 0) vTaskDelay(1);  // let ze idle task breathe
      const bool isLab = r.label < classCount();
      labelled += isLab;
      // a capture has a published waveform if it was pushed before or is in this batch
      bool wave = (r.flags & FLAG_PUBLISHED) && (r.flags & FLAG_RAW);
      if (std::binary_search(waveIds.begin(), waveIds.end(), r.id)) wave = true;
      idx += String(first ? "" : ",") + "[" + r.id + "," + r.ts + ",\"" + (isLab ? className(r.label) : "") +
             "\",\"" + className(r.predCls) + "\"," + String(r.snapP, 3) + "," +
             String(powf(10, r.feat[F_PEAK_LOG]), 3) + "," + (wave ? 1 : 0) + "]";
      first = false;
      csv += String(r.id) + "," + r.ts + "," + (isLab ? className(r.label) : "") + "," + className(r.predCls) + "," +
             String(r.snapP, 3) + "," + r.rateHz;
      for (int i = 0; i < NFEAT; i++) csv += "," + String(r.feat[i], 5);
      csv += "\n";
    }
    idx += "]}";
    String sha = blob(idx);
    if (!sha.length()) return false;
    files.push_back({"data/index.json", sha});
    sha = blob(csv);
    if (!sha.length()) return false;
    files.push_back({"data/samples.csv", sha});
  }
  {
    JsonDocument d;
    app::modelToJson(d.to<JsonObject>());
    String s;
    serializeJson(d, s);
    const String sha = blob(s);
    if (!sha.length()) return false;
    files.push_back({"data/model.json", sha});
  }
  {
    const Stats st = app::stats();
    JsonDocument d;
    d["device"] = settings.deviceName;
    d["version"] = POLH_VERSION;
    d["generated"] = (uint32_t)time(nullptr);
    d["uptime_ms"] = millis();
    d["sensor"] = app::sensorOk() ? app::accel()->name() : "missing";
    d["noise_floor_mg"] = detector.baselineG() * 1000;
    d["threshold_mg"] = detector.thresholdG() * 1000;
    d["events_since_boot"] = st.events;
    d["snaps_total"] = st.snapsTotal;
    d["captures"] = dataset::count();
    d["labelled"] = dataset::labelled();
    d["waveforms_on_device"] = dataset::rawCount();
    d["armed"] = settings.armed;
    String s;
    serializeJson(d, s);
    const String sha = blob(s);
    if (!sha.length()) return false;
    files.push_back({"data/status.json", sha});
  }

  // ---- 5. tree, commit, move the branch
  String tree = "{\"base_tree\":\"" + baseTree + "\",\"tree\":[";
  for (size_t i = 0; i < files.size(); i++)
    tree += String(i ? "," : "") + "{\"path\":\"" + files[i].path + "\",\"mode\":\"100644\",\"type\":\"blob\",\"sha\":\"" +
            files[i].sha + "\"}";
  tree += "]}";
  code = call("POST", "/git/trees", tree, &resp);
  if (code != 201) { lastErr = "tree HTTP " + String(code); return false; }
  const String treeSha = shaOf(resp);

  JsonDocument cd;
  String voiceId;
  cd["message"] = commitMessage(done.size(), hasSnap, labelled, voiceId);
  cd["tree"] = treeSha;
  cd["parents"].add(head);
  String cbody;
  serializeJson(cd, cbody);
  code = call("POST", "/git/commits", cbody, &resp);
  if (code != 201) { lastErr = "commit HTTP " + String(code); return false; }
  const String commitSha = shaOf(resp);

  code = call("PATCH", "/git/refs/heads/" + settings.ghBranch, "{\"sha\":\"" + commitSha + "\",\"force\":false}", &resp);
  if (code == 422) { lastErr = "branch moved meanwhile, retrying"; return false; }
  if (code != 200) { lastErr = "ref update HTTP " + String(code); return false; }

  // ---- 6. remember what is online now
  for (uint32_t id : done) {
    {
      app::Guard g;
      dataset::setFlags(id, FLAG_PUBLISHED, 0);
    }
    vTaskDelay(1);
  }
  pushedTotal += done.size();
  backlog = unpublished - (int)done.size();
  lastMsg = cd["message"].as<String>();
  lastMsg = lastMsg.substring(0, lastMsg.indexOf('\n'));
  Serial.printf("[GitHub 🐙] %s (%s)\n", lastMsg.c_str(), commitSha.substring(0, 7).c_str());
  statusLed::flashPushed();
  speakAfterPush(voiceId, hasSnap);
  return true;
}

void task(void*) {
  vTaskDelay(pdMS_TO_TICKS(BOOT_DELAY_MS));
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!configured() || !net::online() || net::otaInProgress()) continue;
    const uint32_t now = millis();
    const uint32_t every = (uint32_t)max(1, (int)settings.ghEveryMin) * 60000UL;
    const bool due = manual || (pending && (snapSoon || !lastOk || now - lastOk >= every));
    if (!due || (lastTry && !manual && now - lastTry < (lastErr.length() ? RETRY_MS : 30000))) continue;
    lastTry = now;
    manual = snapSoon = false;
    const bool hadPending = pending;
    pending = false;
    net::tlsLock();
    const bool ok = push();
    net::tlsUnlock();
    if (ok) {
      lastOk = millis();
      lastOkTs = time(nullptr);
      if (backlog > 0) { pending = true; snapSoon = true; }  // more to send: next batch soon
    } else {
      pending = true;
      (void)hadPending;
      Serial.printf("[GitHub ❌] %s\n", lastErr.c_str());
    }
  }
}

}  // namespace

// core 1, idle priority: TLS waits for GitHub poll ze socket, and on core 0 (WiFi, lwip, web server)
// that starved ze idle task until ze watchdog fired. Here ze sampler and ze loop simply preempt us.
void begin() { xTaskCreatePinnedToCore(task, "github", 16384, nullptr, tskIDLE_PRIORITY, nullptr, 1); }
bool configured() { return settings.ghToken.length() > 20 && settings.ghRepo.indexOf('/') > 0; }
void notifyCapture(bool snap) {
  pending = true;
  if (snap) snapSoon = true;
  if (backlog >= 0) backlog++;
}
void notifyChange() { pending = true; }
void pushNow() { manual = true; pending = true; }

String pagesUrl() {
  const int sl = settings.ghRepo.indexOf('/');
  if (sl <= 0) return "";
  String owner = settings.ghRepo.substring(0, sl);
  owner.toLowerCase();
  return "https://" + owner + ".github.io/" + settings.ghRepo.substring(sl + 1) + "/";
}

void statusToJson(JsonObject o) {
  o["configured"] = configured();
  o["repo"] = settings.ghRepo;
  o["branch"] = settings.ghBranch;
  o["every_min"] = settings.ghEveryMin;
  o["pages_url"] = pagesUrl();
  o["last_ok_ts"] = (uint32_t)lastOkTs;
  o["last_ok_ago_ms"] = lastOk ? millis() - lastOk : 0;
  o["last_message"] = lastMsg;
  o["last_error"] = lastErr;
  o["pushed_since_boot"] = pushedTotal;
  o["backlog"] = backlog;
  o["pending"] = (bool)pending;
}

}  // namespace github
