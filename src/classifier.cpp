#include "classifier.h"
#include <Arduino.h>
#include <math.h>
#include "features.h"

const char* const METHOD_NAMES[3] = {"heuristic", "bayes", "bayes+rls"};

namespace {
constexpr float RLS_DELTA = 100.0f;   // initial P = delta * I (weak prior)
constexpr float RLS_LAMBDA = 0.998f;  // forgetting factor: old samples fade slowly
constexpr int MIN_PER_CLASS = 3;
}  // namespace

void Classifier::reset() {
  memset(n_, 0, sizeof(n_));
  memset(sum_, 0, sizeof(sum_));
  memset(sumsq_, 0, sizeof(sumsq_));
  memset(w_, 0, sizeof(w_));
  memset(P_, 0, sizeof(P_));
  for (int i = 0; i < D; i++) P_[i][i] = RLS_DELTA;
  rlsN_ = 0;
}

int Classifier::total() const {
  int t = 0;
  for (int c = 0; c < classCount(); c++) t += n_[c];
  return t;
}

void Classifier::normalise(const float f[NFEAT], float x[NFEAT + 1]) {
  for (int i = 0; i < NFEAT; i++) x[i] = (f[i] - FEATURE_CENTER[i]) / FEATURE_SCALE[i];
  x[NFEAT] = 1.0f;  // bias
}

void Classifier::add(const float f[NFEAT], int label, bool updateRls) {
  if (label < 0 || label >= classCount()) return;
  n_[label]++;
  for (int i = 0; i < NFEAT; i++) {
    sum_[label][i] += f[i];
    sumsq_[label][i] += (double)f[i] * f[i];
  }
  if (updateRls) rlsUpdate(f, label == C_SNAP ? 1.0f : -1.0f);
}

void Classifier::remove(const float f[NFEAT], int label) {
  if (label < 0 || label >= classCount() || n_[label] == 0) return;
  n_[label]--;
  for (int i = 0; i < NFEAT; i++) {
    sum_[label][i] -= f[i];
    sumsq_[label][i] -= (double)f[i] * f[i];
  }
}

float Classifier::mean(int c, int i) const { return n_[c] ? sum_[c][i] / n_[c] : 0; }

float Classifier::stdev(int c, int i) const {
  if (n_[c] < 2) return 0;
  const double m = sum_[c][i] / n_[c];
  const double v = (sumsq_[c][i] - n_[c] * m * m) / (n_[c] - 1);
  return v > 0 ? sqrt(v) : 0;
}

// Recursive least squares:  k = P x / (lambda + x' P x);  w += k (y - w'x);
//                           P = (P - k x' P) / lambda
void Classifier::rlsUpdate(const float f[NFEAT], float target) {
  float x[D], Px[D];
  normalise(f, x);
  float denom = RLS_LAMBDA;
  for (int i = 0; i < D; i++) {
    Px[i] = 0;
    for (int j = 0; j < D; j++) Px[i] += P_[i][j] * x[j];
    denom += x[i] * Px[i];
  }
  float err = target;
  for (int i = 0; i < D; i++) err -= w_[i] * x[i];
  for (int i = 0; i < D; i++) w_[i] += Px[i] / denom * err;
  // P is symmetric, so x'P = (Px)'
  for (int i = 0; i < D; i++)
    for (int j = 0; j < D; j++) P_[i][j] = (P_[i][j] - Px[i] * Px[j] / denom) / RLS_LAMBDA;
  rlsN_++;
}

float Classifier::rlsOutput(const float f[NFEAT]) const {
  float x[D], s = 0;
  normalise(f, x);
  for (int i = 0; i < D; i++) s += w_[i] * x[i];
  return s;
}

Prediction Classifier::predict(const float f[NFEAT]) const {
  Prediction p;
  int classesWithData = 0, snapN = n_[C_SNAP], otherN = 0;
  for (int c = 0; c < classCount(); c++) {
    if (n_[c] >= 1) classesWithData++;
    if (c != C_SNAP) otherN += n_[c];
  }

  if (classesWithData < 2) {
    // not enough to compare anything: trust the peak acceleration
    const float peak = powf(10.0f, f[F_PEAK_LOG]);
    const bool shortEvent = f[F_DURATION_MS] < 500;
    p.snapProb = (peak >= snapPeakG && shortEvent) ? 0.8f : min(0.6f, 0.5f * peak / snapPeakG);
    p.cls = p.snapProb >= 0.5f ? C_SNAP : C_OTHER;
    p.probs[C_SNAP] = p.snapProb;
    p.probs[C_OTHER] = 1 - p.snapProb;
    p.method = M_HEURISTIC;
    return p;
  }

  // Gaussian naive Bayes with a variance floor and Laplace-smoothed priors
  const int N = total();
  double logp[MAX_CLASSES];
  double best = -1e300;
  for (int c = 0; c < classCount(); c++) {
    if (n_[c] == 0) { logp[c] = -1e300; continue; }
    double lp = log((n_[c] + 1.0) / (N + classCount()));
    for (int i = 0; i < NFEAT; i++) {
      const double floorSd = (n_[c] < MIN_PER_CLASS ? 0.3 : 0.05) * FEATURE_SCALE[i];
      const double sd = max((double)stdev(c, i), floorSd);
      const double z = (f[i] - mean(c, i)) / sd;
      lp += -0.5 * z * z - log(sd);
    }
    logp[c] = lp;
    if (lp > best) { best = lp; p.cls = c; }
  }
  double norm = 0;
  for (int c = 0; c < classCount(); c++) norm += (logp[c] > -1e299) ? exp(logp[c] - best) : 0;
  for (int c = 0; c < classCount(); c++)
    p.probs[c] = (logp[c] > -1e299) ? exp(logp[c] - best) / norm : 0;

  p.rlsScore = rlsOutput(f);
  if (snapN >= MIN_PER_CLASS && otherN >= MIN_PER_CLASS) {
    const float rlsProb = 1.0f / (1.0f + expf(-3.0f * p.rlsScore));
    p.snapProb = 0.5f * p.probs[C_SNAP] + 0.5f * rlsProb;
    p.method = M_BAYES_RLS;
  } else {
    p.snapProb = p.probs[C_SNAP];
    p.method = M_BAYES;
  }
  return p;
}
