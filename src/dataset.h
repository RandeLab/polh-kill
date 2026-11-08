// Every capture on LittleFS (own "dataset" partition, mounted at /dataset).
//   /ds/index2.bin  fixed-size records (features + prediction + label), append-only;
//                   label LABEL_NONE = not labelled yet, LABEL_DELETED = tombstone
//   /ds/w<id>.bin   raw window (header + int16 xyz), only the newest
//                   MAX_RAW_ON_FLASH are kept (~6 kB = 2 flash blocks each)
// The v1 file /ds/index.bin (labelled samples only) is migrated on first boot.
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <functional>
#include "config.h"
#include "detector.h"

extern fs::LittleFSFS dataFS;  // dataset partition, mounted at /dataset

constexpr uint8_t LABEL_NONE = 0xFE;
constexpr uint8_t LABEL_DELETED = 0xFF;
constexpr uint8_t FLAG_RAW = 0x01;        // raw window on flash
constexpr uint8_t FLAG_PUBLISHED = 0x02;  // capture file pushed to GitHub Pages

struct __attribute__((packed)) SampleRec {
  uint32_t id;
  uint32_t ts;       // unix time, 0 if unknown
  uint8_t label;     // class index, LABEL_NONE or LABEL_DELETED
  uint8_t flags;
  uint16_t rateHz;
  uint8_t predCls;   // what the model said at capture time
  uint8_t method;
  uint16_t reserved;
  float snapP;
  float feat[NFEAT];
};

struct __attribute__((packed)) RawHeader {
  uint32_t magic;   // 'PKW1'
  uint16_t rateHz, n, trig, reserved;
  float thresholdG;
};

namespace dataset {
bool begin();
// store a capture -> sample id (0 on failure)
uint32_t add(const EventWindow& w, const float feat[NFEAT], int label, int predCls, int method, float snapP);
bool relabel(uint32_t id, int label);
bool setFlags(uint32_t id, uint8_t set, uint8_t clear);
bool remove(uint32_t id);
void wipe();
// iterate over all captures that are not deleted (labelled and unlabelled)
void forEach(const std::function<void(const SampleRec&)>& fn);
bool get(uint32_t id, SampleRec& out);
bool readRaw(uint32_t id, RawHeader& h, int16_t* xyz /* WIN_MAX_SAMPLES*3 */);
String rawPath(uint32_t id);
size_t count();      // captures (not deleted)
size_t labelled();   // captures with a label
size_t rawCount();
size_t fsUsed();
size_t fsTotal();
}  // namespace dataset
