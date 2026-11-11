#include "detector.h"
#include <esp_heap_caps.h>
#include <time.h>
#include "app.h"

Detector detector;

EventWindow* allocWindow() {
  void* p = nullptr;
  if (psramFound()) p = heap_caps_malloc(sizeof(EventWindow), MALLOC_CAP_SPIRAM);
  if (!p) p = malloc(sizeof(EventWindow));
  return static_cast<EventWindow*>(p);
}

void freeWindow(EventWindow* w) { free(w); }

void Detector::begin(uint16_t rateHz) {
  rate_ = rateHz;
  // exponential moving averages, alpha = 1 - exp(-dt / tau)
  alphaG_ = 1.0f - expf(-1.0f / (rate_ * 2.0f));   // gravity: tau 2 s
  alphaN_ = 1.0f - expf(-1.0f / (rate_ * 5.0f));   // noise floor: tau 5 s
  preN_ = (uint16_t)min<int>(rate_ * WIN_PRE_MS / 1000, WIN_MAX_SAMPLES / 4);
  postN_ = (uint16_t)min<int>(rate_ * WIN_POST_MS / 1000, WIN_MAX_SAMPLES - preN_);
  postMaxN_ = (uint32_t)min<int>(rate_ * WIN_MAX_MS / 1000, WIN_MAX_SAMPLES) - preN_;
  hangN_ = (uint32_t)rate_ * WIN_HANGOVER_MS / 1000;
  if (!ring_) {
    const size_t bytes = sizeof(int16_t) * 3 * RING_SAMPLES;
    void* p = psramFound() ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM) : nullptr;
    if (!p) p = malloc(bytes);
    memset(p, 0, bytes);
    ring_ = static_cast<int16_t(*)[3]>(p);
  }
  bucketN_ = max<uint32_t>(1, rate_ / LIVE_RATE_HZ);
  bucketLeft_ = bucketN_;
  stateLeft_ = (uint32_t)rate_ * WARMUP_MS / 1000;
  state_ = WARMUP;
  if (!eventQueue) eventQueue = xQueueCreate(4, sizeof(EventWindow*));
  if (!liveQueue) liveQueue = xQueueCreate(256, sizeof(LivePoint));
}

void Detector::push(const Sample& s) {
  count_++;
  if (state_ == WARMUP && count_ < rate_ / 4) {  // converge gravity fast at boot
    g_[0] = s.x; g_[1] = s.y; g_[2] = s.z;
    prev_[0] = s.x; prev_[1] = s.y; prev_[2] = s.z;
  }
  const float sx = s.x - prev_[0], sy = s.y - prev_[1], sz = s.z - prev_[2];
  const float step = sqrtf(sx * sx + sy * sy + sz * sz);
  prev_[0] = s.x; prev_[1] = s.y; prev_[2] = s.z;

  // track gravity while not capturing an impulse (or if ze sensor is resting still at a new tilt)
  if (state_ == WARMUP || (state_ != CAPTURE && step < 0.05f) || step < 0.015f) {
    g_[0] += (s.x - g_[0]) * alphaG_;
    g_[1] += (s.y - g_[1]) * alphaG_;
    g_[2] += (s.z - g_[2]) * alphaG_;
  }
  const float dx = s.x - g_[0], dy = s.y - g_[1], dz = s.z - g_[2];
  const float dyn = sqrtf(dx * dx + dy * dy + dz * dz);

  int16_t* r = ring_[head_ & (RING_SAMPLES - 1)];
  r[0] = (int16_t)constrain(dx * 1000.0f, -32767.0f, 32767.0f);
  r[1] = (int16_t)constrain(dy * 1000.0f, -32767.0f, 32767.0f);
  r[2] = (int16_t)constrain(dz * 1000.0f, -32767.0f, 32767.0f);
  head_++;

  // recursive noise floor: only learn while nothing is happening (never fold impulses into baseline)
  if (state_ == WARMUP || (state_ == IDLE && hits_ == 0 && dyn <= thr_)) {
    const float d = dyn - mean_;
    mean_ += d * alphaN_;
    var_ += (d * d - var_) * alphaN_;
  }
  // echo guard: after a big event ze trap rings out for seconds, and every window of ze ringing
  // used to become its own "event" (13 g, then 0.29, 0.25, 0.15 ... g). So ze threshold is raised
  // to half of ze last peak and decays back with tau 1.5 s. A new, stronger event still triggers.
  if (echoDecay_ == 0) echoDecay_ = expf(-1.0f / (rate_ * 1.5f));
  echo_ *= echoDecay_;
  const float noiseThr = max((float)minG, mean_ + kSigma * sqrtf(var_));
  thr_ = max(noiseThr, echo_);

  // live plot bucket: peak magnitude + signed peak per axis (mg)
  if (dyn > bucketMax_) bucketMax_ = dyn;
  if (fabsf(dx) > fabsf(bucketDx_)) bucketDx_ = dx;
  if (fabsf(dy) > fabsf(bucketDy_)) bucketDy_ = dy;
  if (fabsf(dz) > fabsf(bucketDz_)) bucketDz_ = dz;
  if (--bucketLeft_ == 0) {
    LivePoint lp{
        bucketMax_,
        (int16_t)constrain(bucketDx_ * 1000.0f, -32767.0f, 32767.0f),
        (int16_t)constrain(bucketDy_ * 1000.0f, -32767.0f, 32767.0f),
        (int16_t)constrain(bucketDz_ * 1000.0f, -32767.0f, 32767.0f),
    };
    xQueueSend(liveQueue, &lp, 0);
    bucketMax_ = bucketDx_ = bucketDy_ = bucketDz_ = 0;
    bucketLeft_ = bucketN_;
  }

  switch (state_) {
    case WARMUP:
      if (--stateLeft_ == 0) state_ = IDLE;
      break;
    case IDLE:
      // require >= 4 samples above threshold within 15 ms so a single 1 ms glitch or tap never triggers
      if (dyn > thr_) {
        if (hits_ == 0) {
          trigHead_ = head_ - 1;
          hitWin_ = max<uint16_t>(4, rate_ * 15 / 1000);
          capPeak_ = dyn;
          capGyroPeak_ = 0;
          if (app::accel()) app::accel()->orientation(trigOri_);
        } else if (dyn > capPeak_) {
          capPeak_ = dyn;
        }
        if (app::accel()) {
          float w[3] = {};
          app::accel()->gyro(w);
          const float gw = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
          if (gw > capGyroPeak_) capGyroPeak_ = gw;
        }
        if (++hits_ >= 4) {
          captured_ = head_ - trigHead_;
          quiet_ = hangN_;
          hits_ = 0;
          hitWin_ = 0;
          state_ = CAPTURE;
        }
      } else if (hitWin_ > 0 && --hitWin_ == 0) {
        hits_ = 0;
        capPeak_ = 0;
        capGyroPeak_ = 0;
      }
      break;
    case CAPTURE:
      // adaptive window: at least WIN_POST_MS, longer while ze signal keeps moving (re-arming ze
      // trap takes seconds), ends WIN_HANGOVER_MS after it went quiet, never longer than WIN_MAX_MS
      if (dyn > capPeak_) capPeak_ = dyn;
      if (app::accel()) {
        float w[3] = {};
        app::accel()->gyro(w);
        const float gw = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (gw > capGyroPeak_) capGyroPeak_ = gw;
      }
      captured_++;
      if (dyn > noiseThr) quiet_ = hangN_;
      else if (quiet_) quiet_--;
      if ((captured_ >= postN_ && quiet_ == 0) || captured_ >= postMaxN_) {
        curPostN_ = captured_;
        emitWindow();
        echo_ = max(echo_, 0.5f * capPeak_);
        capPeak_ = 0;
        stateLeft_ = (uint32_t)rate_ * REFRACTORY_MS / 1000;
        state_ = REFRACTORY;
      }
      break;
    case REFRACTORY:
      if (--stateLeft_ == 0) state_ = IDLE;
      break;
  }
}

void Detector::emitWindow() {
  EventWindow* w = allocWindow();
  if (!w) return;
  w->id = nextId_++;
  w->uptimeMs = millis();
  time_t now = time(nullptr);
  w->ts = (now > 1700000000) ? now : 0;
  w->rateHz = rate_;
  w->trig = preN_;
  w->n = preN_ + curPostN_;
  w->thresholdG = thr_;
  w->gyroPeakDps = capGyroPeak_;
  float endOri[3] = {};
  if (app::accel()) app::accel()->orientation(endOri);
  float dRoll = fabsf(endOri[0] - trigOri_[0]);
  if (dRoll > 180.0f) dRoll = 360.0f - dRoll;
  float dPitch = fabsf(endOri[1] - trigOri_[1]);
  if (dPitch > 180.0f) dPitch = 360.0f - dPitch;
  w->deltaTiltDeg = sqrtf(dRoll * dRoll + dPitch * dPitch);
  capGyroPeak_ = 0;
  const uint32_t start = trigHead_ - preN_;
  for (uint16_t i = 0; i < w->n; i++) {
    const int16_t* r = ring_[(start + i) & (RING_SAMPLES - 1)];
    w->xyz[i][0] = r[0];
    w->xyz[i][1] = r[1];
    w->xyz[i][2] = r[2];
  }
  if (xQueueSend(eventQueue, &w, 0) != pdTRUE) freeWindow(w);
}
