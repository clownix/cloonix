#!/usr/bin/env python3
"""demo_voice - the phrase the Demo panel says when a demo starts.

    ./demo_voice.py                   every line of demos/voice
    ./demo_voice.py cloonix/cnt.py    one line of it

demos/voice gives the phrase of each demo; its clip is
audio/<area>/<demo>.ogg in jscloonix. A clip no line asks for is removed.

Needs kokoro and ffmpeg, on the machine that makes the clips only:
  pip install kokoro-onnx
  cd ~/kokoro
  wget https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx
  wget https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/voices-v1.0.bin
"""
import glob
import os
import subprocess
import sys

import numpy

HERE = os.path.dirname(os.path.abspath(__file__))
LIST = os.path.abspath(os.path.join(HERE, "..", "demos", "voice"))
AUDIO = os.path.abspath(os.path.join(HERE, "..", "cloonix", "cloonsrc", "jscloonix",
                                     "jscloonix", "audio"))
MODEL = os.path.expanduser(os.environ.get("KOKORO_DIR", "~/kokoro"))
VOICE = "bf_emma"          # british english, female
SPEED = 0.9                # a little slower than trained, for a foreign ear
PAUSE = 0.5                # seconds between two sentences

def phrases():
    out = {}
    for ln in open(LIST):
        if ln.strip() and not ln.startswith("#"):
            demo, text = ln.split(None, 1)
            out[demo] = text.strip()
    return out

def speak(tts, text, out):
    parts = []
    for sentence in text.replace(". ", ".\n").split("\n"):
        samples, rate = tts.create(sentence, voice=VOICE, speed=SPEED,
                                   lang="en-gb")
        parts += [samples, numpy.zeros(int(rate * PAUSE), numpy.float32)]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "f32le",
                    "-ar", str(rate), "-ac", "1", "-i", "-",
                    "-c:a", "libopus", "-b:a", "24k", out],
                   input=numpy.concatenate(parts[:-1]).tobytes(), check=True)

def main():
    from kokoro_onnx import Kokoro
    tts = Kokoro(os.path.join(MODEL, "kokoro-v1.0.onnx"),
                 os.path.join(MODEL, "voices-v1.0.bin"))
    want = phrases()
    clips = dict((os.path.join(AUDIO, d[:-len(".py")] + ".ogg"), d)
                 for d in want)
    for old in glob.glob(os.path.join(AUDIO, "**", "*.ogg"), recursive=True):
        if old not in clips:
            os.remove(old)
            print("removed %s" % os.path.relpath(old, AUDIO))
    for out, demo in clips.items():
        if sys.argv[1:] and demo not in sys.argv[1:]:
            continue
        speak(tts, want[demo], out)
        print("%-26s %6d bytes  %s" % (demo, os.path.getsize(out), want[demo]))

main()
