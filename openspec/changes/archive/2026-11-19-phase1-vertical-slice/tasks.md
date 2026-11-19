# Tasks: Phase 1 Vertical Slice

- [x] 1. Set up repository licenses (`LICENSE`, `LICENSE-ART`, `CREDITS.md`), CMake build system, Unity test harness, and procedural CC BY 4.0 asset generator (`tools/gen_assets.py`).
- [x] 2. Implement pure-C core modules: `src/core/math3d`, `src/core/calendar`, `src/render/voxel_mesher`, `src/net/net_proto`, `src/platform/touch`.
- [x] 3. Implement deterministic simulation (`src/sim/world`) with Grand Festive Hall (24 portals, voxel fireplace, 24-ornament tree, SANTA-BIOS) and complete Day 1 (*Neon Strip '96* — Duke Nukem 3D homage with Shrink-Ray+Stomp, Pipebombs, Jetpack, Holo-Decoy, Boss, Nuke-Exit, Golden Shades shard).
- [x] 4. Implement SDL3 + OpenGL 3.3 / WebGL2 low-res dynamic-lighting & shadow-map renderer (`src/render/gl_renderer`), retro audio synthesizer (`src/audio/retro_audio`), split-screen viewports, touch overlay, and `src/platform/sdl_main.c`.
- [x] 5. Implement Web shell (`web/index.html`), WebRTC DataChannel multiplayer bridge (`web/webrtc_bridge.js` + signaling worker), and WebXR stereo VR bridge (`web/webxr_bridge.js`).
- [x] 6. Implement Unity C unit tests achieving ≥95% line coverage (`gcovr`) and automated headless deterministic playthrough tests (`tests/game/`).
- [x] 7. Configure GitHub Actions CI (`ci.yml`) and automated `gh-pages` deployment (`deploy-gh-pages.yml`), verify native + Emscripten builds, commit to `main`, and publish live playtest build to `gh-pages`.
