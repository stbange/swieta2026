# retro-renderer Specification

## Purpose
Governs the low-resolution OpenGL 3.3 Core / OpenGL ES 3.0 (WebGL2) rendering pipeline combining 3D meshes, lit directional sprites, editable voxels, dynamic point lights, and real-time shadow mapping.

## Requirements

### Requirement: Hybrid Retro 3D Primitive Rendering
The renderer SHALL draw low-poly 3D meshes, camera-facing directional sprites, and chunked voxel grids into a shared depth buffer at a configurable low internal resolution (`320×240` or `480×270`) with nearest-neighbor upscaling.

#### Scenario: Voxel chunk face culling and runtime destruction
- **WHEN** a `16×16×16` voxel chunk has solid interior blocks and an adjacent block is destroyed
- **THEN** only exposed faces are emitted as mesh quads and the newly exposed interior faces become visible on rebuild

### Requirement: Dynamic Lighting and Real-Time Shadows
The renderer SHALL illuminate meshes, sprites, and voxels using dynamic colored point lights and a real-time directional shadow map while maintaining distance fog.

#### Scenario: Dynamic light and shadow pass execution
- **WHEN** a frame renders with active dynamic point lights and a primary shadow-casting light
- **THEN** the shadow depth map is updated before the low-resolution scene pass and modulates direct lighting across world geometry

### Requirement: Chunked Multi-Zone Voxel Meshing and Textured Surfaces
The renderer SHALL partition worlds up to `128×24×128` into `16×16×16` dirty-tracked voxel chunks and map each voxel material to a distinct tiled retro pixel texture while preserving baked ambient occlusion and real-time shadow mapping.

#### Scenario: Localized chunk dirtying on runtime destruction
- **WHEN** a cracked wall or breakable glass voxel inside one `16×16×16` chunk is destroyed
- **THEN** only the affected chunk (and adjacent boundary chunks if on a seam) is marked dirty for remeshing without rebuilding untouched chunks

### Requirement: Minimap and Combat Feedback HUD
The renderer SHALL display a compact tactical minimap indicating player orientation and active objective direction alongside combat hitmarkers and first-person weapon viewmodels.

#### Scenario: Hitmarker feedback on enemy impact
- **WHEN** a player hits an enemy with a hitscan weapon or projectile
- **THEN** the player hitmarker timer activates for visual confirmation
