// Labelled samples on LittleFS.
//   /ds/index.bin   fixed-size records (features + label), append-only,
//                   deletes are tombstones (label = 0xFF)
//   /ds/w<id>.bin   raw window (header + int16 xyz), only the newest
//                   MAX_RAW_ON_FLASH are kept
#pragma once
#include <Arduino.h>
#include <functional>
#include "config.h"
#include "detector.h"
#include <LittleFS.h>

extern fs::LittleFSFS dataFS;  // dataset partition, mounted at /dataset

struct __attribute__((packed)) SampleRec {
  uint32_t id;
  uint32_t ts;      // unix time, 0 if unknown
  uint8_t label;    // EventClass, 0xFF = deleted
  uint8_t flags;    // bit0: raw window stored
  uint16_t rateHz;
  float feat[NFEAT];
};

struct __attribute__((packed)) RawHeader {
  uint32_t magic;   // 'PKW1'
  uint16_t rateHz, n, trig, reserved;
  float thresholdG;
};

namespace dataset {
bool begin();
uint32_t add(const EventWindow& w, const float feat[NFEAT], int label);  // returns sample id
bool relabel(uint32_t id, int label);
bool remove(uint32_t id);
void wipe();
// iterate over all live samples (not deleted)
void forEach(const std::function<void(const SampleRec&)>& fn);
bool get(uint32_t id, SampleRec& out);
String rawPath(uint32_t id);
size_t count();
size_t fsUsed();
size_t fsTotal();
}  // namespace dataset
