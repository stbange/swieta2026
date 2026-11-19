# retro-renderer Delta Specification

## ADDED Requirements

### Requirement: Normal-Mapped 64x64 Material Atlas, Procedural Skyboxes, and Sculpted 3D Assets
The renderer SHALL synthesize a `1024×192` (`64×64` per tile) diffuse atlas and matching `1024×192` tangent-space normal/gloss map for per-pixel bump and specular shading with 3×3 PCF soft shadows, render per-biome procedural atmospheric skyboxes (`prog_sky`), render pre-baked cylindrical/spherical/conical 3D meshes for weapons, bosses, barrels, boulders, and festive props, and render `64×64` 2-frame animated dark-outlined character sprites.

#### Scenario: Rendering normal-mapped worlds, biome skyboxes, and sculpted 3D assets cleanly in WebGL2
- **WHEN** the renderer draws the Grand Festive Hall or any Act I realm (Days 1–6) at `640×360` default internal resolution on desktop or mobile portrait WebGL2
- **THEN** normal-mapped surfaces, biome skyboxes, articulated 3D bosses, cylindrical/beveled weapons, animated `64×64` sprites, and atmospheric particles render with zero OpenGL/WebGL2 error flags
