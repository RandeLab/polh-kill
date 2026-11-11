// Accelerometer abstraction. One implementation per chip, chosen at compile time.
#pragma once
#include <Arduino.h>

struct Sample {
  float x, y, z;  // acceleration in g (gravity included)
};

class Accel {
 public:
  virtual ~Accel() = default;
  virtual bool begin() = 0;
  // Pull all samples that are ready (FIFO drain). Returns how many were written.
  virtual size_t read(Sample* out, size_t maxCount) = 0;
  virtual uint16_t rateHz() const = 0;
  virtual const char* name() const = 0;
  virtual float tempC() const { return NAN; }
  virtual void gyro(float out[3]) const { out[0] = out[1] = out[2] = 0; }
  virtual void orientation(float out[3]) const { out[0] = out[1] = out[2] = 0; }  // roll, pitch, yaw (deg)
  // For the later battery mode: configure the chip's activity interrupt so it
  // can wake the ESP from deep sleep. Default: not supported.
  virtual bool enableWakeOnMotion(float /*thresholdG*/) { return false; }
  // Only the simulator implements this (web UI "inject" buttons).
  virtual bool inject(int /*cls*/) { return false; }
};

Accel* createAccel();
