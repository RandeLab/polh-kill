"""Render voice messages onto a gh-pages checkout (run by .github/workflows/voice.yml).

    python tools/voice_publish.py <site-dir> <before-sha> <after-sha>

- every commit in before..after (max 5) -> voice/<sha7>.ogg, newest first in voice/index.json
- every fixed line in tools/voice_lines.json -> voice/esp/<id>.ogg (only when its text changed)
Ze ESP reads voice/index.json and sends new commits as Telegram voice messages; ze fixed lines
it sends after its own pushes. Ze bot token never leaves ze ESP.
"""
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import voice  # noqa: E402

KEEP = 15


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def commits(before, after):
    if before and set(before) != {"0"}:
        try:
            out = git("rev-list", "--reverse", "--no-merges", f"{before}..{after}")
            shas = out.split()
            if shas:
                return shas[-5:]
        except subprocess.CalledProcessError:
            pass  # force push or unknown base: speak only ze newest
    return [after]


def main():
    site, before, after = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    vdir = site / "voice"
    (vdir / "esp").mkdir(parents=True, exist_ok=True)
    idx_path = vdir / "index.json"
    index = json.loads(idx_path.read_text(encoding="utf-8")) if idx_path.exists() else {"commits": []}

    for sha in commits(before, after):
        msg = git("log", "-1", "--format=%B", sha)
        subject = msg.splitlines()[0].strip()
        if subject.startswith("Merge"):
            continue
        file = f"voice/{sha[:7]}.ogg"
        spoken = voice.render(msg, site / file)
        index["commits"] = [c for c in index["commits"] if c["sha"] != sha[:7]]
        index["commits"].insert(0, {"sha": sha[:7], "subject": subject, "file": file,
                                    "ts": int(git("log", "-1", "--format=%ct", sha))})
        print(f"🎙 {sha[:7]} {subject}\n   {spoken}")

    for old in index["commits"][KEEP:]:
        (site / old["file"]).unlink(missing_ok=True)
    index["commits"] = index["commits"][:KEEP]
    index["updated"] = int(time.time())

    lines = json.loads((Path(__file__).resolve().parent / "voice_lines.json").read_text(encoding="utf-8"))
    esp = {}
    for key, text in lines.items():
        if key.startswith("_"):
            continue
        spoken = voice.respell(text)
        txt = vdir / "esp" / f"{key}.txt"
        if not txt.exists() or txt.read_text(encoding="utf-8") != spoken or not txt.with_suffix(".ogg").exists():
            voice.render(spoken, txt.with_suffix(".ogg"), already_respelled=True)
            txt.write_text(spoken, encoding="utf-8")
            print(f"🎙 esp/{key}: {spoken}")
        esp[key] = f"voice/esp/{key}.ogg"
    index["esp"] = esp
    idx_path.write_text(json.dumps(index, ensure_ascii=False, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
