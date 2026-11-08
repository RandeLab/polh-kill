#include "dataset.h"
#include <deque>
#include <vector>

// separate partition, so web-UI updates (uploadfs / OTA) never erase the data
fs::LittleFSFS dataFS;

namespace dataset {
namespace {
constexpr const char* INDEX = "/ds/index2.bin";
constexpr const char* INDEX_V1 = "/ds/index.bin";
constexpr uint32_t RAW_MAGIC = 0x31574B50;  // "PKW1"
constexpr float FS_FULL = 0.92f;            // stop writing raw windows above this
uint32_t nextId = 1;
size_t live = 0, nLabelled = 0;
std::deque<uint32_t> rawIds;  // ids with raw windows on flash, oldest first
size_t indexBytes = 0;        // size of ze index file

bool isLabel(uint8_t l) { return l != LABEL_NONE && l != LABEL_DELETED; }

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
  const SampleRec before = rec;
  fn(rec);
  if (isLabel(before.label) && !isLabel(rec.label)) nLabelled--;
  if (!isLabel(before.label) && isLabel(rec.label)) nLabelled++;
  f.seek(pos);
  f.write((const uint8_t*)&rec, sizeof(rec));
  f.close();
  return true;
}

// v1 records (id, ts, label, flags, rate, feat) -> v2 (labelled, prediction unknown)
void migrateV1() {
  struct __attribute__((packed)) V1 {
    uint32_t id, ts;
    uint8_t label, flags;
    uint16_t rateHz;
    float feat[NFEAT];
  };
  if (!dataFS.exists(INDEX_V1) || dataFS.exists(INDEX)) return;
  File in = dataFS.open(INDEX_V1, "r"), out = dataFS.open(INDEX, "w");
  if (!in || !out) return;
  V1 v;
  int n = 0;
  while (in.read((uint8_t*)&v, sizeof(v)) == sizeof(v)) {
    SampleRec r{};
    r.id = v.id; r.ts = v.ts; r.label = v.label; r.flags = v.flags & FLAG_RAW; r.rateHz = v.rateHz;
    r.predCls = v.label == LABEL_DELETED ? 0 : v.label; r.snapP = -1;
    memcpy(r.feat, v.feat, sizeof(r.feat));
    out.write((const uint8_t*)&r, sizeof(r));
    n++;
  }
  in.close();
  out.close();
  dataFS.rename(INDEX_V1, "/ds/index_v1.bak");
  Serial.printf("[Dataset 📚] migrated %d v1 records\n", n);
}
}  // namespace

String rawPath(uint32_t id) { return String("/ds/w") + id + ".bin"; }

bool begin() {
  if (!dataFS.begin(true, "/dataset", 10, "dataset")) {
    Serial.println("[Dataset ❌] could not mount the dataset partition");
    return false;
  }
  if (!dataFS.exists("/ds")) dataFS.mkdir("/ds");
  migrateV1();
  live = nLabelled = 0;
  rawIds.clear();
  File f = dataFS.open(INDEX, "r");
  if (f) {
    SampleRec rec;
    while (f.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec)) {
      indexBytes += sizeof(rec);
      nextId = max(nextId, rec.id + 1);
      if (rec.label != LABEL_DELETED) live++;
      if (isLabel(rec.label)) nLabelled++;
      if (rec.flags & FLAG_RAW) rawIds.push_back(rec.id);
    }
    f.close();
  }
  Serial.printf("[Dataset 📚] %u captures (%u labelled), %u raw windows, %u/%u kB\n", (unsigned)live,
                (unsigned)nLabelled, (unsigned)rawIds.size(), (unsigned)(refreshUsage() / 1024),
                (unsigned)(fsTotal() / 1024));
  return true;
}

uint32_t add(const EventWindow& w, const float feat[NFEAT], int label, int predCls, int method, float snapP) {
  SampleRec rec{};
  rec.id = nextId++;
  rec.ts = (uint32_t)w.ts;
  rec.label = label < 0 ? LABEL_NONE : (uint8_t)label;
  rec.rateHz = w.rateHz;
  rec.predCls = (uint8_t)predCls;
  rec.method = (uint8_t)method;
  rec.snapP = snapP;
  memcpy(rec.feat, feat, sizeof(rec.feat));

  // keep flash usage bounded: drop the oldest raw windows (features stay). Max 2 per capture,
  // so lowering MAX_RAW_ON_FLASH never means one giant delete that blocks ze flash for seconds
  int dropped = 0;
  while (!rawIds.empty() && dropped++ < 2 && (rawIds.size() >= (size_t)MAX_RAW_ON_FLASH || fsUsed() > fsTotal() * FS_FULL)) {
    const uint32_t old = rawIds.front();
    rawIds.pop_front();
    dataFS.remove(rawPath(old));
    patchRecord(old, [](SampleRec& r) { r.flags &= ~FLAG_RAW; });
  }
  if (rawIds.size() < (size_t)MAX_RAW_ON_FLASH && fsUsed() < fsTotal() * FS_FULL) {
    File raw = dataFS.open(rawPath(rec.id), "w");
    if (raw) {
      RawHeader h{RAW_MAGIC, w.rateHz, w.n, w.trig, 0, w.thresholdG};
      raw.write((const uint8_t*)&h, sizeof(h));
      raw.write((const uint8_t*)w.xyz, (size_t)w.n * 3 * sizeof(int16_t));
      raw.close();
      rec.flags |= FLAG_RAW;
      rawIds.push_back(rec.id);
    }
  }

  File f = dataFS.open(INDEX, "a");
  if (!f) return 0;
  f.write((const uint8_t*)&rec, sizeof(rec));
  f.close();
  indexBytes += sizeof(rec);
  live++;
  if (isLabel(rec.label)) nLabelled++;
  return rec.id;
}

bool relabel(uint32_t id, int label) {
  return patchRecord(id, [&](SampleRec& r) {
    r.label = label < 0 ? LABEL_NONE : (uint8_t)label;
    r.flags &= ~FLAG_PUBLISHED;  // label changed -> publish again
  });
}

bool setFlags(uint32_t id, uint8_t set, uint8_t clear) {
  return patchRecord(id, [&](SampleRec& r) { r.flags = (r.flags | set) & ~clear; });
}

bool remove(uint32_t id) {
  bool ok = patchRecord(id, [](SampleRec& r) { r.label = LABEL_DELETED; r.flags &= ~FLAG_RAW; });
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
  indexBytes = 0;
  live = nLabelled = 0;
}

void forEach(const std::function<void(const SampleRec&)>& fn) {
  File f = dataFS.open(INDEX, "r");
  if (!f) return;
  SampleRec rec;
  while (f.read((uint8_t*)&rec, sizeof(rec)) == sizeof(rec))
    if (rec.label != LABEL_DELETED) fn(rec);
  f.close();
}

bool get(uint32_t id, SampleRec& out) {
  File f = dataFS.open(INDEX, "r");
  if (!f) return false;
  size_t pos;
  bool ok = seekRecord(f, id, out, pos) && out.label != LABEL_DELETED;
  f.close();
  return ok;
}

bool readRaw(uint32_t id, RawHeader& h, int16_t* xyz) {
  File f = dataFS.open(rawPath(id), "r");
  if (!f) return false;
  bool ok = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == RAW_MAGIC && h.n <= WIN_MAX_SAMPLES &&
            f.read((uint8_t*)xyz, h.n * 3 * sizeof(int16_t)) == h.n * 3 * sizeof(int16_t);
  f.close();
  return ok;
}

size_t count() { return live; }
size_t labelled() { return nLabelled; }
size_t rawCount() { return rawIds.size(); }

bool trimOne() {
  if (rawIds.size() <= (size_t)MAX_RAW_ON_FLASH) return false;
  const uint32_t old = rawIds.front();
  rawIds.pop_front();
  dataFS.remove(rawPath(old));
  patchRecord(old, [](SampleRec& r) { r.flags &= ~FLAG_RAW; });
  return true;
}
// lfs_fs_size walks every block of ze filesystem, and flash reads stall BOTH cores: with ~400
// captures on a nearly full partition this took >5 s and ze task watchdog killed ze trap every
// 45 s. So we never ask LittleFS; we count ourselves: each raw window = 2 blocks, plus ze index.
size_t fsUsed() {
  const size_t blocks = rawIds.size() * 2 + (indexBytes + 4095) / 4096 + 16;  // 16: dirs, metadata, classes
  return blocks * 4096;
}
size_t refreshUsage() { return fsUsed(); }
size_t fsTotal() { return dataFS.totalBytes(); }

}  // namespace dataset
