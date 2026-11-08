# AGENTS.md – branch `interview-voice-fixes`

Instructions for any coding agent (opencode, Claude Code, …) working on this branch.
Read `docs/HANDOFF-voice.md` first: it has the state, the setup and everything already tried.

## What this branch is for
Make the interview audio (`data/interview.mp3` + `data/interview.json`) sound better than the
original on GitHub Pages, by fixing single words the Slovenian TTS voice says wrong.
The re-recorded interview on this branch is already "a little better" than the original (Aaron, 2026-11-08).

## Hard rules
1. **Do not merge into `main`, do not push `main`, do not touch the `gh-pages` branch.**
   The Pages site keeps the original interview until Aaron explicitly says the branch version is better.
   Pushing this branch does not deploy anything (the Actions only run on `main`).
2. **Commit messages on this branch are written in English, in the voice and personality of
   Barbara** – a Swiss philosopher and host of a philosophy show: calm, precise, curious, asks the
   question behind the question, likes ethics, time, work, responsibility and the middle of life,
   a dry Swiss understatement. She is the sole authority on these voice questions and may bring in
   her view of the world. Name her at most as **"Barbara"**, never by her surname.
   Format: subject line with an emoji, a short reflective paragraph in her voice, then a factual
   bullet list of the changes.
3. Replies to Aaron in **English** (he may write German). Code comments and docs in English.
4. **Never switch the voice and never respell everything.** Aaron tried that for hours and rejected
   it. Keep `sl-SI-RokNeural` reading plain English; fix only single words he reports, in
   `FIX` / `FIX_PHRASES` in `tools/voice.py`.
5. To fix a word: render **3 spellings (A/B/C) inside the sentence where the word occurs**, send
   Aaron the audio, put his pick into the fix list, re-record. Do not guess winners yourself.
6. If a word cannot be fixed (e.g. "waits" → sounds like "mates"), rephrase the sentence instead
   ("is waiting"). Keep the transcript in `data/interview.json` identical to what is spoken
   (apart from the fix spellings, which only affect the audio).
7. No secrets in the repo. Bot token, WiFi and GitHub token live only in the ESP's NVS.
