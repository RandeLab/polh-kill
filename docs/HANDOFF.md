# Handoff: polh-kill (`main`)

For Marc, and for whatever coding agent he uses. Written 2026-11-10. First job: **improve the live
view** of the web interface. Everything else here is context so nothing gets broken or redone.

## Where things are
| | |
|---|---|
| repo | https://github.com/RandeLab/polh-kill (`main`) |
| device | http://polh-kill.local (RandeLab WiFi; IP by DHCP, was .102/.103). Windows needs ~2.7 s for the first `.local` lookup |
| dashboard | https://randelab.github.io/polh-kill/ (the ESP pushes every capture to `gh-pages/data/`) |
| Telegram | @Ze_trap_bot (owner Aaron; others join with ze approval button or ze join code) |
| hardware | ESP32-S3 N16R8 DevKitC-1 + MPU-92.65 (answers as MPU-6500), I2C SDA 8, SCL 9, INT 7 → `docs/wiring.md` |
| crash info | http://polh-kill.local/api/crash – `software`/`power-on` good, `task-wdt`/`panic` = crashed (with backtrace) |

## What is implemented (state of `5153d96`)
- **Detection:** 1 kHz FIFO, ±16 g; trigger at noise floor + 6σ (min 0.05 g); **echo guard** (after an
  event ze threshold = half ze last peak, decays with τ 1.5 s – a snap is 1 event, not 8);
  **adaptive window**: 200 ms before, ≥ 0.8 s after, grows while ze signal moves, max **2.5 s**.
- **Learning:** 12 features, naive Bayes + RLS (snap vs. not), leave-one-out in ze UI → `docs/learning.md`.
  6 built-in classes: snap, bump, footstep, mouse, other, **🪤 armed-trap** (re-arming after a snap) +
  own classes (👆 finger). New class right in ze lab's "keep as" dialog; 77 emoji suggestions.
- **Dataset:** reset on 2026-11-08 (dataset 1 was mostly echoes, model confused snaps and taps).
  Every capture stored; ordinary vibrations max 20 per 10 min, possible snaps always. Waveforms on flash
  by **size budget ~2.6 MB** (640 blocks), older ones only on GitHub Pages.
- **Stability:** network tasks (GitHub, Telegram) on core 1 at idle priority; task watchdog 20 s;
  no LittleFS walks in request handlers; no Telegram boot message after a crash.
- **Telegram:** up to 10 people per bot, throttled alerts, label buttons, commits as voice messages
  (🎙 switch in ze settings), voice rating keyboard 🔥 👍 😐 👎 🤮.
- **Live view (ze part you will work on):** smooth since 2026-11-08 – see below.

## The live view – how it works today
**Data path**
1. `src/detector.cpp` – every sample: `dyn` = |a − gravity| in g. Ze max of each 20 ms bucket goes
   into `liveQueue` (`LIVE_RATE_HZ = 50` in `include/config.h`, queue 256 points ≈ 5 s).
2. `src/web.cpp` `liveTask()` – own FreeRTOS task (priority 2, core 1), every 100 ms: drains ze queue,
   one WebSocket message to all clients:
   `{"t":"live","v":[mg, mg, …],"thr":<threshold mg>,"base":<noise floor mg>}` (~5 values).
   Other WebSocket messages: `{"t":"event","e":{…}}` (new capture), `{"t":"lab",…}` (every 1 s).
3. `data/app.js` – `onMessage()` appends to `live.v` (last `LIVE_N = 500` points = 10 s);
   `drawLive()` is called on every animation frame (`liveLoop`) and scrolls by ze time since ze last
   batch, so it moves smoothly between packets. Log scale 1 mg … a few g, dashed lines for
   threshold (red) and noise floor (grey). Colours come from CSS variables, cached in `css()`;
   call `cssReset()` when ze theme changes.
4. Markup: `data/index.html` `#tab-live` (stat tiles, `<canvas id="cvLive">`, legend, simulator panel,
   recent events). Styles: `data/style.css` (themes "Pinkograph" default and "Randen" via 🎨).

**Measured** (RandeLab WiFi): `/api/status` 0.03 s. Live stream 2026-11-08: 193 of 200 packets,
gap p99 261 ms, max 287 ms; **2026-11-10: 121 of 121 packets in 12 s, median 100 ms, p99 115 ms,
max 157 ms** (reference). Measure again after changes:
```bash
node tools/live_latency.js polh-kill.local 20     # Node >= 22
```

**Constraints**
- Ze UI lives in a 2 MB LittleFS partition on ze ESP and must work **offline** (hotspot mode):
  no CDN, no build step, no frameworks – plain HTML/CSS/JS. Keep files small.
- English UI with a German switch: text in `data-i18n="key"` attributes + ze `DE` table at ze top
  of `app.js`. New strings need both.
- Ze ESP is a small web server: avoid more polling. `/api/status` is polled every 3 s and after each
  event. Prefer pushing over ze WebSocket. More live data (e.g. x/y/z instead of |a|) is possible but
  costs bandwidth: 50 Hz × 3 axes is fine, 1 kHz is not.
- Never call `LittleFS.usedBytes()`/`totalBytes()` in a web handler (walks ze whole flash → seconds).

**Ideas Aaron and I had (none started)**
- time axis in seconds, pause/freeze button, zoom (1 s / 10 s / 60 s)
- mark captured events on ze timeline (with their emoji), click → open ze waveform
- peak hold / max of ze last minute, current value as a number
- optional x/y/z lines (needs a firmware change in detector + liveTask)
- bigger canvas on phones, dark/light readable

## Develop and test
```bash
# UI without a device (mock data, simulated events):
python -m http.server -d data 8766        # → http://localhost:8766/?mock=1
# deploy to ze trap (builds firmware + UI, flashes both over WiFi, checks ze capture count):
tools/prod.sh                              # or: tools/prod.sh --ui-only   (only data/ changed)
```
- PlatformIO (pioarduino, Arduino core 3.x), env `polh`. Windows: set `PYTHONUTF8=1` or esptool hangs.
- Web OTA: `curl -F "update=@littlefs.bin" http://polh-kill.local/update?type=fs` (`type=fw` for firmware).
  ArduinoOTA (`env polh-ota`) is often blocked by firewalls – use web OTA.
- USB (CH343 port): `pio run -e polh -t upload` and `-t uploadfs`. Captures live on a separate
  partition and survive both.
- Ze dataset on ze device survives UI and firmware updates. Never wipe it without asking Aaron.

## Rules
1. **Commit messages on `main`** are written in ze voice of ze Slovenian philosopher of ze README:
   English with "ze", "and so on, and so on", ideology jokes, *sniff* – **never his name**. Subject
   with an emoji, a short philosophical paragraph, then a factual bullet list of ze changes.
   Every commit on `main` is read aloud on Telegram: avoid ze word "waits" (always sounds like
   "mates"), write "is waiting".
2. **Code comments:** English, short, say *why*. A little "ze" flavour is welcome, ze facts stay precise.
3. Branch **`interview-voice-fixes`** belongs to Aaron (voice of ze interview): do not merge it, do not
   commit there. Its commits use a different voice (see its own `AGENTS.md`).
4. **No secrets in git.** WiFi, bot token, GitHub token live only in ze ESP's NVS (setup page).
5. Do not touch ze `gh-pages` branch by hand: ze ESP and two Actions write it.
6. Line endings LF (`.gitattributes`). Docs and comments in English.

## Further reading
`README.md` (EN + DE) · `docs/learning.md` · `docs/publishing.md` · `docs/led.md` · `docs/wiring.md`
· `docs/camera.md` · `docs/HANDOFF-voice.md` (on ze voice branch)
