"""Let ze comrades judge ze voice: render variants, send them to Telegram with 🔥 👍 😐 👎 🤮, read ze votes.

    python tools/voice_rate.py render <round.json> <gh-pages checkout>   render OGGs into voice/tests/
    python tools/voice_rate.py send   <round.json> <trap ip>             trap sends them with ze rating keyboard
    python tools/voice_rate.py results <round.json> <trap ip>            mean score per variant, best first

round.json:
    {"name": "r1", "text": "...commit message...",
     "variants": {"a": {"voice": "hr-HR-SreckoNeural", "pitch": "-10Hz", "rate": "+0%"}, ...}}
    optional per variant: "respell": true (Slovenian letters), "the": true (ze -> the)

Between render and send, ze checkout must be pushed and GitHub Pages must serve ze files
(Telegram fetches them by URL). Score: 🔥 5, 👍 4, 😐 3, 👎 2, 🤮 1.
"""
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import voice  # noqa: E402

PAGES = "https://randelab.github.io/polh-kill/"
EMOJI = {5: "🔥", 4: "👍", 3: "😐", 2: "👎", 1: "🤮"}


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def render(rnd, site):
    for key, v in rnd["variants"].items():
        out = Path(site) / "voice" / "tests" / f"{rnd['name']}_{key}.ogg"
        text = rnd["text"]
        if v.get("the"):  # test whether ze "ze" itself is ze problem
            text = text.replace("Ze ", "The ").replace(" ze ", " the ")
        voice.RESPELL = bool(v.get("respell"))  # Slovenian letters, made for sl-SI-RokNeural
        voice.render(text, out, voice=v.get("voice"), rate=v.get("rate"), pitch=v.get("pitch"))
        voice.RESPELL = False
        print(f"🎙 {out.name}: {v}")


def wait_for_pages(url, timeout=600):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            with urllib.request.urlopen(url + f"?t={int(time.time())}", timeout=10) as r:
                if r.status == 200:
                    return True
        except Exception:
            pass
        time.sleep(15)
    return False


def send(rnd, ip):
    keys = list(rnd["variants"])
    if not wait_for_pages(f"{PAGES}voice/tests/{rnd['name']}_{keys[-1]}.ogg"):
        sys.exit("❌ GitHub Pages does not serve ze files yet")
    for i, key in enumerate(keys, 1):
        sample = f"{rnd['name']}{key}"
        caption = (f"🎙 <b>Voice test {rnd['name'].upper()} · {key.upper()}</b> ({i}/{len(keys)})\n"
                   "Ze same commit, a different throat. Judge me: 🔥 👍 😐 👎 🤮")
        data = urllib.parse.urlencode({"url": f"{PAGES}voice/tests/{rnd['name']}_{key}.ogg",
                                       "sample": sample, "caption": caption}).encode()
        with urllib.request.urlopen(f"http://{ip}/api/voice/test", data=data, timeout=15) as r:
            print(sample, r.read().decode())
        time.sleep(4)  # keep ze order in Telegram


def results(rnd, ip):
    with urllib.request.urlopen(f"http://{ip}/api/voice/ratings", timeout=15) as r:
        votes = json.load(r)["votes"]
    rows = []
    for key, v in rnd["variants"].items():
        mine = [x for x in votes if x["sample"] == f"{rnd['name']}{key}"]
        mean = sum(x["score"] for x in mine) / len(mine) if mine else 0
        who = " ".join(f"{x['name']}{EMOJI[x['score']]}" for x in mine)
        rows.append((mean, key, len(mine), v, who))
    for mean, key, n, v, who in sorted(rows, key=lambda r: -r[0]):
        print(f"{key.upper()}  {mean:4.2f}  ({n} votes)  {v}  {who}")


if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in ("render", "send", "results"):
        sys.exit(__doc__)
    {"render": render, "send": send, "results": results}[sys.argv[1]](load(sys.argv[2]), sys.argv[3])
