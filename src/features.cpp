#include "features.h"
#include <math.h>

const char* const FEATURE_NAMES[NFEAT] = {
    "peak_log", "rms_log", "jerk_log", "duration_ms", "zcr_hz", "band_0_30",
    "band_30_100", "band_100_250", "band_250_top", "axis_dom", "settle_ms", "peaks",
    "gyro_log", "delta_tilt", "axis_z"};
const float FEATURE_CENTER[NFEAT] = {0.0f, -1.0f, 2.0f, 200, 100, 0.25f, 0.25f, 0.25f, 0.25f, 0.6f, 200, 5, 1.0f, 2.0f, 0.33f};
const float FEATURE_SCALE[NFEAT] = {1.0f, 1.0f, 1.0f, 300, 150, 0.25f, 0.25f, 0.25f, 0.25f, 0.2f, 300, 5, 1.0f, 10.0f, 0.3f};

namespace {

constexpr int FFT_N = 256;

// in-place iterative radix-2 FFT
void fft(float* re, float* im, int n) {
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      float t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -2.0f * PI / len;
    const float wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; k++) {
        const int a = i + k, b = i + k + len / 2;
        const float xr = re[b] * cr - im[b] * ci;
        const float xi = re[b] * ci + im[b] * cr;
        re[b] = re[a] - xr; im[b] = im[a] - xi;
        re[a] += xr;        im[a] += xi;
        const float t = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = t;
      }
    }
  }
}

}  // namespace

void computeFeatures(const EventWindow& w, float out[NFEAT]) {
  const int n = w.n, t0 = w.trig;
  const float rate = w.rateHz;
  const float thr = max(w.thresholdG, 0.001f);
  auto ax = [&](int i, int a) { return w.xyz[i][a] / 1000.0f; };
  auto mag = [&](int i) {
    const float x = ax(i, 0), y = ax(i, 1), z = ax(i, 2);
    return sqrtf(x * x + y * y + z * z);
  };

  // peak, rms, jerk, axis energy, duration
  float peak = 0, sumsq = 0, jerk = 0, e[3] = {0, 0, 0};
  int peakIdx = t0, lastAbove = t0;
  float prev = mag(max(t0 - 1, 0));
  for (int i = t0; i < n; i++) {
    const float m = mag(i);
    if (m > peak) { peak = m; peakIdx = i; }
    sumsq += m * m;
    jerk = max(jerk, fabsf(m - prev) * rate);
    prev = m;
    if (m > thr) lastAbove = i;
    for (int a = 0; a < 3; a++) e[a] += ax(i, a) * ax(i, a);
  }
  const int postN = max(1, n - t0);
  const float etot = e[0] + e[1] + e[2] + 1e-9f;
  const int dom = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);

  out[F_PEAK_LOG] = log10f(peak + 1e-3f);
  out[F_RMS_LOG] = log10f(sqrtf(sumsq / postN) + 1e-4f);
  out[F_JERK_LOG] = log10f(jerk + 1e-2f);
  out[F_DURATION_MS] = (lastAbove - t0) * 1000.0f / rate;
  out[F_AXIS_DOM] = max(e[0], max(e[1], e[2])) / etot;

  // zero-crossing rate on the dominant axis, small hysteresis
  {
    const int end = min(n, t0 + (int)(rate * 0.2f));
    const float hyst = 0.2f * thr;
    int crossings = 0, sign = 0;
    for (int i = t0; i < end; i++) {
      const float v = ax(i, dom);
      const int s = (v > hyst) ? 1 : (v < -hyst ? -1 : 0);
      if (s != 0 && sign != 0 && s != sign) crossings++;
      if (s != 0) sign = s;
    }
    out[F_ZCR_HZ] = crossings / 2.0f / max(0.001f, (end - t0) / rate);
  }

  // band energies: Hann-windowed 256-point FFT per axis, starting just before the trigger
  {
    static float re[FFT_N], im[FFT_N];  // only called from the main task
    float bands[4] = {0, 0, 0, 0};
    const int start = max(0, t0 - 16);
    for (int a = 0; a < 3; a++) {
      for (int i = 0; i < FFT_N; i++) {
        const int k = start + i;
        const float hann = 0.5f - 0.5f * cosf(2 * PI * i / (FFT_N - 1));
        re[i] = (k < n) ? ax(k, a) * hann : 0;
        im[i] = 0;
      }
      fft(re, im, FFT_N);
      for (int b = 1; b < FFT_N / 2; b++) {
        const float f = b * rate / FFT_N;
        const float p = re[b] * re[b] + im[b] * im[b];
        bands[f < 30 ? 0 : f < 100 ? 1 : f < 250 ? 2 : 3] += p;
      }
    }
    const float bt = bands[0] + bands[1] + bands[2] + bands[3] + 1e-12f;
    for (int b = 0; b < 4; b++) out[F_BAND_LOW + b] = bands[b] / bt;
  }

  // envelope (10 ms moving max) -> settle time and peak count
  {
    const int win = max(1, (int)(rate * 0.01f));
    int settle = n - 1, peaks = 0, lastPeak = -100000;
    bool settled = false;
    const int minGap = max(1, (int)(rate * 0.005f));
    for (int i = peakIdx; i < n && !settled; i++) {
      float env = 0;
      for (int k = i; k < min(n, i + win); k++) env = max(env, mag(k));
      if (env < 0.1f * peak) { settle = i; settled = true; }
    }
    for (int i = max(t0, 1); i < n - 1; i++) {
      const float m = mag(i);
      if (m > 0.5f * peak && m >= mag(i - 1) && m > mag(i + 1) && i - lastPeak >= minGap) {
        peaks++;
        lastPeak = i;
      }
    }
    out[F_SETTLE_MS] = (settle - peakIdx) * 1000.0f / rate;
    out[F_PEAKS] = peaks;
  }

  // multi-sensor fusion: rotational angular velocity, tilt delta, perpendicular base impact share
  out[F_GYRO_LOG] = log10f(max(w.gyroPeakDps, 0.05f));
  out[F_DELTA_TILT] = w.deltaTiltDeg;
  out[F_AXIS_Z] = e[2] / etot;
}
