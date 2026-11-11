// Feature extraction: turns one event window into NFEAT numbers.
#pragma once
#include "config.h"
#include "detector.h"

enum Feature {
  F_PEAK_LOG = 0,  // log10 peak dynamic acceleration [g]
  F_RMS_LOG,       // log10 RMS after the trigger [g]
  F_JERK_LOG,      // log10 peak jerk [g/s]
  F_DURATION_MS,   // time above threshold
  F_ZCR_HZ,        // zero crossings/s on the dominant axis (first 200 ms)
  F_BAND_LOW,      // spectral energy fraction 0-30 Hz
  F_BAND_MID,      // 30-100 Hz
  F_BAND_HIGH,     // 100-250 Hz
  F_BAND_TOP,      // 250 Hz - Nyquist
  F_AXIS_DOM,      // energy share of the strongest axis (1 = one axis only)
  F_SETTLE_MS,     // time from the peak until the envelope falls below 10 %
  F_PEAKS,         // number of separate peaks above 50 % of the maximum
  F_GYRO_LOG,      // log10 peak rotational angular velocity [deg/s]
  F_DELTA_TILT,    // orientation angle change between trigger and settle [deg]
  F_AXIS_Z,        // energy share of the Z axis (perpendicular impact into base)
};

extern const char* const FEATURE_NAMES[NFEAT];
// fixed centre/scale per feature, used to normalise for the linear model
extern const float FEATURE_CENTER[NFEAT];
extern const float FEATURE_SCALE[NFEAT];

void computeFeatures(const EventWindow& w, float out[NFEAT]);
