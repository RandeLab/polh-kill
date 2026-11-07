#include "dataset.h"
#include <LittleFS.h>
#include <deque>
#include <vector>

// separate partition, so web-UI updates (uploadfs / OTA) never erase the data
fs::LittleFSFS dataFS;

namespace dataset {
namespace {
constexpr const char* INDEX = "/ds/index.bin";
constexpr uint32_t RAW_MAGIC = 0x31574B50;  // "PKW1"
uint32_t nextId = 1;
size_t live = 0;
std::deque<uint32_t> rawIds;  // ids with raw windows on flash, oldest first

bool seekRecord(File& f, uint32_t id, SampleRec& rec, size_t& pos) {
  f.seek(0);
  pos = 0;
  while (f.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec)) {
    if (rec.id == id) return true;
    pos += sizeof(rec);
  }
  return false;
}

bool patchRecord(uint32_t id, const std::function<void(SampleRec&)>& fn) {
  File f = dataFS.open(INDEX, "r+");
  if (!f) return false;
  SampleRec rec;
  size_t pos;
  if (!seekRecord(f, id, rec, pos)) { f.close(); return false; }
  fn(rec);
  f.seek(pos);
  f.write((const uint8_t*)&rec, sizeof(rec));
  f.close();
  return true;
}
}  // namespace

String rawPath(uint32_t id) { return String("/ds/w") + id + ".bin"; }

bool begin() {
  if (!dataFS.begin(true, "/dataset", 10, "dataset")) {
    Serial.println("[Dataset ❌] could not mount the dataset partition");
    return false;
  }
  if (!dataFS.exists("/ds")) dataFS.mkdir("/ds");
  live = 0;
  rawIds.clear();
  File f = dataFS.open(INDEX, "r");
  if (f) {
    SampleRec rec;
    while (f.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec)) {
      nextId = max(nextId, rec.id + 1);
      if (rec.label != 0xFF) live++;
      if (rec.flags & 1) rawIds.push_back(rec.id);
    }
    f.close();
  }
  Serial.printf("[Dataset 📚] %u labelled samples, %u raw windows\n", (unsigned)live, (unsigned)rawIds.size());
  return true;
}

uint32_t add(const EventWindow& w, const float feat[NFEAT], int label) {
  SampleRec rec{};
  rec.id = nextId++;
  rec.ts = (uint32_t)w.ts;
  rec.label = (uint8_t)label;
  rec.rateHz = w.rateHz;
  memcpy(rec.feat, feat, sizeof(rec.feat));

  File raw = dataFS.open(rawPath(rec.id), "w");
  if (raw) {
    RawHeader h{RAW_MAGIC, w.rateHz, w.n, w.trig, 0, w.thresholdG};
    raw.write((const uint8_t*)&h, sizeof(h));
    raw.write((const uint8_t*)w.xyz, (size_t)w.n * 3 * sizeof(int16_t));
    raw.close();
    rec.flags |= 1;
    rawIds.push_back(rec.id);
  }
  // keep flash usage bounded: drop the oldest raw windows (features stay)
  while (rawIds.size() > (size_t)MAX_RAW_ON_FLASH) {
    const uint32_t old = rawIds.front();
    rawIds.pop_front();
    dataFS.remove(rawPath(old));
    patchRecord(old, [](SampleRec& r) { r.flags &= ~1; });
  }

  File f = dataFS.open(INDEX, "a");
  if (!f) return 0;
  f.write((const uint8_t*)&rec, sizeof(rec));
  f.close();
  live++;
  return rec.id;
}

bool relabel(uint32_t id, int label) {
  return patchRecord(id, [&](SampleRec& r) { r.label = (uint8_t)label; });
}

bool remove(uint32_t id) {
  bool ok = patchRecord(id, [](SampleRec& r) { r.label = 0xFF; r.flags &= ~1; });
  if (ok) {
    dataFS.remove(rawPath(id));
    for (auto it = rawIds.begin(); it != rawIds.end(); ++it)
      if (*it == id) { rawIds.erase(it); break; }
    if (live) live--;
  }
  return ok;
}

void wipe() {
  File dir = dataFS.open("/ds");
  if (dir) {
    std::vector<String> names;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) names.push_back(String("/ds/") + e.name());
    dir.close();
    for (auto& n : names) dataFS.remove(n);
  }
  rawIds.clear();
  live = 0;
}

void forEach(const std::function<void(const SampleRec&)>& fn) {
  File f = dataFS.open(INDEX, "r");
  if (!f) return;
  SampleRec rec;
  while (f.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec))
    if (rec.label != 0xFF) fn(rec);
  f.close();
}

bool get(uint32_t id, SampleRec& out) {
  File f = dataFS.open(INDEX, "r");
  if (!f) return false;
  size_t pos;
  bool ok = seekRecord(f, id, out, pos) && out.label != 0xFF;
  f.close();
  return ok;
}

size_t count() { return live; }
size_t fsUsed() { return dataFS.usedBytes(); }
size_t fsTotal() { return dataFS.totalBytes(); }

}  // namespace dataset
