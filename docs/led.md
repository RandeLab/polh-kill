# Ze LED speaks

The RGB LED on the board (WS2812, GPIO 48) is ze face of ze trap. It has two layers: a steady
colour that says what ze trap *is* doing, and short flashes on top that say what *just happened*.
You do not need ze web UI to know what is going on. You only need to look. This is, how you say,
ze most direct form of communication, without ideology in between. Almost.

## Steady state – what ze trap is doing

| colour | meaning |
|---|---|
| ⚪ white, dim | booting |
| 🔵 blue, breathing | setup hotspot **polh-kill-AP** is open – join it with your phone |
| 🟢 green, dim | online and **armed**: snaps go to Telegram |
| 🟡 yellow, dim | online but **disarmed** (`/disarm`, or the toggle in the web UI) |
| 🟣 magenta, breathing | a **learning test is armed** – do ze thing now (snap, knock, walk …) |
| 🟣 magenta, solid | **OTA update** running – do not unplug |
| 🟠 orange, blinking | **no accelerometer** found – check the wiring ([wiring.md](wiring.md)) |

## Flashes – what just happened

| flash | meaning |
|---|---|
| 🔴 red, blinking 3 s | **SNAP!** The model is sure enough to send an alert |
| 🟠 orange, 0.4 s | capture classified as **bump** |
| 🔵 blue, 0.4 s | capture classified as **footstep** |
| 🟡 yellow, 0.4 s | capture classified as **mouse** |
| ⚪ white, 0.4 s | capture classified as **other** |
| 🟣 violet, 0.4 s | capture classified as one of **your own classes** (cucumber, sausage, …) |
| 🟢 green double blink | a **labelled sample was learned** (lab or Telegram button) |
| 🩵 cyan double blink | captures were **pushed to GitHub Pages** |

A lab test never blinks red: it is a test, not a tragedy.

Code: [`src/status_led.h`](../src/status_led.h). Brightness is kept low on purpose – a mouse
should not see ze trap thinking.
