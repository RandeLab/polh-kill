# How polh-kill learns: Edge ML & Data Architecture

> *"Ze trap does not know what a snap is. It must be taught. Like all of us, by ze big Other."*

---

## 1. Detecting that *something* happened

The MPU-6500 is sampled at a continuous **1,000 Hz** rate across 3 axes of acceleration and 3 axes of gyroscope telemetry (`src/detector.cpp` & `src/sensor_mpu6500.cpp`):

1. **Gravity separation:** Gravity $\vec{g}$ per axis is tracked with a slow exponential moving average ($\tau = 2\text{ s}$) and subtracted. What remains is the dynamic acceleration $\vec{a}_{\text{dyn}}$.
2. **Noise floor tracking:** The baseline vibration mean and variance ($\sigma_{\text{floor}}$) are tracked recursively ($\tau = 5\text{ s}$) during quiet periods. The floor adapts automatically whether placed in a cellar, a barn, or next to a washing machine.
3. **Trigger confirmation:** Trigger condition:
   $$\text{Threshold} = \max(\text{min\_g}, \, \mu_{\text{floor}} + k \cdot \sigma_{\text{floor}})$$
   *(Defaults: $k = 8.0$, $\text{min\_g} = 0.20\text{ g}$)*.
   To reject typing or table nudges, **4 consecutive samples** ($4\text{ ms}$) above threshold within a $15\text{ ms}$ window are required before committing an event.
4. **Windowing:** A window of **$200\text{ ms}$ pre-trigger** and at least **$800\text{ ms}$ post-trigger** is captured from a circular buffer. If motion continues (e.g. setting the spring bar), the window dynamically extends up to **$2.5\text{ s}$**, ending $400\text{ ms}$ after motion ceases, followed by a $300\text{ ms}$ refractory dead time.

---

## 2. 15-Dimensional Multi-Sensor Features (`src/features.cpp`)

Every raw vibration wave ($1,000\text{–}2,500$ samples $\times$ 6 axes) is reduced to a 15-parameter feature vector $\vec{f} \in \mathbb{R}^{15}$:

| Feature | Idea & Physical Meaning |
|---|---|
| `peak_log` | $\log_{10}$ of peak dynamic acceleration (snap: $> 5\text{–}20\text{ g}$, footstep: $\approx 0.1\text{–}0.3\text{ g}$). |
| `rms_log` | Total vibration energy density after the trigger. |
| `jerk_log` | Derivative $\frac{da}{dt}$ — spring snap release is acoustically violent ($> 10^4\text{ g/s}$). |
| `duration_ms` | Milliseconds the vibration stays above noise threshold. |
| `zcr_hz` | Zero-crossing rate on dominant axis (ringing frequency). |
| `band_0_30` | Sub-bass energy fraction ($0\text{–}30\text{ Hz}$, heavy floor footsteps). |
| `band_30_100` | Low-mid energy fraction ($30\text{–}100\text{ Hz}$, table bumps). |
| `band_100_250`| Structural resonance fraction ($100\text{–}250\text{ Hz}$, spring vibration). |
| `band_250_top`| Acoustic snap shock fraction ($250\text{–}500\text{ Hz}$). |
| `axis_dom` | Spatial directivity ratio ($\max(E_x,E_y,E_z) / E_{\text{total}}$). |
| `settle_ms` | Ring-down decay time to $10\%$ peak amplitude. |
| `peaks` | Count of distinct shock echoes / bar bounces. |
| `gyro_log` | $\log_{10}$ of peak angular rotational velocity ($\omega$ in $^\circ/\text{s}$). |
| `delta_tilt` | Total angular orientation change ($\Delta\theta$) before vs. after trigger. |
| `axis_z` | Direct perpendicular vertical impact fraction into the wooden base. |

---

## 3. Online Recursive Learning (`src/classifier.cpp`)

Every labelled sample updates the model **immediately in $\approx 1\text{ }\mu\text{s}$**, with zero batch retraining:

- **Gaussian Naive Bayes:** Running sum ($S = \sum f$) and sum of squares ($SS = \sum f^2$) per feature and class. From these two values, the exact mean ($\mu$) and standard deviation ($\sigma$) are calculated on the fly. A variance floor prevents overfitting when samples are sparse.
- **Recursive Least Squares (RLS):** Linear target model for *snap vs. not-snap* with forgetting factor $\lambda = 0.998$:
  $$k = \frac{P x}{\lambda + x^T P x}, \qquad w \leftarrow w + k (y - w^T x), \qquad P \leftarrow \frac{P - k x^T P}{\lambda}$$
- **Heuristic fallback:** If fewer than two classes have data, peak acceleration ($> 1.0\text{ g}$) acts as the trigger.
- **Ensemble Fusion:** When both `snap` and background have $\ge 3$ samples, $\text{snap\_p}$ blends Bayes ($50\%$) and RLS ($50\%$).
- **Alert Dispatch:**
  * $\text{snap\_p} \ge 0.70$ $\rightarrow$ Immediate Telegram alert (`@Ze_trap_bot`).
  * $0.35 \le \text{snap\_p} < 0.70$ $\rightarrow$ Telegram asks *"what was it?"* with inline buttons to learn from your response.

---

## 4. 4-Tier Data Persistence

1. **Hardware Flash (`/dataset` partition):**
   * Stored in dedicated 6 MB LittleFS flash memory.
   * `/ds/index2.bin`: Binary records of all 15 features and labels across all events.
   * `/ds/w*.bin`: Raw 1,000 Hz 3-axis waveforms (budget of $2.56\text{ MB}$).
   * Survives power loss and firmware updates.
2. **Interactive Web Interface:**
   * **`📚 Dataset` tab** (`/#data`): View all events, waveform graphs (`📈`), and download `samples.csv`.
   * **`🧪 Learning lab` tab** (`/#lab`): Confusion matrix, class distributions, and Leave-One-Out validation.
3. **Local Computer Backup:**
   * Stored in workspace at [`dataset/samples.csv`](../dataset/samples.csv).
4. **Cloud Archive (GitHub Pages):**
   * The ESP32 background task pushes batches to branch [`origin/gh-pages`](https://github.com/RandeLab/polh-kill/tree/gh-pages) over WiFi.
   * Contains `data/samples.csv`, individual raw waveforms under `data/captures/*.json`, and `data/model.json`.
   * Live dashboard: **[randelab.github.io/polh-kill](https://randelab.github.io/polh-kill/)**.

---

## 5. How to train a resilient model

1. **Ambient & negative classes:**
   * `bump`: Knock the table/floor from different distances.
   * `mouse`: Light scratches and nibbles on the bait plate with a pencil or brush.
   * `footstep`: Walk past the setup at normal pacing.
   * `trap-armed`: Set the spring bar, place the arming rod (records the angular tilt shift).
   * `other`: Door slams, speech, chair movement.
2. **The positive class (`snap`):**
   * Arm the trap.
   * Use a stick or pencil to trigger the bait pedal.
   * Label the event as **`💥 snap`**.
   * Record $5\text{–}10$ snaps with empty trap and baited trap.
3. **Verify:** Check the Leave-One-Out confusion matrix on the `🧪 Learning lab` tab. When snaps achieve $100\%$ precision/recall, your edge detector is ready for unattended deployment!
