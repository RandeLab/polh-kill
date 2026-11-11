#include "app.h"
#include <Preferences.h>
#include <freertos/semphr.h>
#include <vector>
#include "dataset.h"
#include "github.h"
#include "features.h"
#include "net.h"
#include "settings.h"
#include "status_led.h"
#include "telegram.h"
#include "web.h"

namespace app {
namespace {

SemaphoreHandle_t mtx = nullptr;
Accel* sensor = nullptr;
volatile bool sensorReady = false;
Classifier clf;
Stats st;
RecentEvent recent[RECENT_EVENTS];
int recentN = 0;  // number of valid entries, newest at recentN-1
int armedLabel = -1;
uint32_t armedAt = 0;

void samplerTask(void*) {
  Sample buf[64];
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    if (!sensorReady) {
      sensorReady = sensor->begin();
      if (sensorReady) {
        detector.begin(sensor->rateHz());
        Serial.printf("[Sensor ✅] %s @ %u Hz\n", sensor->name(), sensor->rateHz());
        wake = xTaskGetTickCount();
      } else {
        Serial.println("[Sensor ❌] accelerometer not found, retrying in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }
    const size_t n = sensor->read(buf, 64);
    for (size_t i = 0; i < n; i++) detector.push(buf[i]);
    if (n < 64) {
      const TickType_t now = xTaskGetTickCount();
      if ((TickType_t)(now - wake) > pdMS_TO_TICKS(2)) wake = now;
      vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
    }
  }
}

void pushRecent(const RecentEvent& e) {
  if (recentN == RECENT_EVENTS) {
    freeWindow(recent[0].win);
    for (int i = 1; i < RECENT_EVENTS; i++) recent[i - 1] = recent[i];
    recentN--;
  }
  recent[recentN++] = e;
}

RecentEvent* findMutable(uint32_t id) {
  for (int i = 0; i < recentN; i++)
    if (recent[i].win && recent[i].win->id == id) return &recent[i];
  return nullptr;
}

void persistSnapCount() {
  Preferences p;
  p.begin("polhstat", false);
  p.putUInt("snaps", st.snapsTotal);
  p.end();
}

}  // namespace

void lock() { xSemaphoreTakeRecursive(mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(mtx); }

void begin() {
  mtx = xSemaphoreCreateRecursiveMutex();
  {
    Preferences p;
    p.begin("polhstat", true);
    st.snapsTotal = p.getUInt("snaps", 0);
    p.end();
  }
  dataset::begin();
  classesBegin();
  rebuildModel();
  applySettings();
  sensor = createAccel();
  // core 1, above the Arduino loop: sampling must never starve
  xTaskCreatePinnedToCore(samplerTask, "sampler", 4096, nullptr, 5, nullptr, 1);
}

void applySettings() {
  detector.kSigma = settings.kSigma;
  detector.minG = settings.minG;
  Guard g;
  clf.snapPeakG = settings.snapPeakG;
}

void loop() {
  // shrink ze waveform store slowly in ze background: one file every 3 s, never a big flash storm
  static uint32_t lastTrim = 0;
  if (millis() - lastTrim > 3000) {
    lastTrim = millis();
    Guard g;
    dataset::trimOne();
  }
  if (armedLabel >= 0 && millis() - armedAt > TEST_ARM_TIMEOUT_MS) {
    Guard g;
    armedLabel = -1;
    web::notifyLab();
  }

  EventWindow* w = nullptr;
  if (!detector.eventQueue || xQueueReceive(detector.eventQueue, &w, 0) != pdTRUE) return;

  RecentEvent e;
  e.win = w;
  computeFeatures(*w, e.feat);
  {
    Guard g;
    e.pred = clf.predict(e.feat);
    if (armedLabel >= 0) {
      e.candidate = true;
      e.testLabel = armedLabel;
      armedLabel = -1;
    }
    st.events++;
    st.lastEventMs = millis();
    const bool snap = !e.candidate && e.pred.snapProb >= settings.alertProb;
    if (snap) {
      st.snapsTotal++;
      st.snapsBoot++;
      st.lastSnapMs = millis();
      st.lastSnapTs = w->ts;
      persistSnapCount();
    }
    // captures are kept unlabelled until somebody says what it was; lab candidates only when the user
    // keeps them. But an event storm (cable handling, a printer next door: one event every 2 s) wrote
    // flash non-stop and made ze trap lag and die. So ordinary vibrations have a budget of 20 per
    // 10 min (token bucket); possible snaps are always stored, and every event is still counted.
    static float tokens = 20;
    static uint32_t lastRefill = millis();
    tokens = min(20.0f, tokens + (millis() - lastRefill) / 30000.0f);
    lastRefill = millis();
    const bool important = e.pred.snapProb >= settings.unsureProb;
    bool stored = false;
    if (!e.candidate && (important || tokens >= 1)) {
      if (!important) tokens -= 1;
      e.sampleId = dataset::add(*w, e.feat, -1, e.pred.cls, e.pred.method, e.pred.snapProb);
      stored = true;
    } else if (!e.candidate) {
      st.skipped++;
    }
    pushRecent(e);
    if (stored) github::notifyCapture(snap);
  }
  Serial.printf("[Event %s] #%u peak %.2f g -> %s (snap %.0f %%, %s)%s\n",
                classEmoji(e.pred.cls), (unsigned)w->id, powf(10, e.feat[F_PEAK_LOG]),
                className(e.pred.cls), e.pred.snapProb * 100, METHOD_NAMES[e.pred.method],
                e.candidate ? " [lab]" : "");
  statusLed::flashEvent(e.pred.cls, !e.candidate && e.pred.snapProb >= settings.alertProb);
  web::notifyEvent(e);
  if (!e.candidate) telegram::onEvent(e);
}

Accel* accel() { return sensor; }
bool sensorOk() { return sensorReady; }
Classifier& model() { return clf; }

Stats stats() {
  Guard g;
  return st;
}

int recentCount() { return recentN; }
const RecentEvent* recentAt(int i) { return (i >= 0 && i < recentN) ? &recent[i] : nullptr; }
const RecentEvent* findEvent(uint32_t id) { return findMutable(id); }

void armTest(int label) {
  Guard g;
  armedLabel = constrain(label, 0, classCount() - 1);
  armedAt = millis();
}

void cancelTest() {
  Guard g;
  armedLabel = -1;
}

int testLabel() { return armedLabel; }

uint32_t testRemainingMs() {
  if (armedLabel < 0) return 0;
  const uint32_t el = millis() - armedAt;
  return el >= TEST_ARM_TIMEOUT_MS ? 0 : TEST_ARM_TIMEOUT_MS - el;
}

uint32_t labelEvent(uint32_t eventId, int label) {
  if (label < 0 || label >= classCount()) return 0;
  Guard g;
  RecentEvent* e = findMutable(eventId);
  if (!e) return 0;
  if (e->sampleId) {  // already stored: relabel instead
    if (e->storedLabel == label) return e->sampleId;
    relabelSample(e->sampleId, label);
    e->storedLabel = label;
    return e->sampleId;
  }
  const uint32_t id = dataset::add(*e->win, e->feat, label, e->pred.cls, e->pred.method, e->pred.snapProb);
  if (!id) return 0;
  e->sampleId = id;
  e->storedLabel = label;
  clf.add(e->feat, label);  // the recursive update
  statusLed::flashLearned();
  Serial.printf("[Learn 🧠] event #%u stored as sample %u = %s (%u samples)\n", (unsigned)eventId,
                (unsigned)id, className(label), (unsigned)clf.total());
  return id;
}

bool discardEvent(uint32_t eventId) {
  Guard g;
  RecentEvent* e = findMutable(eventId);
  if (!e) return false;
  e->candidate = false;
  return true;
}

bool relabelSample(uint32_t sampleId, int label) {
  Guard g;
  if (!dataset::relabel(sampleId, label)) return false;
  for (int i = 0; i < recentN; i++)
    if (recent[i].sampleId == sampleId) recent[i].storedLabel = label;
  rebuildModel();
  return true;
}

bool deleteSample(uint32_t sampleId) {
  Guard g;
  if (!dataset::remove(sampleId)) return false;
  for (int i = 0; i < recentN; i++)
    if (recent[i].sampleId == sampleId) { recent[i].sampleId = 0; recent[i].storedLabel = -1; }
  rebuildModel();
  return true;
}

// Delete a custom class. Only allowed while it has no samples; samples of
// classes above it move down one index so their labels stay correct.
bool deleteClass(int idx, String& err) {
  Guard g;
  if (idx < builtinClassCount() || idx >= classCount()) { err = "built-in classes cannot be deleted"; return false; }
  if (clf.count(idx) > 0) { err = "class still has samples - relabel or delete them first"; return false; }
  std::vector<SampleRec> move;
  dataset::forEach([&](const SampleRec& r) { if (r.label > idx && r.label < LABEL_NONE) move.push_back(r); });
  for (auto& r : move) dataset::relabel(r.id, r.label - 1);
  for (int i = 0; i < recentN; i++) {
    if (recent[i].storedLabel > idx) recent[i].storedLabel--;
    if (recent[i].testLabel > idx) recent[i].testLabel--;
    if (recent[i].pred.cls >= idx) recent[i].pred.cls = 0;
  }
  if (armedLabel >= idx) armedLabel = -1;
  if (!classRemove(idx, err)) return false;
  rebuildModel();
  return true;
}

void wipeDataset() {
  Guard g;
  dataset::wipe();
  for (int i = 0; i < recentN; i++) { recent[i].sampleId = 0; recent[i].storedLabel = -1; }
  st.snapsTotal = 0;  // fresh dataset, fresh count
  persistSnapCount();
  rebuildModel();
}

// The model is fully defined by the dataset: replaying it in order gives the
// exact same state as the incremental updates did.
void rebuildModel() {
  Guard g;
  clf.reset();
  clf.snapPeakG = settings.snapPeakG;
  dataset::forEach([](const SampleRec& r) { if (r.label < classCount()) clf.add(r.feat, r.label); });
  github::notifyChange();
}

LooResult leaveOneOut() {
  Guard g;
  LooResult res;
  std::vector<SampleRec> all;
  dataset::forEach([&](const SampleRec& r) { if (r.label < classCount()) all.push_back(r); });
  for (auto& r : all) {
    clf.remove(r.feat, r.label);
    const Prediction p = clf.predict(r.feat);
    clf.add(r.feat, r.label, false);
    res.confusion[r.label][p.cls]++;
    res.total++;
    if (p.cls == r.label) res.correct++;
    const bool isSnap = r.label == C_SNAP, saidSnap = p.cls == C_SNAP;
    if (isSnap && saidSnap) res.snapTp++;
    if (!isSnap && saidSnap) res.snapFp++;
    if (isSnap && !saidSnap) res.snapFn++;
  }
  return res;
}

void eventToJson(const RecentEvent& e, JsonObject o) {
  o["id"] = e.win->id;
  o["ts"] = (uint32_t)e.win->ts;
  o["ago_ms"] = millis() - e.win->uptimeMs;
  o["peak_g"] = powf(10, e.feat[F_PEAK_LOG]);
  o["cls"] = className(e.pred.cls);
  o["snap_p"] = e.pred.snapProb;
  o["method"] = METHOD_NAMES[e.pred.method];
  o["candidate"] = e.candidate;
  if (e.testLabel >= 0) o["test_label"] = className(e.testLabel);
  if (e.storedLabel >= 0) o["stored_label"] = className(e.storedLabel);
  o["sample_id"] = e.sampleId;
  JsonObject probs = o["probs"].to<JsonObject>();
  for (int c = 0; c < classCount(); c++) probs[className(c)] = e.pred.probs[c];
  JsonObject f = o["features"].to<JsonObject>();
  for (int i = 0; i < NFEAT; i++) f[FEATURE_NAMES[i]] = e.feat[i];
}

void statusToJson(JsonObject o) {
  Guard g;
  o["name"] = settings.deviceName;
  o["version"] = POLH_VERSION;
  o["uptime_ms"] = millis();
  o["time"] = (uint32_t)time(nullptr);
  o["heap"] = ESP.getFreeHeap();
  o["psram"] = ESP.getFreePsram();
  JsonObject n = o["net"].to<JsonObject>();
  n["mode"] = net::modeName();
  n["ip"] = net::ip();
  n["ssid"] = settings.wifiSsid;
  n["rssi"] = net::rssi();
  n["ap_clients"] = net::apClients();
  JsonObject s = o["sensor"].to<JsonObject>();
  s["ok"] = (bool)sensorReady;
  s["name"] = sensor ? sensor->name() : "-";
  s["rate"] = detector.rateHz();
  s["samples"] = detector.samples();
  s["baseline_g"] = detector.baselineG();
  s["sigma_g"] = detector.sigmaG();
  s["threshold_g"] = detector.thresholdG();
  JsonArray gv = s["gravity"].to<JsonArray>();
  gv.add(detector.gx());
  gv.add(detector.gy());
  gv.add(detector.gz());
  if (sensor) {
    const float tC = sensor->tempC();
    if (!isnan(tC)) s["temp_c"] = tC;
    float w[3] = {0, 0, 0};
    sensor->gyro(w);
    JsonArray gy = s["gyro"].to<JsonArray>();
    gy.add(w[0]);
    gy.add(w[1]);
    gy.add(w[2]);
    float ori[3] = {0, 0, 0};
    sensor->orientation(ori);
    JsonArray oa = s["orientation"].to<JsonArray>();
    oa.add(ori[0]);
    oa.add(ori[1]);
    oa.add(ori[2]);
  }
#ifdef POLH_SIM_SENSOR
  s["simulated"] = true;
#endif
  JsonObject a = o["stats"].to<JsonObject>();
  a["events"] = st.events;
  a["skipped"] = st.skipped;
  a["snaps_total"] = st.snapsTotal;
  a["snaps_boot"] = st.snapsBoot;
  a["last_event_ago_ms"] = st.lastEventMs ? millis() - st.lastEventMs : 0;
  a["last_snap_ago_ms"] = st.lastSnapMs ? millis() - st.lastSnapMs : 0;
  a["last_snap_ts"] = (uint32_t)st.lastSnapTs;
  JsonObject l = o["lab"].to<JsonObject>();
  l["armed_label"] = armedLabel >= 0 ? className(armedLabel) : nullptr;
  l["remaining_ms"] = testRemainingMs();
  l["samples"] = clf.total();
  JsonObject t = o["telegram"].to<JsonObject>();
  t["configured"] = telegram::configured();
  telegram::usersToJson(t);
  t["bot"] = telegram::botName();
  t["last_error"] = telegram::lastError();
  o["armed"] = settings.armed;
  o["admin_protected"] = settings.adminPass.length() > 0;
  JsonObject fs = o["fs"].to<JsonObject>();
  fs["used"] = dataset::fsUsed();
  fs["total"] = dataset::fsTotal();
  fs["captures"] = dataset::count();
  fs["labelled"] = dataset::labelled();
  fs["raw"] = dataset::rawCount();
  fs["raw_kb"] = dataset::rawBytes() / 1024;
  fs["raw_budget_kb"] = RAW_BUDGET_BLOCKS * 4;
  github::statusToJson(o["github"].to<JsonObject>());
}

void modelToJson(JsonObject o) {
  Guard g;
  o["total"] = clf.total();
  JsonArray cls = o["classes"].to<JsonArray>();
  for (int c = 0; c < classCount(); c++) {
    JsonObject k = cls.add<JsonObject>();
    k["name"] = className(c);
    k["emoji"] = classEmoji(c);
    k["n"] = clf.count(c);
    JsonArray m = k["mean"].to<JsonArray>();
    JsonArray sd = k["sd"].to<JsonArray>();
    for (int i = 0; i < NFEAT; i++) {
      m.add(clf.mean(c, i));
      sd.add(clf.stdev(c, i));
    }
  }
  JsonArray fn = o["features"].to<JsonArray>();
  for (int i = 0; i < NFEAT; i++) fn.add(FEATURE_NAMES[i]);
  const LooResult r = leaveOneOut();
  JsonObject loo = o["loo"].to<JsonObject>();
  loo["total"] = r.total;
  loo["correct"] = r.correct;
  loo["snap_tp"] = r.snapTp;
  loo["snap_fp"] = r.snapFp;
  loo["snap_fn"] = r.snapFn;
  JsonArray cm = loo["confusion"].to<JsonArray>();
  for (int i = 0; i < classCount(); i++) {
    JsonArray row = cm.add<JsonArray>();
    for (int j = 0; j < classCount(); j++) row.add(r.confusion[i][j]);
  }
}

String uptimeText(uint32_t ms) {
  const uint32_t s = ms / 1000;
  const uint32_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
  char buf[32];
  if (d) snprintf(buf, sizeof(buf), "%ud %uh", (unsigned)d, (unsigned)h);
  else if (h) snprintf(buf, sizeof(buf), "%uh %umin", (unsigned)h, (unsigned)m);
  else snprintf(buf, sizeof(buf), "%umin %us", (unsigned)m, (unsigned)(s % 60));
  return buf;
}

}  // namespace app
