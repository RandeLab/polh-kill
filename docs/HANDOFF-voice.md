# Handoff: interview voice (branch `interview-voice-fixes`)

Written 2026-11-08 when moving this work from Windows/Claude Code to Linux/opencode.
Rules for agents: [`AGENTS.md`](../AGENTS.md).

## Goal
The interview player on https://randelab.github.io/polh-kill/ (section "Ze interview") plays
`data/interview.mp3` with a live transcript from `data/interview.json`. The philosopher is a
Slovenian TTS voice reading English; a few words come out totally wrong. This branch fixes them.
**Pages keeps the original until Aaron says the branch version is better. Then, and only then,
merge into `main`** – the "Deploy ze dashboard" Action copies `data/interview.*` to `gh-pages`.

## State (commit `923e3cd`)
- `tools/make_interview.py` sends the philosopher's lines through `voice.respell()`, so the word
  fixes apply; the transcript keeps the normal text.
- "So the trap waits, the mouse waits, the cheese waits" → "… is waiting …" ("waits" is unfixable).
- Re-recorded interview: 128 s, judged "a little better than the original" by Aaron.

| | voice | rate | pitch |
|---|---|---|---|
| philosopher (`PHIL`) | `sl-SI-RokNeural` | +6 % | −4 Hz |
| host (`HOST`) | `en-GB-SoniaNeural` | 0 | 0 |
| Telegram commit voices (`tools/voice.py`) | `sl-SI-RokNeural` | +4 % | −4 Hz |

Fix list in `tools/voice.py` (all chosen by ear, 2026-11-08):

| word | spoken as | note |
|---|---|---|
| waiting | uejjting | |
| world | uörrld | 12+ spellings tried before this one |
| wants | uonc | "uonts" worked but was stretched |
| of course | ofkors | phrase (`FIX_PHRASES`) |
| open source | oupen sors | phrase |
| waits | – | always sounds like "mates" → rephrase to "is waiting" |

## Setup on Linux
```bash
git clone git@github.com:RandeLab/polh-kill.git && cd polh-kill
git checkout interview-voice-fixes
python3 -m venv .venv && . .venv/bin/activate
pip install edge-tts cmudict          # cmudict only needed if RESPELL is switched on
sudo apt install ffmpeg               # voice.render() makes OGG/Opus with it
python tools/make_interview.py        # writes data/interview.mp3 + data/interview.json
```
edge-tts uses Microsoft's online Edge TTS service (no key). It sometimes returns no audio;
`voice._tts()` retries 4×, `make_interview.py` does not yet (just run it again).

Preview the page locally: `python -m http.server -d data 8766` → http://localhost:8766/
(the device UI with mock data; the interview player is in the footer). The Pages dashboard
lives in `pages/` and needs `data/style.css`, `interview.*`, `transcript.js` next to it.

## How to test one word (the method that worked)
Render the word in its interview sentence with three spellings and let Aaron pick:
```python
import sys; sys.path.insert(0, "tools"); import voice
tpl = "The most honest machine in the {}."
parts = [f"{l}. " + tpl.format(v) for l, v in zip(["Ej", "Bi", "Si"], ["uörrld", "verld", "uooreld"])]
voice.render(" ".join(parts), "/tmp/world.ogg", already_respelled=True)   # voice: sl-SI-RokNeural
```
("Ej, Bi, Si" is how the Slovenian voice says A, B, C.) Send him the file, put the winner into
`FIX` or `FIX_PHRASES`, run `python tools/make_interview.py` again, send him the new MP3.
Slovenian spelling tricks that helped: `c` = "ts" (uonc), doubled letters against swallowed or
stretched sounds (uejjting, uörrld), `ou` for English "o" (oupen).

## Already tried and rejected (do not repeat)
- Respelling every word in Slovenian letters via the CMU dictionary (`RESPELL = True`): some words
  better, many worse.
- Other voices: Croatian (hr-HR-SreckoNeural, several pitches and speeds), Serbian, Bosnian,
  Czech, Polish, Russian, Ukrainian, German/Italian multilingual voices. Rated 👎/🤮 on Telegram.
- Splicing a single word from an English voice into the sentence: "doesn't work at all".
- `tools/voice_rate.py` + the 🔥 👍 😐 👎 🤮 Telegram rating: works technically, but needs the
  trap on the RandeLab LAN (`http://192.168.1.102`) and the `gh-pages` branch; not needed for
  single-word fixes.

## Elsewhere in the project (context only)
- `main` has the same `tools/voice.py` fix list: every commit on `main` is rendered by
  `.github/workflows/voice.yml` and sent by the trap as a Telegram voice message. Commits on this
  branch are not read aloud until they are merged.
- Commit messages on `main` are in the voice of the Slovenian philosopher; on this branch in
  Barbara's (see `AGENTS.md`). Never name the philosopher.
- Firmware, wiring, learning: `README.md`, `docs/`. Aaron's vault notes (German):
  `Projekte/polh-kill/Stimmtest Philosoph.md` in the Obsidian vault "Hirni".

## Done when
Aaron says the branch interview is better than the one on Pages → merge `interview-voice-fixes`
into `main` (remove or keep `AGENTS.md` / this file as he prefers), push, check the "Deploy ze
dashboard" Action and the player on the Pages site.
