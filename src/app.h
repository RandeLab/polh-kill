// Application core: sampler task, event pipeline, learning lab, statistics.
// Everything that web handlers and the Telegram task touch is guarded by one
// recursive mutex (app::Guard).
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "classifier.h"
#include "config.h"
#include "detector.h"
#include "sensor.h"

struct RecentEvent {
  EventWindow* win = nullptr;
  float feat[NFEAT] = {};
  Prediction pred;
  bool candidate = false;  // recorded while a learning test was armed
  int8_t testLabel = -1;   // label chosen before the test
  int8_t storedLabel = -1; // label it was saved with (-1 = not in dataset)
  uint32_t sampleId = 0;
  bool alerted = false;    // Telegram snap alert sent
};

struct Stats {
  uint32_t events = 0;      // since boot
  uint32_t skipped = 0;     // since boot: not stored because of ze event-storm budget
  uint32_t snapsTotal = 0;  // persisted
  uint32_t snapsBoot = 0;
  uint32_t lastEventMs = 0;
  uint32_t lastSnapMs = 0;
  time_t lastSnapTs = 0;
};

namespace app {
void begin();
void loop();
void applySettings();

void lock();
void unlock();
struct Guard {
  Guard() { lock(); }
  ~Guard() { unlock(); }
};

Accel* accel();
bool sensorOk();
Classifier& model();
Stats stats();

// recent events (newest last)
int recentCount();
const RecentEvent* recentAt(int i);
const RecentEvent* findEvent(uint32_t eventId);

// learning lab
void armTest(int label);
void cancelTest();
int testLabel();                 // -1 = not armed
uint32_t testRemainingMs();
uint32_t labelEvent(uint32_t eventId, int label);  // -> sample id, 0 on failure
bool discardEvent(uint32_t eventId);

// dataset maintenance (rebuilds the model)
bool relabelSample(uint32_t sampleId, int label);
bool deleteSample(uint32_t sampleId);
void wipeDataset();
bool deleteClass(int idx, String& err);
void rebuildModel();
LooResult leaveOneOut();

// JSON helpers shared by web + telegram
void eventToJson(const RecentEvent& e, JsonObject o);
void statusToJson(JsonObject o);
void modelToJson(JsonObject o);
String uptimeText(uint32_t ms);
}  // namespace app
