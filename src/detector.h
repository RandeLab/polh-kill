// Event detector: tracks gravity and the noise floor recursively, triggers on
// impulses and cuts a pre/post window of raw dynamic acceleration.
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"
#include "sensor.h"

struct EventWindow {
  uint32_t id;
  uint32_t uptimeMs;
  time_t ts;            // 0 if NTP not synced yet
  uint16_t rateHz;
  uint16_t n;           // samples in xyz
  uint16_t trig;        // index of the trigger sample
  float thresholdG;     // trigger threshold at that moment
  int16_t xyz[WIN_MAX_SAMPLES][3];  // gravity removed, milli-g
};

struct LivePoint {
  float mag;           // max dynamic g in 20 ms bucket
  int16_t dx, dy, dz;  // signed peak dynamic accel per axis in bucket (mg)
};

EventWindow* allocWindow();
void freeWindow(EventWindow* w);

class Detector {
 public:
  void begin(uint16_t rateHz);
  void push(const Sample& s);   // called from the sampler task only

  // tunables (written by settings, read by the sampler task)
  volatile float kSigma = 6.0f;   // trigger at noise mean + k * sigma ...
  volatile float minG = 0.05f;    // ... but never below this

  // read-only telemetry
  float baselineG() const { return mean_; }
  float sigmaG() const { return sqrtf(var_); }
  float thresholdG() const { return thr_; }
  float gx() const { return g_[0]; }
  float gy() const { return g_[1]; }
  float gz() const { return g_[2]; }
  uint16_t rateHz() const { return rate_; }
  uint32_t samples() const { return count_; }
  uint32_t events() const { return nextId_ - 1; }
  bool capturing() const { return state_ == CAPTURE; }

  QueueHandle_t eventQueue = nullptr;  // EventWindow*
  QueueHandle_t liveQueue = nullptr;   // LivePoint: max dynamic g + signed xyz peak per live bucket

 private:
  enum State { WARMUP, IDLE, CAPTURE, REFRACTORY };
  void emitWindow();

  uint16_t rate_ = 1000;
  State state_ = WARMUP;
  uint32_t count_ = 0, stateLeft_ = 0;
  float g_[3] = {0, 0, 1};
  float alphaG_ = 0, alphaN_ = 0;
  float mean_ = 0.005f, var_ = 0.0001f, thr_ = 0.05f;
  int16_t (*ring_)[3] = nullptr;  // RING_SAMPLES x 3, allocated in PSRAM
  uint32_t head_ = 0, trigHead_ = 0;
  uint16_t preN_ = 0, postN_ = 0;
  uint32_t bucketN_ = 0, bucketLeft_ = 0;
  float bucketMax_ = 0, bucketDx_ = 0, bucketDy_ = 0, bucketDz_ = 0;
  float echo_ = 0, echoDecay_ = 0, capPeak_ = 0;  // echo guard (see detector.cpp)
  uint16_t hits_ = 0, hitWin_ = 0;                // multi-sample trigger confirmation
  float prev_[3] = {0, 0, 1};
  uint32_t captured_ = 0, quiet_ = 0, postMaxN_ = 0, hangN_ = 0, curPostN_ = 0;  // adaptive window
  uint32_t nextId_ = 1;
};

extern Detector detector;
