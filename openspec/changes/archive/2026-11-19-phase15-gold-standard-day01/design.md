# Design: Phase 1.5 — Chunked Textured Engine & Full 20-Minute Day 1

## Chunked Voxel Architecture (`128 × 24 × 128`)
- World size: `VOX_WORLD_X = 128`, `VOX_WORLD_Y = 24`, `VOX_WORLD_Z = 128`, split into `8 × 2 × 8 = 128` chunks (`16 × 16 × 16` cells each).
- Each chunk stores its own `chunk_dirty[128]` bit; `voxel_set` dirties the owning chunk plus any face-adjacent neighbor chunk when a cell lies on a `0` or `15` chunk boundary.
- `VoxelVertex` carries atlas UV coordinates mapped directly into a `16 × 2` tile grid (`512 × 64` procedural CC BY 4.0 retro pixel texture atlas), eliminating flat untextured faces while keeping a single draw call per non-empty chunk!

## Day 1 Four-Beat World Blueprint (`Neon Strip '96`)
- **Beat 1 (South-West `x=6..54, z=78..122`):** Snowy Neon Boulevard, Pixel Pawnshop (breakable glass → Shotgun), Subway Underpass, Alley Security Kiosk (**Blue Keycard** + Checkpoint 1).
- **Beat 2 (West-Center `x=10..56, z=34..74`):** Blue Keycard Door → Yule Cinema Lobby & Auditorium (Octabrain balcony), Cracked Wall → **Secret #1 Retro Arcade**, Mirror Room (`VOX_MIRROR`) → self-shrink reflection → 1-voxel miniature vent → locked Projection Booth → **Cinema Projector Switch** (opens Freight Ramp + Checkpoint 2).
- **Beat 3 (East Skyline `x=60..118, z=36..114`):** Freight Ramp → Roof A (`y=8`, Jetpack + Laser Tripmines), Jetpack gap → Roof B (`y=10`, **Secret #2** breakable skylight + Broadcast Relay #1 + Turret), Jetpack gap → Roof C (`y=12`, **Secret #3** vent cache + Broadcast Relay #2 + Turret + **Red Keycard** + Checkpoint 3 + Skybridge unlock).
- **Beat 4 (North Penthouse `x=22..96, z=6..30`, `y=12`):** Red Keycard Gate → Checkpoint 4 → **KRAMPUS-MECH MK.IV** (3-phase boss with destructible cover pillars & Phase 2 Trooper wave) → crimson **Nuke-Exit Button** → **Memory Shard #1: Golden Shades** → return to Grand Festive Hall.

## Dynamic Height & Mirror Shrink Physics
- `player_height_for(p)` returns `0.62f` (eye `0.48f`) while `p->self_shrunk_timer > 0.0f`, and `1.72f` (eye `1.52f`) normally.
- Standing up at the end of `self_shrunk_timer` checks ceiling clearance (`!player_collides_height(..., 1.72f)`) and safely extends the shrink timer slightly until the player exits the low 1-voxel duct so they can never get wedged inside a vent!
