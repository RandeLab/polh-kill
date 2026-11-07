// Simulated accelerometer: gravity + noise + synthetic events on demand.
// The waveforms are rough guesses of what each class looks like; the real
// data from the learning lab replaces them as soon as a sensor is wired.
#include "sensor.h"
#include "config.h"

#ifdef POLH_SIM_SENSOR

#include <math.h>

namespace {

float gauss() {  // Box-Muller, cheap enough at 1 kHz
  float u1 = (esp_random() + 1.0f) / 4294967297.0f;
  float u2 = (esp_random() + 1.0f) / 4294967297.0f;
  return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI * u2);
}

float frand(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }

struct Burst {
  bool active = false;
  int cls = 0;
  uint32_t t = 0;          // samples since start
  float amp, freq, tau;    // damped sine
  float ax, ay, az;        // direction (unit vector)
  float amp2, delay2;      // secondary hit (snap bar landing, second footstep)
};

class SimAccel : public Accel {
 public:
  bool begin() override {
    t0_ = micros();
    return true;
  }

  size_t read(Sample* out, size_t maxCount) override {
    uint64_t due = (uint64_t)(micros() - t0_) * kRate / 1000000ULL;
    size_t n = 0;
    while (produced_ < due && n < maxCount) {
      out[n++] = next();
      produced_++;
    }
    return n;
  }

  uint16_t rateHz() const override { return kRate; }
  const char* name() const override { return "simulator"; }

  bool inject(int cls) override {
    Burst b;
    b.active = true;
    b.cls = cls;
    float th = frand(0, 2 * PI), ph = frand(-0.5f, 0.5f);
    b.ax = cosf(th) * cosf(ph);
    b.ay = sinf(th) * cosf(ph);
    b.az = sinf(ph);
    switch (cls) {
      case 0:  // snap: hard, high-frequency ring-down plus the bar landing
        b.amp = frand(2.5f, 6.0f); b.freq = frand(140, 260); b.tau = frand(0.015f, 0.035f);
        b.amp2 = b.amp * frand(0.3f, 0.6f); b.delay2 = frand(0.04f, 0.09f);
        break;
      case 1:  // bump: someone knocks the box / table
        b.amp = frand(0.3f, 1.2f); b.freq = frand(25, 60); b.tau = frand(0.05f, 0.12f);
        b.amp2 = 0; b.delay2 = 0;
        break;
      case 2:  // footstep: low, slow, often twice
        b.amp = frand(0.06f, 0.25f); b.freq = frand(8, 20); b.tau = frand(0.08f, 0.15f);
        b.amp2 = b.amp * frand(0.6f, 1.0f); b.delay2 = frand(0.45f, 0.65f);
        break;
      case 3:  // mouse: small scratchy bursts
        b.amp = frand(0.05f, 0.15f); b.freq = frand(250, 400); b.tau = frand(0.15f, 0.3f);
        b.amp2 = 0; b.delay2 = 0;
        break;
      default:  // other: anything
        b.amp = frand(0.1f, 2.0f); b.freq = frand(10, 400); b.tau = frand(0.01f, 0.2f);
        b.amp2 = 0; b.delay2 = 0;
        break;
    }
    pendingBurst_ = b;   // handed over to the sampler task in next()
    pending_ = true;
    return true;
  }

 private:
  static constexpr uint16_t kRate = 1000;

  Sample next() {
    Sample s{gauss() * 0.004f, gauss() * 0.004f, 1.0f + gauss() * 0.004f};
    if (pending_) {
      burst_ = pendingBurst_;
      pending_ = false;
    }
    if (burst_.active) {
      float t = burst_.t / (float)kRate;
      float v = hit(t, burst_.amp);
      if (burst_.amp2 > 0 && t > burst_.delay2) v += hit(t - burst_.delay2, burst_.amp2);
      if (burst_.cls == 3) v *= (gauss() > 0.3f) ? 1.0f : 0.2f;  // scratchy
      s.x += v * burst_.ax;
      s.y += v * burst_.ay;
      s.z += v * burst_.az;
      if (++burst_.t > kRate * 2) burst_.active = false;
    }
    return s;
  }

  float hit(float t, float amp) const {
    return amp * expf(-t / burst_.tau) * sinf(2 * PI * burst_.freq * t) * (1.0f + 0.15f * gauss());
  }

  uint32_t t0_ = 0;
  uint64_t produced_ = 0;
  Burst burst_;
  Burst pendingBurst_;
  volatile bool pending_ = false;
};

}  // namespace

Accel* createAccel() { return new SimAccel(); }

#endif  // POLH_SIM_SENSOR
