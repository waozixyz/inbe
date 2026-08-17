#!/usr/bin/env python3
"""Compare two mono PCM WAV files by their short-window amplitude envelopes."""

import math
import struct
import sys
import wave


def envelope(path: str, window_ms: int = 100) -> list[float]:
    with wave.open(path, "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError(f"{path}: expected mono 16-bit PCM")
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    samples = struct.unpack(f"<{len(raw) // 2}h", raw)
    window = max(1, rate * window_ms // 1000)
    return [
        math.sqrt(sum(value * value for value in samples[i:i + window]) /
                  max(1, len(samples[i:i + window])))
        for i in range(0, len(samples), window)
    ]


def normalized(values: list[float]) -> list[float]:
    mean = sum(values) / max(1, len(values))
    centered = [value - mean for value in values]
    scale = math.sqrt(sum(value * value for value in centered)) or 1.0
    return [value / scale for value in centered]


def best_correlation(source: list[float], capture: list[float]) -> float:
    source = normalized(source)
    best = -1.0
    minimum = min(30, len(source))
    for offset in range(max(1, len(capture) - minimum + 1)):
        count = min(len(source), len(capture) - offset)
        if count < minimum:
            continue
        candidate = normalized(capture[offset:offset + count])
        score = sum(source[i] * candidate[i] for i in range(count))
        best = max(best, score)
    return best


if len(sys.argv) != 3:
    raise SystemExit("usage: audio-envelope-match.py SOURCE.wav CAPTURE.wav")

source_envelope = envelope(sys.argv[1])
capture_envelope = envelope(sys.argv[2])
capture_rms = math.sqrt(sum(value * value for value in capture_envelope) /
                        max(1, len(capture_envelope)))
correlation = best_correlation(source_envelope, capture_envelope)
print(f"capture_rms={capture_rms:.1f} envelope_correlation={correlation:.3f}")
if capture_rms < 80 or correlation < 0.20:
    raise SystemExit(1)
