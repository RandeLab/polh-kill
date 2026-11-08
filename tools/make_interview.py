"""Generate data/interview.mp3 + data/interview.json (timed transcript): a short radio interview with ze philosopher of ze trap.

    pip install edge-tts
    python tools/make_interview.py

The philosopher is a Slovenian neural voice reading English (sl-SI-RokNeural),
the interviewer an English one. Ze philosopher's lines go through voice.respell(), so ze few
words he gets totally wrong ("waiting", "world", "of course" ...) use ze spellings tuned by ear;
ze transcript keeps ze normal text. Uses Microsoft's online Edge TTS service.
"""
import asyncio
import json
from pathlib import Path

import edge_tts

import voice as voice_mod  # ze same word fixes as ze Telegram voice messages (FIX / FIX_PHRASES)

HOST = ("en-GB-SoniaNeural", "+0%", "+0Hz")
PHIL = ("sl-SI-RokNeural", "+6%", "-4Hz")

SCRIPT = [
    (HOST, "Welcome. Today we talk to the philosopher behind polh-kill, a mousetrap that learns. So, what is this thing?"),
    (PHIL, "Look, here is the paradox. A mousetrap is the most honest machine in the world. It does one thing. "
           "Clack. And it is over. But nobody is there to hear it! So the trap is waiting, the mouse is waiting, the cheese is waiting. "
           "And you, you do not know. This is unbearable, no?"),
    (HOST, "But why does a mousetrap need machine learning?"),
    (PHIL, "Because a snap is not simply a snap! The trap feels everything. Footsteps, ze washing machine, "
           "somebody knocking on the table. So it must learn what is the real snap. You arm a test, you let the trap go, "
           "and the model updates itself, immediately. Recursively! Every new example changes how it sees all the others. "
           "This is, how you say, pure ideology in its most efficient form."),
    (HOST, "And then it sends you a Telegram message."),
    (PHIL, "Yes, and this is the beautiful obscenity of it. Trap snapped. Confidence ninety three percent. "
           "Ninety three! Not one hundred. Even the machine has its doubts. "
           "This is more honest than most of our politicians, I tell you."),
    (HOST, "What about the mouse?"),
    (PHIL, "Ah, the mouse. You know, the mouse does not know that it wants the cheese. The cheese knows. "
           "The cheese is not the goal. The cheese is the ideology."),
    (HOST, "Last question. Can people build their own?"),
    (PHIL, "Of course! It is open source. GPL three. An ESP thirty two, a small accelerometer, a wooden trap. "
           "Hack it, remix it, share it. And so on, and so on."),
]


async def main():
    out = Path(__file__).resolve().parent.parent / "data" / "interview.mp3"
    parts = []
    for i, ((voice, rate, pitch), text) in enumerate(SCRIPT):
        tmp = out.with_name(f"_part{i}.mp3")
        spoken = voice_mod.respell(text) if voice == PHIL[0] else text
        await edge_tts.Communicate(spoken, voice, rate=rate, pitch=pitch).save(str(tmp))
        parts.append(tmp)
    # all parts share the same MP3 format (24 kHz mono 48 kbit/s CBR), so plain concatenation is
    # a valid stream - and the byte count gives the exact time where each part starts
    transcript, t = [], 0.0
    with out.open("wb") as f:
        for p, ((voice, _, _), text) in zip(parts, SCRIPT):
            data = p.read_bytes()
            dur = len(data) * 8 / 48000
            transcript.append({"start": round(t, 2), "end": round(t + dur, 2),
                               "who": "host" if voice == HOST[0] else "philosopher", "text": text})
            t += dur
            f.write(data)
            p.unlink()
    out.with_suffix(".json").write_text(json.dumps(transcript, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"wrote {out} ({out.stat().st_size // 1024} kB, {t:.0f} s) + transcript")


asyncio.run(main())
