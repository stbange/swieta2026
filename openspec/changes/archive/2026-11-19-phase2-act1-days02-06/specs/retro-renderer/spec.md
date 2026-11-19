# retro-renderer Delta Specification

## ADDED Requirements

### Requirement: Expanded Multi-Biome Retro Texture Atlas and Act I Entity Visuals
The renderer SHALL provide a `512×96` (`16×3`) tiled pixel-art voxel texture atlas covering all 42 retro materials, render distinct first-person weapon viewmodels and 3D boss silhouettes for Act I, and display tactical screen overlays (Sniper Scope, Quad Damage blue glow, Berserk red pulse, Flashbang whiteout) plus active utility and hazard props.

#### Scenario: Rendering Act I biome textures and tactical overlays without WebGL2 errors
- **WHEN** the renderer draws any Act I scene (Days 2–6) with active flares, smoke screens, explosive barrels, push-blocks, or tactical HUD overlays
- **THEN** all chunked biome textures and entity overlays render cleanly on both desktop OpenGL 3.3 and mobile WebGL2 with zero GL error flags
