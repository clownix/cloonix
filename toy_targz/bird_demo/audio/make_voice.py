#!/usr/bin/env python3
"""make_voice.py - the clips of bird_demo/voice, into bird_demo/audio/<demo>.ogg.

    ./make_voice.py                    every line of voice
    ./make_voice.py pim_sample.py      one line of it

Same voice as debug/demo_voice.py. Needs kokoro and ffmpeg, on the machine
that makes the clips only:
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

AUDIO = os.path.dirname(os.path.abspath(__file__))
HERE = os.path.dirname(AUDIO)
LIST = os.path.join(HERE, "voice")
MODEL = os.path.expanduser(os.environ.get("KOKORO_DIR", "~/kokoro"))
VOICE = "bf_emma"
SPEED = 0.9
PAUSE = 0.5

def phrases():
    out = {}
    for ln in open(LIST):
        if ln.strip() and not ln.startswith("#"):
            demo, text = ln.split(None, 1)
            out[os.path.basename(demo)] = text.strip()
    return out

def speak(tts, text, out):
    parts = []
    for sentence in text.replace(". ", ".\n").split("\n"):
        samples, rate = tts.create(sentence, voice=VOICE, speed=SPEED,
                                   lang="en-gb")
        parts += [samples, numpy.zeros(int(rate * PAUSE), numpy.float32)]
    os.makedirs(AUDIO, exist_ok=True)
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
    for old in glob.glob(os.path.join(AUDIO, "*.ogg")):
        if old not in clips:
            os.remove(old)
            print("removed %s" % os.path.basename(old))
    for out, demo in clips.items():
        if sys.argv[1:] and demo not in sys.argv[1:]:
            continue
        speak(tts, want[demo], out)
        print("%-36s %6d bytes" % (demo, os.path.getsize(out)))

main()
