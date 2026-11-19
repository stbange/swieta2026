## ADDED Requirements

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
