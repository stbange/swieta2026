# Design: Phase 2 — Act I Shared Mechanics & Modular Level Builders

## Modular C11 Simulation Split
- `src/sim/world.h`: Public data structures (`GameWorld`, `Player`, `Enemy`, `WeaponId`, `PushBlock`, `SwitchTrigger`, `Barrel`, `Teleporter`, `JumpPad`, `BoulderTrap`, `FlareLight`).
- `src/sim/world_internal.h`: Internal builder & FX helper API (`world_clear_entities`, `world_add_enemy`, `world_add_pickup`, `world_add_door`, `world_open_door`, `world_add_secret`, `world_add_pushblock`, `world_add_switch`, `world_add_barrel`, `world_add_teleporter`, `world_add_jumppad`, `world_add_boulder`, `world_set_banner`, `world_set_objective`, `world_add_light`, `world_spawn_explosion`, `world_reset_player_for_spawn`, `world_damage_enemy`).
- `src/sim/levels_act1.h/.c`: Self-contained 4-beat builders, per-day `USE` / switch handlers, per-day tick hooks (boulder triggers, C4 defusal progress, infighting/water-discharge rules), and dynamic lighting setups for Days 2–6.
- `src/sim/world.c`: Hub, Day 1, Preview Chamber, deterministic 60 Hz player physics (including Ledge Mantle, Swimming, Rocket-Jump knockback, Strafe Air-Control, and Lava/Slime hazards), unified 5-slot weapon dispatch (`w->level_weapons[5]`), projectile/explosion/barrel/burn-DoT/infighting simulation, and network snapshots.

## Controls Consistency Across All 6 Days
- Keys `1`–`5` and `Tab` / Mobile `WEAP` always select from the current level's 5-slot loadout (`w->level_weapons[0..4]`) skipping unowned slots (`player.owned_weapons` bitmask).
- `LMB` / `FIRE`: Primary weapon attack.
- `RMB` / `ALT`: Context-authentic Alt-Fire per weapon (Day 1 Pipebomb detonate; Day 2 Acrobatic side-flip dodge; Day 3 Sniper Zoom / Smoke Grenade; Day 5 Flare Starburst / Molotov / Proximity TNT / Voodoo Curse).
- `E` / `USE`: Interact with switches, push-blocks, CS Buy-Zone crates, C4 defusal, keycard doors, and shard exit shrines.
- `Space` / `JUMP`: Jump, swim upward in water, auto-mantle climbable ledges, or Jetpack on Day 1.

## Rendering & Procedural Retro Textures
- Expand voxel atlas to `512×96` (`16×3 = 48` `32×32` nearest-neighbor pixel-art tiles) adding `VOX_SANDSTONE`, `VOX_WATER`, `VOX_LAVA`, `VOX_SLIME`, `VOX_RUST_BRASS`, `VOX_CRATE`, `VOX_DOOR_YELLOW`, `VOX_RED_CARNIVAL`, `VOX_CRYPT_STONE`, `VOX_TECH_WALL`, `VOX_PUSH_BLOCK`, `VOX_SPIKES`, and `VOX_SLIPGATE`.
- Render distinct 3D voxel boss archetypes (T-Rex, Shambler, Gargoyle Matriarch, Cyber-Titan), explosive barrels, rolling boulders, push-blocks, green flares, billowing smoke screens, Thunderbolt lightning beam, and full-screen tactical overlays (Sniper Scope, Quad blue glow, Berserk crimson pulse, Flashbang whiteout).
