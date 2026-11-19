#ifndef SWIETA_VOXEL_MESHER_H
#define SWIETA_VOXEL_MESHER_H

#include "core/math3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOX_WORLD_X 48
#define VOX_WORLD_Y 16
#define VOX_WORLD_Z 48
#define VOX_TOTAL_CELLS (VOX_WORLD_X * VOX_WORLD_Y * VOX_WORLD_Z)

typedef enum {
    VOX_AIR = 0,
    VOX_STONE = 1,
    VOX_WOOD = 2,
    VOX_SNOW = 3,
    VOX_BRICK = 4,
    VOX_CRACKED = 5,        /* Destructible by Remote Pipebomb */
    VOX_NEON_PINK = 6,
    VOX_NEON_CYAN = 7,
    VOX_GOLD = 8,
    VOX_LEAVES = 9,
    VOX_FIRE = 10,
    VOX_ASPHALT = 11,
    VOX_METAL = 12,
    VOX_PORTAL_FRAME = 13,
    VOX_CARPET = 14,
    VOX_CINEMA_STATIC = 15, /* Turns golden when projector restored */
    VOX_CINEMA_LIT = 16
} VoxelType;

typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
    float color[4]; /* rgba + baked AO in rgb */
    float emissive;
} VoxelVertex;

typedef struct {
    VoxelVertex *vertices;
    uint32_t *indices;
    size_t vertex_count;
    size_t vertex_cap;
    size_t index_count;
    size_t index_cap;
} VoxelMesh;

typedef struct {
    uint8_t cells[VOX_TOTAL_CELLS];
    bool dirty;
} VoxelWorld;

void voxel_world_clear(VoxelWorld *vw);
bool voxel_in_bounds(int x, int y, int z);
uint8_t voxel_get(const VoxelWorld *vw, int x, int y, int z);
void voxel_set(VoxelWorld *vw, int x, int y, int z, uint8_t type);
void voxel_fill_box(VoxelWorld *vw, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type);
bool voxel_is_solid(const VoxelWorld *vw, int x, int y, int z);

/* Destroys VOX_CRACKED cells within radius; returns number destroyed */
int voxel_explode_cracked(VoxelWorld *vw, Vec3 center, float radius);

/* Replaces all occurrences of from_type with to_type */
int voxel_replace_all(VoxelWorld *vw, uint8_t from_type, uint8_t to_type);

/* Simple DDA raycast against solid voxels */
bool voxel_raycast(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, float *out_dist, Vec3 *out_hit);

void voxel_mesh_init(VoxelMesh *mesh);
void voxel_mesh_free(VoxelMesh *mesh);
bool voxel_build_mesh(const VoxelWorld *vw, VoxelMesh *out_mesh);

void voxel_type_color(uint8_t type, float out_rgba[4], float *out_emissive);

#endif /* SWIETA_VOXEL_MESHER_H */
