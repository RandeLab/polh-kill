"""Ze voice of ze trap: English text -> Slovenian-accented speech -> OGG/Opus for Telegram voice messages.

    pip install edge-tts cmudict          (+ ffmpeg on PATH, or: pip install imageio-ffmpeg)
    python tools/voice.py "Ze trap is waiting." out.ogg      one line
    python tools/voice.py --show "Ze trap is waiting."      only print ze respelled text

Today (RESPELL = False): a Croatian voice reads plain English, which has ze best accent.
Before, why respelling: ze Slovenian neural voice (sl-SI-RokNeural) reads every word with Slovenian
spelling rules - "waiting" becomes "va-i-ting", which is not an accent any more, it is a different
word. So every English word is looked up in ze CMU pronouncing dictionary and written down again in
Slovenian letters ("waiting" -> "uejting", "the" -> "ze"). Ze voice then says ze right English
sounds, but with its own melody - better English, same accent. And so on, and so on.
"""
import asyncio
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# 2026-11-08, chosen by ear: ze Croatian voice reading plain English has ze best accent of all
# Slavic voices, pitched down a little. Ze Slovenian voice + respelling below is kept, switched off.
VOICE, RATE, PITCH = "hr-HR-SreckoNeural", "+0%", "-10Hz"
RESPELL = False  # True: Slovenian letters from ze CMU dictionary (made for sl-SI-RokNeural)

# plain mode: abbreviations spelled out, otherwise ze voice reads them as Croatian words
PLAIN = {
    "esp32-s3": "E S P thirty two S three", "esp32": "E S P thirty two", "esp": "E S P", "github": "git hub",
    "gpl": "G P L", "ui": "U I", "ota": "O T A", "led": "L E D", "rgb": "R G B", "nvs": "N V S",
    "csv": "C S V", "json": "jason", "api": "A P I", "mp3": "M P three", "ogg": "ogg", "wifi": "wifi",
    "tls": "T L S", "ap": "A P", "rls": "R L S", "polh-kill": "polh kill",
}

# words ze dictionary does not know, or which ze philosopher says his own way
LEXICON = {
    "polh": "polh", "polh-kill": "polh kil", "esp": "e es pi", "esp32": "e es pi zerti tu",
    "esp32-s3": "e es pi zerti tu es tri", "s3": "es tri", "gpl": "dži pi el", "ui": "ju aj",
    "github": "git hab", "telegram": "telegram", "ota": "o ti ej", "led": "el i di", "rgb": "ar dži bi",
    "ze": "ze", "zis": "zis", "zat": "zet", "sniff": "šnif", "and": "end", "lab": "lep",
    "wifi": "uaj faj", "api": "ej pi aj", "ai": "ej aj", "ok": "okej", "mousetrap": "maustrep",
    "rls": "ar el es", "nvs": "en vi es", "mp3": "em pi tri", "json": "džej son", "csv": "si es vi",
    # tuned by ear (2026-11-08): letter-perfect respelling is not always what sounds right
    "waiting": "uejjting", "trap": "trap", "traps": "traps", "world": "vöreld", "published": "published",
    "eight": "ejt", "ideology": "ajdi-ologi", "philosopher": "philosopher", "wanted": "vonted",
    "model": "modell", "rewritten": "rewritten", "nothing": "noting",
}

VOWELS = {
    "AA": "o", "AE": "e", "AO": "o", "AW": "au", "AY": "aj", "EH": "e", "ER": "er", "EY": "ej",
    "IH": "i", "IY": "i", "OW": "ou", "OY": "oj", "UH": "u", "UW": "u",
}
CONS = {
    "B": "b", "CH": "č", "D": "d", "DH": "z", "F": "f", "G": "g", "HH": "h", "JH": "dž", "K": "k",
    "L": "l", "M": "m", "N": "n", "NG": "ng", "P": "p", "R": "r", "S": "s", "SH": "š", "T": "t",
    "TH": "t", "V": "v", "W": "u", "Y": "j", "Z": "z", "ZH": "ž",
}

ONES = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen " \
       "sixteen seventeen eighteen nineteen".split()
TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()

_cmu = None


def cmu():
    global _cmu
    if _cmu is None:
        import cmudict
        _cmu = cmudict.dict()
    return _cmu


def number_words(n):
    if n < 20:
        return ONES[n]
    if n < 100:
        return TENS[n // 10] + ("" if n % 10 == 0 else " " + ONES[n % 10])
    if n < 1000:
        return ONES[n // 100] + " hundred" + ("" if n % 100 == 0 else " " + number_words(n % 100))
    if n < 1_000_000:
        return number_words(n // 1000) + " thousand" + ("" if n % 1000 == 0 else " " + number_words(n % 1000))
    return " ".join(ONES[int(c)] for c in str(n))


def phonemes_to_sl(ph):
    out = []
    for i, p in enumerate(ph):
        base, stress = re.match(r"([A-Z]+)(\d?)", p).groups()
        if base == "AH":  # stressed "cut" -> a, unstressed schwa -> e (Slovenian writes its schwa as e)
            out.append("a" if stress == "1" else "e")
        elif base in VOWELS:
            out.append(VOWELS[base])
        elif base == "NG" and i + 1 < len(ph) and ph[i + 1] in ("K", "G"):
            out.append("n")
        else:
            out.append(CONS.get(base, ""))
    return "".join(out)


def fix_endings(sl):
    """Slovenian swallows final consonants before a pause and says a final l like w: double them."""
    if re.search(r"[^aeiou]t$", sl):
        return sl + "t"          # first -> ferst -> ferstt
    if re.search(r"[aeiou]l$", sl):
        return sl + "l"          # model -> modell
    return sl


def respell_word(w):
    low = w.lower()
    if low in LEXICON:
        sl = LEXICON[low]
    elif low in cmu():
        sl = fix_endings(phonemes_to_sl(cmu()[low][0]))
    else:
        return w  # unknown: let ze voice improvise, like every philosopher
    return sl.capitalize() if w[:1].isupper() else sl


def clean(text):
    """Strip what nobody should hear: emoji, markdown, URLs, bullet lists, co-author lines."""
    lines = []
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith(("-", "*  ", "Co-Authored-By", "Signed-off-by", "#")) and not s.startswith("*sniff"):
            if lines and lines[-1] != "":
                lines.append("")
            continue
        lines.append(s if s[-1] in ".!?:;," else s + ".")  # a subject line also deserves a pause
    text = " ".join(l if l else "\n" for l in lines)
    text = re.sub(r"https?://\S+", "", text)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\*([^*]+)\*", r"\1.", text)  # *sniff* -> sniff.
    text = re.sub(r"[^\w\s.,!?;:'()\-/<>%]", " ", text)  # emoji and friends
    text = text.replace("->", ", ").replace("/", " ").replace("%", " percent")
    text = re.sub(r"\d+", lambda m: number_words(int(m.group())), text)
    return re.sub(r"\s+", " ", text).strip()


def respell(text):
    if not RESPELL:  # abbreviations and versions first, before clean() turns digits into words
        text = re.sub(r"\bv(\d+)\.(\d+)\.(\d+)\b", r"version \1 point \2 point \3", text)
        text = re.sub(r"[A-Za-z][A-Za-z0-9]*(?:-[A-Za-z0-9]+)*", lambda m: PLAIN.get(m.group().lower(), m.group()), text)
        return clean(text)
    return re.sub(r"[A-Za-z][A-Za-z'\-]*[A-Za-z0-9]|[A-Za-z]", lambda m: respell_word(m.group()), clean(text))


def ffmpeg():
    exe = os.environ.get("FFMPEG") or shutil.which("ffmpeg")
    if exe:
        return exe
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


async def _tts(text, mp3, voice=None, rate=None, pitch=None):
    import edge_tts
    for attempt in range(4):  # ze service sometimes answers with silence: ask again
        try:
            await edge_tts.Communicate(text, voice or VOICE, rate=rate or RATE, pitch=pitch or PITCH).save(mp3)
            return
        except edge_tts.exceptions.NoAudioReceived:
            if attempt == 3:
                raise
            await asyncio.sleep(2 + 2 * attempt)


def render(text, out, already_respelled=False, voice=None, rate=None, pitch=None):
    """Speak text into an OGG/Opus file (Telegram voice message format). Returns ze spoken text."""
    spoken = text if already_respelled else respell(text)
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        mp3 = os.path.join(tmp, "v.mp3")
        asyncio.run(_tts(spoken, mp3, voice, rate, pitch))
        subprocess.run([ffmpeg(), "-loglevel", "error", "-y", "-i", mp3, "-ac", "1", "-ar", "48000",
                        "-c:a", "libopus", "-b:a", "24k", "-application", "voip", str(out)], check=True)
    return spoken


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--show":
        print(respell(" ".join(sys.argv[2:])))
    elif len(sys.argv) == 3:
        print(render(sys.argv[1], sys.argv[2]))
    else:
        print(__doc__)
