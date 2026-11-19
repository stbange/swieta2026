# Proposal: Phase 1 Vertical Slice — Grand Festive Hall, Day 1 Portal, Retro 3D Renderer, Multiplayer, Touch & WebXR

## Why
Establish the complete playable vertical slice of **Święta 2026: The Grand Hall of 24 Portals** in C11/SDL3/OpenGL (desktop + Emscripten WebGL2 + WebXR), proving every architectural pillar early and deploying a live playtest build to `gh-pages`.

## What Changes
- **Advent Hub (`advent-hub`):** First-person 3D Grand Festive Hall with voxel fireplace, 24-ornament Christmas tree, SANTA-BIOS terminal, 24 dated portals (`2026-12-01`–`2026-12-24`), and playtest unlock-all mode.
- **Retro 3D Renderer (`retro-renderer`):** Low-resolution framebuffer (`320×240` / `480×270`) combining low-poly 3D meshes, lit billboard sprites, and chunked destructible voxels with real-time dynamic lighting and shadow mapping.
- **Day 1 Portal (`level-day01-duke`):** Playable *Duke Nukem 3D* homage (*Neon Strip '96*) with Shrink-Ray + Boot Stomp, Remote Pipebombs blasting cracked voxel walls, Jetpack + Holo-Decoy, Yule Cinema projector switch, Rooftop Battle-Mech boss, and Golden Shades Memory Shard.
- **Multiplayer (`multiplayer-net`):** Local 2–4 player split-screen plus Web-first host-authoritative WebRTC DataChannel networking (signaling relay room codes + zero-server copy-paste SDP fallback).
- **Cross-Platform Input & VR (`input-touch-xr`):** Keyboard/Mouse, Gamepad, automatic adaptive Mobile Touch Controls, and WebXR stereoscopic VR rendering.

## Capabilities

### New Capabilities
- `advent-hub`: Grand Festive Hall navigation, 24-day calendar gating, playtest unlock-all mode, and Memory Shard tree progression.
- `retro-renderer`: Low-res dynamic lighting, real-time shadow mapping, 3D meshes, lit directional sprites, and greedy-meshed editable voxels.
- `level-day01-duke`: Day 1 Neon Strip '96 homage level with Shrink-Ray+Stomp, Remote Pipebombs, Jetpack/Holo-Decoy, boss fight, and shard extraction.
- `multiplayer-net`: 2–4 player local split-screen and WebRTC DataChannel peer-to-peer multiplayer with deterministic binary snapshots.
- `input-touch-xr`: Adaptive mobile touch controls and WebXR stereo session support.

### Modified Capabilities
*(None — greenfield project)*

## Impact
- Creates core C11 simulation, renderer, netcode, input, and asset generator pipeline.
- Enforces ≥95% unit test line coverage and deterministic headless game playthrough tests in CI.
- Publishes automated Web/WebXR playtest builds from `main` to `gh-pages`.
