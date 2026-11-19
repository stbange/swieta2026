#ifndef SWIETA_VOXEL_MESHER_H
#define SWIETA_VOXEL_MESHER_H

#include "core/math3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VOX_CHUNK_SIZE 16
#define VOX_CHUNKS_X 8
#define VOX_CHUNKS_Y 2
#define VOX_CHUNKS_Z 8
#define VOX_CHUNK_COUNT (VOX_CHUNKS_X * VOX_CHUNKS_Y * VOX_CHUNKS_Z)

#define VOX_WORLD_X (VOX_CHUNKS_X * VOX_CHUNK_SIZE) /* 128 */
#define VOX_WORLD_Y 24
#define VOX_WORLD_Z (VOX_CHUNKS_Z * VOX_CHUNK_SIZE) /* 128 */
#define VOX_TOTAL_CELLS (VOX_WORLD_X * VOX_WORLD_Y * VOX_WORLD_Z)

#define VOX_ATLAS_COLS 16
#define VOX_ATLAS_ROWS 2

typedef enum {
    VOX_AIR = 0,
    VOX_STONE = 1,
    VOX_WOOD = 2,
    VOX_SNOW = 3,
    VOX_BRICK = 4,
    VOX_CRACKED = 5,         /* Destructible by Remote Pipebomb */
    VOX_NEON_PINK = 6,
    VOX_NEON_CYAN = 7,
    VOX_GOLD = 8,
    VOX_LEAVES = 9,
    VOX_FIRE = 10,
    VOX_ASPHALT = 11,
    VOX_METAL = 12,
    VOX_PORTAL_FRAME = 13,
    VOX_CARPET = 14,
    VOX_CINEMA_STATIC = 15,  /* Turns golden when projector restored */
    VOX_CINEMA_LIT = 16,
    VOX_GLASS = 17,          /* Breakable storefront / skylight glass */
    VOX_MIRROR = 18,         /* Reflects Shrink-Ray beam back onto player! */
    VOX_VENT = 19,           /* Air-duct metallic slat lining */
    VOX_HAZARD = 20,         /* Yellow/black caution stripes */
    VOX_CONCRETE = 21,       /* Rooftop concrete slabs */
    VOX_ROAD_STRIPE = 22,    /* Asphalt with dashed yellow center line */
    VOX_DOOR_BLUE = 23,      /* Locked Blue Keycard door */
    VOX_DOOR_RED = 24,       /* Locked Red Keycard door */
    VOX_DOOR_LOCKED = 25,    /* Switch/Relay locked security gate */
    VOX_SEAT = 26,           /* Red velvet theater seats */
    VOX_GIFT = 27,           /* Festive gift boxes / props */
    VOX_RELAY_CORE = 28,     /* Destructible KRAMPUS broadcast relay */
    VOX_TYPE_COUNT = 29
} VoxelType;

typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
    float color[4]; /* tint + baked AO */
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
    bool chunk_dirty[VOX_CHUNK_COUNT];
    bool dirty;
} VoxelWorld;

typedef struct {
    bool hit;
    float dist;
    Vec3 point;
    Vec3 normal;
    int vx, vy, vz;
    uint8_t type;
} VoxelRayHit;

int voxel_chunk_index(int cx, int cy, int cz);
void voxel_chunk_coords(int chunk_idx, int *out_cx, int *out_cy, int *out_cz);

void voxel_world_clear(VoxelWorld *vw);
bool voxel_in_bounds(int x, int y, int z);
uint8_t voxel_get(const VoxelWorld *vw, int x, int y, int z);
void voxel_set(VoxelWorld *vw, int x, int y, int z, uint8_t type);
void voxel_fill_box(VoxelWorld *vw, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type);
bool voxel_is_solid(const VoxelWorld *vw, int x, int y, int z);
bool voxel_is_opaque(uint8_t type);

/* Destruction & dynamic level mutations */
int voxel_explode_cracked(VoxelWorld *vw, Vec3 center, float radius);
int voxel_break_glass(VoxelWorld *vw, Vec3 center, float radius);
int voxel_replace_all(VoxelWorld *vw, uint8_t from_type, uint8_t to_type);

/* Raycasting (legacy wrapper + rich hit with normal/type/coords) */
bool voxel_raycast_ext(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, VoxelRayHit *out_hit);
bool voxel_raycast(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, float *out_dist, Vec3 *out_hit);

/* Chunked & full mesh building */
void voxel_mesh_init(VoxelMesh *mesh);
void voxel_mesh_free(VoxelMesh *mesh);
bool voxel_build_chunk_mesh(const VoxelWorld *vw, int chunk_idx, VoxelMesh *out_mesh);
bool voxel_build_mesh(const VoxelWorld *vw, VoxelMesh *out_mesh);

void voxel_type_color(uint8_t type, float out_rgba[4], float *out_emissive);
int voxel_type_atlas_tile(uint8_t type);

#endif /* SWIETA_VOXEL_MESHER_H */
