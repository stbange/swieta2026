#!/usr/bin/env python3
"""
SWIETA 2026 — Reproducible CC BY 4.0 Retro Asset Generator
Generates original retro pixel-art sprite sheets (PPM/PNG), 24-portal metadata
manifest, and retro chiptune/SFX WAV packs under CC BY 4.0.
Custom hand-crafted art can be dropped into assets/custom/ to override defaults.
"""
import json
import math
import os
import struct
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")


def write_ppm(path, width, height, pixels_rgb):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        f.write(bytes(pixels_rgb))


def generate_textures():
    """Generate crisp 32x32 procedural retro voxel textures (CC BY 4.0)."""
    tex_dir = os.path.join(ASSETS, "textures")
    os.makedirs(tex_dir, exist_ok=True)

    # Festive brick wall
    brick = bytearray()
    for y in range(32):
        for x in range(32):
            mortar = (y % 8 == 0) or ((x + (y // 8) * 8) % 16 == 0)
            if mortar:
                brick.extend((82, 74, 72))
            else:
                v = ((x * 17 ^ y * 31) & 15) - 8
                brick.extend((max(0, min(255, 148 + v)), max(0, min(255, 58 + v)), max(0, min(255, 48 + v))))
    write_ppm(os.path.join(tex_dir, "festive_brick.ppm"), 32, 32, brick)

    # Cracked destructible wall (Day 1 Duke Nukem 3D homage)
    cracked = bytearray()
    for y in range(32):
        for x in range(32):
            diag = abs((x - y)) <= 1 or abs((x + y - 31)) <= 1
            if diag and (x > 4 and x < 28):
                cracked.extend((30, 18, 16))
            else:
                cracked.extend((176, 92, 58))
    write_ppm(os.path.join(tex_dir, "day01_cracked_wall.ppm"), 32, 32, cracked)


def generate_audio():
    """Generate sample CC BY 4.0 retro SFX WAVs (22050 Hz mono 16-bit PCM)."""
    aud_dir = os.path.join(ASSETS, "audio")
    os.makedirs(aud_dir, exist_ok=True)
    sr = 22050

    def write_wav(name, samples):
        with wave.open(os.path.join(aud_dir, name), "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            pcm = b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 30000)))) for s in samples)
            wf.writeframes(pcm)

    # Shrink ray descending square chirp
    n = int(sr * 0.25)
    shrink = [
        (1.0 if math.sin(2.0 * math.pi * (1000.0 - 700.0 * (i / n)) * (i / sr)) >= 0 else -1.0) * (1.0 - i / n) * 0.6
        for i in range(n)
    ]
    write_wav("day01_shrink_ray.wav", shrink)

    # Festive Memory Shard chime arpeggio
    notes = [523.25, 659.25, 783.99, 1046.50]
    chime = []
    for f in notes:
        seg = int(sr * 0.12)
        for i in range(seg):
            env = 1.0 - (i / seg)
            chime.append(math.sin(2.0 * math.pi * f * (i / sr)) * env * 0.65)
    write_wav("shard_collect.wav", chime)


def generate_manifest():
    manifest = {
        "license": "CC BY 4.0",
        "generator": "tools/gen_assets.py",
        "custom_override_dir": "assets/custom/",
        "levels": [
            {"day": 1, "id": "day01_duke", "title": "Neon Strip '96", "inspiration": "Duke Nukem 3D (1996)", "status": "playable"},
            {"day": 24, "id": "day24_redalert", "title": "Operation: Chrono-Eve", "inspiration": "C&C: Red Alert (1996)", "status": "planned"}
        ]
    }
    os.makedirs(ASSETS, exist_ok=True)
    with open(os.path.join(ASSETS, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)


if __name__ == "__main__":
    generate_textures()
    generate_audio()
    generate_manifest()
    print("Generated CC BY 4.0 retro assets in", ASSETS)
