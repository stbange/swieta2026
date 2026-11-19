# ŚWIĘTA 2026: The Grand Hall of 24 Portals

A retro-3D first-person advent calendar game written in **C11 + SDL3 + OpenGL (3.3 Core / ES 3.0 WebGL2)** targeting **Windows, Linux, Web (Emscripten), and WebXR**.

- **Playtest Web Build (`gh-pages`):** https://stbange.github.io/swieta2026/
- **Full Game Design & Technical Concept:** [docs/CONCEPT.md](docs/CONCEPT.md)
- **Living Specifications:** [openspec/](openspec/)

## Features
- **The Grand Festive Hall (3D Hub):** Warm candlelit winter cathedral with a dynamic-lit voxel hearth, central 24-ornament Christmas tree, SANTA-BIOS terminal, and 24 portals unlocking daily from **1.12.2026 to 24.12.2026** (with Playtest Unlock-All mode for pre-December testing).
- **24 Classic Game Homage Portals:** ~20-minute levels celebrating 24 iconic 1990s classics (*Duke Nukem 3D* through *C&C: Red Alert*), each introducing 2–3 signature mechanics, weapons, and items tied together by the *LAN Before Christmas (1999 → 2026)* meta-story.
- **Hybrid Retro 3D Graphics:** Low-resolution offscreen framebuffer (`320×240` / `480×270`) combining **low-poly 3D meshes**, **lit 8-direction billboard sprites**, and **chunked destructible voxels** with **real-time dynamic lighting and shadow mapping**.
- **Cross-Platform Input & VR:** Keyboard/Mouse, Gamepad, automatic **Mobile Touch Controls** on phones/tablets, and stereoscopic **WebXR VR**.
- **Multiplayer:** Local **2–4 player Split-Screen** plus Web-first **WebRTC DataChannel** peer-to-peer multiplayer (room codes via lightweight signaling relay + zero-server copy-paste SDP fallback).
- **Engineering Rigor:** OpenSpec Spec-Driven Development, Unity C unit tests (**≥95% line coverage** enforced in CI), and automated deterministic headless game playthrough tests.

## Building & Testing

### Native Linux / Windows (CMake + Ninja/Make)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/swieta2026
```

### Unit Tests & Coverage (≥95% Gate)
```bash
cmake -B build-cov -DSWIETA_ENABLE_COVERAGE=ON -DSWIETA_BUILD_APP=OFF
cmake --build build-cov
ctest --test-dir build-cov --output-on-failure
gcovr --root . --filter 'src/(core|sim|levels|net|render/voxel_mesher|platform/touch)' --fail-under-line 95
```

### Web / WebXR Build (Emscripten)
```bash
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

## Licensing
- **Code:** [MIT License](LICENSE)
- **Art, Audio, Levels & Story:** [CC BY 4.0](LICENSE-ART)
