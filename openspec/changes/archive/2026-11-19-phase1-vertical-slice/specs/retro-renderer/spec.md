## ADDED Requirements

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
