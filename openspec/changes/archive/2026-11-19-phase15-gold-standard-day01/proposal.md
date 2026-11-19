# Proposal: Phase 1.5 — Chunked Textured Retro Engine & Full 20-Minute Day 1 (*Neon Strip '96*)

## Why
The Phase 1 vertical slice proved the C11/SDL3/OpenGL/Emscripten/WebXR/WebRTC pipeline, but its single `48×16×48` untextured arena only provided ~1–2 minutes of Day 1 gameplay. Before implementing Days 2–24, the engine must reach the real gold-standard quality bar: chunked multi-zone maps, richly tiled retro pixel textures, keycard doors, checkpoints, secrets, mirror self-shrink puzzles, Laser Tripmines, Shotgun, Octabrains, Turrets, and a genuine 4-beat ~20-minute Day 1 homage.

## What Changes
- **Chunked World & Textured Renderer (`retro-renderer`):** Upgrade voxel worlds to `128×24×128` (`16³` dirty-tracked chunks) rendered with a 32-material tiled `32×32` retro pixel-art texture atlas, animated multi-archetype sprites, detailed multi-part weapon viewmodels with walk-bob/hitmarkers, and a live top-left tactical minimap/objective compass.
- **Full 4-Beat Day 1 (`level-day01-duke`):** Rebuild *Neon Strip '96* across 4 interconnected city zones (Snowy Neon Boulevard, Yule Cinema + Mirror Self-Shrink Vent Puzzle + Secret Retro Arcade, 3-Rooftop Broadcast Skyline with Laser Tripmines & Turrets, and Multi-Phase Penthouse Boss Arena) with Blue & Red Keycards, 4 Checkpoints, 3 Secrets, 5 Weapons (Pistol, Shotgun, Shrink-Ray, Pipebombs, Laser Tripmines), Jetpack, and Holo-Decoy.
- **Expanded Grand Festive Hall (`advent-hub`):** Upgrade the Hub into a vaulted `128×24×128` winter cathedral with textured timber/brick/stained-glass architecture, 24 individual tree ornament lights, and cycling 1999 LAN-party SANTA-BIOS logs.

## Capabilities

### New Capabilities
*(None)*

### Modified Capabilities
- `retro-renderer`: Chunked `128×24×128` dirty-tracked voxel meshing, tiled retro pixel-art texture atlas, breakable glass/mirror materials, multi-part weapon viewmodels, and minimap HUD.
- `level-day01-duke`: Full 4-beat ~20-minute level progression with Shotgun, Mirror self-shrink vent puzzle, Laser Tripmines, Blue/Red Keycard doors, Checkpoints, Secrets, Octabrains/Turrets, and 3-phase Battle-Mech boss.
- `advent-hub`: Expanded vaulted cathedral layout with 24 individual shard tree lights and cycling 1999 SANTA-BIOS LAN logs.

## Impact
- Establishes the scalable chunked world, trigger/door/checkpoint, and textured rendering foundation reused by Days 2–24.
- Keeps ≥95% unit test coverage and full deterministic headless 4-beat automated playthrough verification.
