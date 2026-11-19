# Proposal: Visual Overhaul — High-Detail Textured, Normal-Mapped, Sculpted Retro-3D Renderer

## Why
User playtest feedback highlighted that `32×32` flat textures, `16×16` boxy sprites, cube-only weapon/boss models, flat solid-color skies, and a `400×240` default framebuffer look too blocky and fail to capture the rich, atmospheric spirit of late-90s classics (*Duke Nukem 3D*, *Tomb Raider*, *Counter-Strike*, *Quake*, *Blood*, *Doom*).

## What Changes
- **Higher-Detail Procedural Textures & Real-Time Normal/Specular Mapping (`src/render/gl_renderer.h/.c`):** Upgrade the tiled voxel atlas from `32×32` to `64×64` (`1024×192`, `16×3` tiles) with multi-octave painterly surface detail, plus a matching `1024×192` tangent-space normal & gloss map (`GL_TEXTURE2`) driving per-pixel bump shading, Blinn-Phong specular highlights, hemisphere ambient bounce, soft 3×3 PCF shadows, and filmic tone mapping.
- **Procedural Atmospheric Skybox Shader (`prog_sky`):** Render biome-specific skies behind every realm (neon starry city skyline, Andean sunlit peaks, sunlit `de_dust` desert clouds, scrolling Quake storm clouds, blood-moon horror mist, Doom fiery brimstone mountains).
- **Sculpted 3D Mesh Library & Particle Atmosphere:** Replace single-cube props, weapons, and bosses with pre-baked GPU meshes (12-sided cylinders, 20-sided faceted spheres, tapered cones, beveled parts) for articulated 3D bosses (T-Rex, KRAMPUS-MECH, Shambler, Gargoyle Matriarch, Cyber-Titan), ribbed cylindrical barrels, craggy boulders, multi-sphere smoke clouds, detailed cylindrical/beveled weapons with gloved hands & starburst muzzle flashes, festive tree ornaments, and drifting snow/ember/ash particles.
- **High-Detail `64×64` 2-Frame Animated Character Sprites (`1024×128` atlas):** Replace `16×16` rectangles with 16 richly shaded, dark-outlined, 2-frame animated classic enemy & pickup sprites authentic to each game.
- **Crisper Default Resolution (`640×360` + `320×240` / `480×270` / `640×360` / `960×540` presets):** Preserve retro character while making textures, sprites, and weapons dramatically sharper on desktop and mobile.
