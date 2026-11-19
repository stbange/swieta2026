# Tasks: Phase 1.5 — Chunked Textured Engine & Full 20-Minute Day 1

- [x] 1. Upgrade `src/render/voxel_mesher.h/.c` to chunked `128×24×128` (`16³` chunks) with per-chunk dirty flags, 29 retro materials with atlas UVs, breakable glass, mirror detection, and dynamic door opening.
- [x] 2. Upgrade `src/sim/world.h/.c` with 5 weapons (Pistol, Shotgun, Shrink-Ray w/ mirror self-shrink, Remote Pipebombs, Laser Tripmines), Blue/Red Keycard Doors, 4 Checkpoints, 3 Secrets, Broadcast Relays, Octabrains, Turrets, 3-Phase Battle-Mech Boss, full 4-beat `Neon Strip '96`, and expanded vaulted Grand Festive Hall with cycling 1999 SANTA-BIOS logs.
- [x] 3. Upgrade `src/render/gl_renderer.h/.c` with per-chunk GPU buffers, procedural `512×64` tiled retro pixel texture atlas, expanded sprite atlas, multi-part weapon viewmodels + boot kick + hitmarker, and live tactical minimap / objective HUD.
- [x] 4. Update `src/platform/sdl_main.c`, `src/audio/retro_audio.c`, and `web/index.html` for 5 weapons, `--day=N` CLI/URL launch, touch weapon/gadget controls, richer SFX, and smooth mobile play.
- [x] 5. Expand unit tests (`tests/unit/test_core.c`) and automated 4-beat Day 1 playthrough bot (`tests/game/test_game_playthrough.c`) maintaining ≥95% line coverage (`gcovr`, currently 98.6%) under ASan/UBSan.
- [x] 6. Visually verify Hub & Day 1 (Boulevard, Cinema, Rooftops, Mobile Portrait + Desktop) in headless Chrome with zero WebGL errors, archive OpenSpec change, push to `main`, and deploy to `gh-pages`.
