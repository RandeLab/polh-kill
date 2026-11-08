// Publishes captures to GitHub Pages: one commit per batch on the gh-pages
// branch through the Git Data API (blobs -> tree -> commit -> ref).
//   data/index.json            all captures (id, time, label, prediction, peak)
//   data/samples.csv           features of all captures
//   data/model.json            class stats + leave-one-out result
//   data/status.json           device summary (no IPs, no SSIDs)
//   data/captures/<id>.json    waveform + features of each capture
// Needs a fine-grained token with "Contents: read and write" on the repo.
#pragma once
#include <ArduinoJson.h>

namespace github {
void begin();
bool configured();
void notifyCapture(bool snap);  // new capture stored (snap = push soon)
void notifyChange();            // labels / classes changed
void pushNow();
void statusToJson(JsonObject o);
String pagesUrl();              // https://<owner>.github.io/<repo>/ (from ze repo setting)
}  // namespace github
