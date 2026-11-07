// Online ("recursive") learning: every labelled sample updates the model
// immediately, nothing is ever retrained in batch.
//  - Gaussian naive Bayes over all classes (running sums per class/feature)
//  - Recursive least squares linear model for "snap vs. not snap"
//  - Peak-g heuristic as long as there is not enough data
#pragma once
#include "config.h"

enum EventClass { C_SNAP = 0, C_BUMP, C_FOOTSTEP, C_MOUSE, C_OTHER };
extern const char* const CLASS_NAMES[NUM_CLASSES];
extern const char* const CLASS_EMOJI[NUM_CLASSES];

enum Method { M_HEURISTIC = 0, M_BAYES, M_BAYES_RLS };
extern const char* const METHOD_NAMES[3];

struct Prediction {
  int cls = C_OTHER;             // most likely class
  float probs[NUM_CLASSES] = {};  // naive-Bayes posterior (or heuristic)
  float snapProb = 0;            // combined probability that this was a snap
  float rlsScore = 0;            // raw RLS output (+1 snap, -1 not)
  Method method = M_HEURISTIC;
};

struct LooResult {
  int confusion[NUM_CLASSES][NUM_CLASSES] = {};  // [true][predicted]
  int total = 0, correct = 0;
  int snapTp = 0, snapFp = 0, snapFn = 0;
};

class Classifier {
 public:
  void reset();
  void add(const float f[NFEAT], int label, bool updateRls = true);  // recursive update
  void remove(const float f[NFEAT], int label);  // undo (naive Bayes only, RLS is rebuilt)
  Prediction predict(const float f[NFEAT]) const;
  int count(int cls) const { return n_[cls]; }
  int total() const;
  float mean(int cls, int feat) const;
  float stdev(int cls, int feat) const;

  float snapPeakG = 1.0f;  // heuristic threshold until enough data exists

 private:
  void rlsUpdate(const float f[NFEAT], float target);
  float rlsOutput(const float f[NFEAT]) const;
  static void normalise(const float f[NFEAT], float x[NFEAT + 1]);

  int n_[NUM_CLASSES] = {};
  double sum_[NUM_CLASSES][NFEAT] = {};
  double sumsq_[NUM_CLASSES][NFEAT] = {};
  static constexpr int D = NFEAT + 1;
  float w_[D] = {};
  float P_[D][D] = {};
  int rlsN_ = 0;
};
