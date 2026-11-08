# How polh-kill learns

> "Ze trap does not know what a snap is. It must be taught. Like all of us, by ze big Other."

## 1. Detecting that *something* happened

The MPU is read at 1 kHz from its FIFO. For every sample (`src/detector.cpp`):

1. **Gravity** per axis is tracked with a slow exponential moving average (τ = 2 s) and subtracted.
   What remains is the *dynamic* acceleration `|a_dyn|`.
2. The **noise floor** (mean and variance of `|a_dyn|`) is tracked recursively (τ = 5 s), but only
   while nothing is happening. The floor adapts to the place: a quiet cellar, a washing machine…
3. **Trigger** when `|a_dyn| > max(min_g, mean + k·σ)` (defaults `k = 6`, `min_g = 0.05 g`).
4. A **window** of 200 ms before the trigger and at least 800 ms after it is cut from a ring buffer. While ze signal keeps moving the window grows (re-arming ze trap takes seconds); it ends 400 ms after things went quiet and is **never longer than 2.5 s** in total.
   (1000 samples × xyz, milli-g), then 300 ms dead time.

## 2. Features (`src/features.cpp`)

| feature | idea |
|---|---|
| `peak_log` | log10 of the peak, a snap is a few g, a footstep a few hundred mg |
| `rms_log` | energy after the trigger |
| `jerk_log` | how *sudden* it is — the spring bar is brutal |
| `duration_ms` | time above the threshold |
| `zcr_hz` | zero crossings on the dominant axis ≈ ringing frequency |
| `band_*` | spectral energy fractions 0-30 / 30-100 / 100-250 / 250-500 Hz (256-pt FFT) |
| `axis_dom` | is it one direction (bar hits the base) or everywhere |
| `settle_ms` | ring-down time |
| `peaks` | number of separate hits (bar bounces) |

## 3. Recursive learning (`src/classifier.cpp`)

Every labelled sample updates the model **immediately**, no batch retraining:

- **Gaussian naive Bayes** over the 6 built-in classes (snap, bump, footstep, mouse, other,
  armed-trap) plus your own ones: running sums
  per class and feature. Adding a sample is O(features). Priors are Laplace-smoothed, variances
  have a floor so 2-3 samples do not make the model overconfident.
- **Recursive least squares (RLS)** linear model for *snap vs. not-snap*, with forgetting factor
  λ = 0.998, so old samples fade slowly when the trap moves:

  ```
  k = P x / (λ + xᵀ P x)
  w = w + k (y - wᵀ x)
  P = (P - k xᵀ P) / λ
  ```

- **Heuristic** (peak > `snap_g` and short) as long as fewer than two classes have data.
- Final `snap_p` = average of Bayes P(snap) and the RLS score (through a sigmoid) once both
  snap and non-snap have ≥ 3 samples.

Decisions: `snap_p ≥ alert_p` (0.70) → Telegram alert. `unsure_p ≤ snap_p < alert_p` → Telegram
asks "what was it?" with buttons, and your answer becomes a new training sample.

## 4. Data

- Labelled samples live on their own flash partition (`dataset`, ~6 MB), so updating the web UI
  or firmware never erases them. Features of every sample are kept; raw windows for ze newest ones within a budget of ~2.6 MB (640 flash blocks; a 1 s window takes 2, a 2.5 s window 4).
- The model is fully defined by the dataset: on boot (and after relabel/delete) it is rebuilt by
  replaying all samples — same result as the incremental updates.
- **Leave-one-out** test in the learning lab: every sample is classified by a model that has not
  seen it. Confusion matrix, accuracy, snap recall/precision.
- `/api/export.csv` exports all features for offline work (pandas, a notebook, and so on).

## 5. How to record good data

1. Mount the sensor where it will stay (see [wiring.md](wiring.md)).
2. Learning lab → choose **snap** → *arm test* → let the trap snap (with a pencil, not a finger).
   Use **series mode** to do 10-20 in a row. Vary: empty trap, with bait, different places.
3. Then **bump** (knock the table/floor), **footstep** (walk past), **mouse** (scratch, nibble
   the bait with a brush), **other** (door slams, washing machine), **armed-trap** 🪤 (re-arming
   ze trap after a snap: pull ze spring, set ze bar, place it - a long but very typical pattern).
4. Look at the confusion matrix. If snaps are confused with bumps: more of both, from closer.
