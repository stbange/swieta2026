#include "render/voxel_mesher.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static inline size_t vox_idx(int x, int y, int z) {
    return (size_t)((y * VOX_WORLD_Z + z) * VOX_WORLD_X + x);
}

int voxel_chunk_index(int cx, int cy, int cz) {
    if (cx < 0 || cx >= VOX_CHUNKS_X || cy < 0 || cy >= VOX_CHUNKS_Y || cz < 0 || cz >= VOX_CHUNKS_Z) {
        return -1;
    }
    return (cy * VOX_CHUNKS_Z + cz) * VOX_CHUNKS_X + cx;
}

void voxel_chunk_coords(int chunk_idx, int *out_cx, int *out_cy, int *out_cz) {
    int cx = chunk_idx % VOX_CHUNKS_X;
    int cz = (chunk_idx / VOX_CHUNKS_X) % VOX_CHUNKS_Z;
    int cy = chunk_idx / (VOX_CHUNKS_X * VOX_CHUNKS_Z);
    if (out_cx) *out_cx = cx;
    if (out_cy) *out_cy = cy;
    if (out_cz) *out_cz = cz;
}

static inline void mark_chunk_dirty_if_valid(VoxelWorld *vw, int cx, int cy, int cz) {
    int idx = voxel_chunk_index(cx, cy, cz);
    if (idx >= 0 && idx < VOX_CHUNK_COUNT) {
        vw->chunk_dirty[idx] = true;
    }
}

static void mark_chunk_dirty_at(VoxelWorld *vw, int x, int y, int z) {
    int cx = x / VOX_CHUNK_SIZE;
    int cy = y / VOX_CHUNK_SIZE;
    int cz = z / VOX_CHUNK_SIZE;
    mark_chunk_dirty_if_valid(vw, cx, cy, cz);

    /* Also dirty neighbor chunks when editing a seam cell so face culling & AO update */
    int lx = x % VOX_CHUNK_SIZE;
    int ly = y % VOX_CHUNK_SIZE;
    int lz = z % VOX_CHUNK_SIZE;
    if (lx == 0)                  mark_chunk_dirty_if_valid(vw, cx - 1, cy, cz);
    if (lx == VOX_CHUNK_SIZE - 1) mark_chunk_dirty_if_valid(vw, cx + 1, cy, cz);
    if (ly == 0)                  mark_chunk_dirty_if_valid(vw, cx, cy - 1, cz);
    if (ly == VOX_CHUNK_SIZE - 1) mark_chunk_dirty_if_valid(vw, cx, cy + 1, cz);
    if (lz == 0)                  mark_chunk_dirty_if_valid(vw, cx, cy, cz - 1);
    if (lz == VOX_CHUNK_SIZE - 1) mark_chunk_dirty_if_valid(vw, cx, cy, cz + 1);
    vw->dirty = true;
}

void voxel_world_clear(VoxelWorld *vw) {
    if (!vw) return;
    memset(vw->cells, VOX_AIR, sizeof(vw->cells));
    for (int i = 0; i < VOX_CHUNK_COUNT; ++i) {
        vw->chunk_dirty[i] = true;
    }
    vw->dirty = true;
}

bool voxel_in_bounds(int x, int y, int z) {
    return x >= 0 && x < VOX_WORLD_X &&
           y >= 0 && y < VOX_WORLD_Y &&
           z >= 0 && z < VOX_WORLD_Z;
}

uint8_t voxel_get(const VoxelWorld *vw, int x, int y, int z) {
    if (!vw || !voxel_in_bounds(x, y, z)) {
        return VOX_AIR;
    }
    return vw->cells[vox_idx(x, y, z)];
}

void voxel_set(VoxelWorld *vw, int x, int y, int z, uint8_t type) {
    if (!vw || !voxel_in_bounds(x, y, z)) {
        return;
    }
    size_t idx = vox_idx(x, y, z);
    if (vw->cells[idx] != type) {
        vw->cells[idx] = type;
        mark_chunk_dirty_at(vw, x, y, z);
    }
}

void voxel_fill_box(VoxelWorld *vw, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type) {
    if (!vw) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                voxel_set(vw, x, y, z, type);
            }
        }
    }
}

bool voxel_is_solid(const VoxelWorld *vw, int x, int y, int z) {
    uint8_t t = voxel_get(vw, x, y, z);
    return t != VOX_AIR && t != VOX_FIRE && t != VOX_WATER &&
           t != VOX_LAVA && t != VOX_SLIME && t != VOX_SPIKES;
}

bool voxel_is_opaque(uint8_t type) {
    return type != VOX_AIR && type != VOX_FIRE && type != VOX_GLASS &&
           type != VOX_WATER && type != VOX_SPIKES;
}

static int clear_matching_radius(VoxelWorld *vw, Vec3 center, float radius, uint8_t target_type) {
    if (!vw || radius <= 0.0f) return 0;
    int destroyed = 0;
    int x0 = (int)floorf(center.x - radius);
    int x1 = (int)ceilf(center.x + radius);
    int y0 = (int)floorf(center.y - radius);
    int y1 = (int)ceilf(center.y + radius);
    int z0 = (int)floorf(center.z - radius);
    int z1 = (int)ceilf(center.z + radius);
    float r2 = radius * radius;

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (voxel_get(vw, x, y, z) == target_type) {
                    Vec3 c = vec3_make((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f);
                    if (vec3_len_sq(vec3_sub(c, center)) <= r2) {
                        voxel_set(vw, x, y, z, VOX_AIR);
                        ++destroyed;
                    }
                }
            }
        }
    }
    return destroyed;
}

int voxel_explode_cracked(VoxelWorld *vw, Vec3 center, float radius) {
    int n = clear_matching_radius(vw, center, radius, VOX_CRACKED);
    n += clear_matching_radius(vw, center, radius, VOX_GLASS);
    return n;
}

int voxel_break_glass(VoxelWorld *vw, Vec3 center, float radius) {
    return clear_matching_radius(vw, center, radius, VOX_GLASS);
}

int voxel_replace_all(VoxelWorld *vw, uint8_t from_type, uint8_t to_type) {
    if (!vw || from_type == to_type) return 0;
    int replaced = 0;
    for (int y = 0; y < VOX_WORLD_Y; ++y) {
        for (int z = 0; z < VOX_WORLD_Z; ++z) {
            for (int x = 0; x < VOX_WORLD_X; ++x) {
                size_t idx = vox_idx(x, y, z);
                if (vw->cells[idx] == from_type) {
                    vw->cells[idx] = to_type;
                    mark_chunk_dirty_at(vw, x, y, z);
                    ++replaced;
                }
            }
        }
    }
    return replaced;
}

bool voxel_raycast_ext(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, VoxelRayHit *out_hit) {
    if (out_hit) memset(out_hit, 0, sizeof(*out_hit));
    if (!vw || max_dist <= 0.0f) return false;
    Vec3 d = vec3_norm(dir);
    if (vec3_len_sq(d) < 1e-6f) return false;

    const float step = 0.08f;
    int prev_x = (int)floorf(origin.x);
    int prev_y = (int)floorf(origin.y);
    int prev_z = (int)floorf(origin.z);

    for (float t = 0.0f; t <= max_dist; t += step) {
        Vec3 p = vec3_add(origin, vec3_scale(d, t));
        int vx = (int)floorf(p.x);
        int vy = (int)floorf(p.y);
        int vz = (int)floorf(p.z);
        if (voxel_is_solid(vw, vx, vy, vz)) {
            if (out_hit) {
                out_hit->hit = true;
                out_hit->dist = t;
                out_hit->point = p;
                out_hit->vx = vx;
                out_hit->vy = vy;
                out_hit->vz = vz;
                out_hit->type = voxel_get(vw, vx, vy, vz);
                Vec3 n = vec3_make((float)(prev_x - vx), (float)(prev_y - vy), (float)(prev_z - vz));
                if (vec3_len_sq(n) < 0.5f) {
                    n = vec3_scale(d, -1.0f);
                }
                out_hit->normal = vec3_norm(n);
            }
            return true;
        }
        prev_x = vx;
        prev_y = vy;
        prev_z = vz;
    }
    return false;
}

bool voxel_raycast(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, float *out_dist, Vec3 *out_hit) {
    VoxelRayHit h;
    if (!voxel_raycast_ext(vw, origin, dir, max_dist, &h)) {
        return false;
    }
    if (out_dist) *out_dist = h.dist;
    if (out_hit) *out_hit = h.point;
    return true;
}

int voxel_type_atlas_tile(uint8_t type) {
    if (type < VOX_TYPE_COUNT) {
        return (int)type;
    }
    return 1;
}

void voxel_type_color(uint8_t type, float out_rgba[4], float *out_emissive) {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f, e = 0.0f;
    switch (type) {
        case VOX_STONE:        r = 0.88f; g = 0.90f; b = 0.94f; break;
        case VOX_WOOD:         r = 0.95f; g = 0.88f; b = 0.80f; break;
        case VOX_SNOW:         r = 0.96f; g = 0.98f; b = 1.00f; break;
        case VOX_BRICK:        r = 0.96f; g = 0.86f; b = 0.84f; break;
        case VOX_CRACKED:      r = 1.00f; g = 0.85f; b = 0.72f; e = 0.22f; break;
        case VOX_NEON_PINK:    r = 1.00f; g = 0.35f; b = 0.82f; e = 1.05f; break;
        case VOX_NEON_CYAN:    r = 0.28f; g = 0.96f; b = 1.00f; e = 1.05f; break;
        case VOX_GOLD:         r = 1.00f; g = 0.90f; b = 0.38f; e = 0.62f; break;
        case VOX_LEAVES:       r = 0.85f; g = 0.95f; b = 0.86f; break;
        case VOX_FIRE:         r = 1.00f; g = 0.65f; b = 0.20f; e = 1.15f; break;
        case VOX_ASPHALT:      r = 0.82f; g = 0.84f; b = 0.88f; break;
        case VOX_METAL:        r = 0.88f; g = 0.92f; b = 0.96f; break;
        case VOX_PORTAL_FRAME: r = 0.86f; g = 0.84f; b = 0.95f; break;
        case VOX_CARPET:       r = 0.96f; g = 0.76f; b = 0.76f; break;
        case VOX_CINEMA_STATIC:r = 0.85f; g = 0.88f; b = 0.92f; e = 0.45f; break;
        case VOX_CINEMA_LIT:   r = 1.00f; g = 0.94f; b = 0.65f; e = 1.05f; break;
        case VOX_GLASS:        r = 0.72f; g = 0.94f; b = 1.00f; a = 0.52f; e = 0.25f; break;
        case VOX_MIRROR:       r = 0.78f; g = 0.96f; b = 1.00f; e = 0.55f; break;
        case VOX_VENT:         r = 0.85f; g = 0.90f; b = 0.94f; break;
        case VOX_HAZARD:       r = 1.00f; g = 0.94f; b = 0.65f; e = 0.12f; break;
        case VOX_CONCRETE:     r = 0.86f; g = 0.88f; b = 0.90f; break;
        case VOX_ROAD_STRIPE:  r = 0.92f; g = 0.92f; b = 0.86f; break;
        case VOX_DOOR_BLUE:    r = 0.45f; g = 0.75f; b = 1.00f; e = 0.40f; break;
        case VOX_DOOR_RED:     r = 1.00f; g = 0.38f; b = 0.38f; e = 0.45f; break;
        case VOX_DOOR_LOCKED:  r = 0.95f; g = 0.72f; b = 0.25f; e = 0.32f; break;
        case VOX_SEAT:         r = 0.92f; g = 0.28f; b = 0.32f; break;
        case VOX_GIFT:         r = 1.00f; g = 0.92f; b = 0.88f; e = 0.15f; break;
        case VOX_RELAY_CORE:   r = 1.00f; g = 0.22f; b = 0.42f; e = 0.95f; break;
        default: break;
    }
    out_rgba[0] = r;
    out_rgba[1] = g;
    out_rgba[2] = b;
    out_rgba[3] = a;
    if (out_emissive) *out_emissive = e;
}

void voxel_mesh_init(VoxelMesh *mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

void voxel_mesh_free(VoxelMesh *mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->indices);
    memset(mesh, 0, sizeof(*mesh));
}

static bool mesh_reserve(VoxelMesh *mesh, size_t add_verts, size_t add_inds) {
    if (mesh->vertex_count + add_verts > mesh->vertex_cap) {
        size_t new_cap = mesh->vertex_cap ? mesh->vertex_cap * 2 : 1024;
        while (new_cap < mesh->vertex_count + add_verts) new_cap *= 2;
        VoxelVertex *nv = (VoxelVertex *)realloc(mesh->vertices, new_cap * sizeof(VoxelVertex));
        if (!nv) return false;
        mesh->vertices = nv;
        mesh->vertex_cap = new_cap;
    }
    if (mesh->index_count + add_inds > mesh->index_cap) {
        size_t new_cap = mesh->index_cap ? mesh->index_cap * 2 : 1536;
        while (new_cap < mesh->index_count + add_inds) new_cap *= 2;
        uint32_t *ni = (uint32_t *)realloc(mesh->indices, new_cap * sizeof(uint32_t));
        if (!ni) return false;
        mesh->indices = ni;
        mesh->index_cap = new_cap;
    }
    return true;
}

typedef struct {
    int dx, dy, dz;
    float corners[4][3];
    float uv[4][2];
} FaceDef;

static const FaceDef kFaces[6] = {
    /* +X */ { 1, 0, 0, {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}, {{0,1},{1,1},{1,0},{0,0}} },
    /* -X */ {-1, 0, 0, {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, {{0,1},{1,1},{1,0},{0,0}} },
    /* +Y */ { 0, 1, 0, {{0,1,1},{1,1,1},{1,1,0},{0,1,0}}, {{0,1},{1,1},{1,0},{0,0}} },
    /* -Y */ { 0,-1, 0, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, {{0,1},{1,1},{1,0},{0,0}} },
    /* +Z */ { 0, 0, 1, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{0,1},{1,1},{1,0},{0,0}} },
    /* -Z */ { 0, 0,-1, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, {{0,1},{1,1},{1,0},{0,0}} }
};

static float compute_vertex_ao(const VoxelWorld *vw, int x, int y, int z, const FaceDef *f, const float c[3]) {
    int nx = x + f->dx;
    int ny = y + f->dy;
    int nz = z + f->dz;
    int sx = (c[0] > 0.5f) ? 1 : -1;
    int sy = (c[1] > 0.5f) ? 1 : -1;
    int sz = (c[2] > 0.5f) ? 1 : -1;

    int occ = 0;
    if (f->dx != 0) {
        if (voxel_is_opaque(voxel_get(vw, nx, ny + sy, nz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx, ny, nz + sz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx, ny + sy, nz + sz))) ++occ;
    } else if (f->dy != 0) {
        if (voxel_is_opaque(voxel_get(vw, nx + sx, ny, nz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx, ny, nz + sz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx + sx, ny, nz + sz))) ++occ;
    } else {
        if (voxel_is_opaque(voxel_get(vw, nx + sx, ny, nz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx, ny + sy, nz))) ++occ;
        if (voxel_is_opaque(voxel_get(vw, nx + sx, ny + sy, nz))) ++occ;
    }
    static const float kAoTable[4] = {1.0f, 0.83f, 0.66f, 0.50f};
    return kAoTable[occ];
}

static bool append_region_to_mesh(const VoxelWorld *vw, int x0, int y0, int z0, int x1, int y1, int z1, VoxelMesh *out_mesh) {
    const float du = 1.0f / (float)VOX_ATLAS_COLS;
    const float dv = 1.0f / (float)VOX_ATLAS_ROWS;
    const float pad_u = du * 0.015f;
    const float pad_v = dv * 0.015f;

    for (int y = y0; y < y1; ++y) {
        for (int z = z0; z < z1; ++z) {
            for (int x = x0; x < x1; ++x) {
                uint8_t type = voxel_get(vw, x, y, z);
                if (type == VOX_AIR) continue;

                float rgba[4];
                float emissive = 0.0f;
                voxel_type_color(type, rgba, &emissive);

                int tile = voxel_type_atlas_tile(type);
                int tx = tile % VOX_ATLAS_COLS;
                int ty = tile / VOX_ATLAS_COLS;
                float u0 = (float)tx * du + pad_u;
                float u1 = (float)(tx + 1) * du - pad_u;
                float v0 = (float)ty * dv + pad_v;
                float v1 = (float)(ty + 1) * dv - pad_v;

                float hash = (float)(((x * 73856093) ^ (y * 19349663) ^ (z * 83492791)) & 15) / 15.0f;
                float var = 0.94f + hash * 0.10f;

                for (int fi = 0; fi < 6; ++fi) {
                    const FaceDef *f = &kFaces[fi];
                    uint8_t neighbor = voxel_get(vw, x + f->dx, y + f->dy, z + f->dz);
                    if (type == VOX_GLASS || type == VOX_WATER) {
                        if (neighbor == type || voxel_is_opaque(neighbor)) continue;
                    } else if (voxel_is_opaque(neighbor)) {
                        continue;
                    }
                    if (!mesh_reserve(out_mesh, 4, 6)) {
                        return false;
                    }

                    uint32_t base = (uint32_t)out_mesh->vertex_count;
                    for (int ci = 0; ci < 4; ++ci) {
                        VoxelVertex v;
                        v.pos[0] = (float)x + f->corners[ci][0];
                        v.pos[1] = (float)y + f->corners[ci][1];
                        v.pos[2] = (float)z + f->corners[ci][2];
                        v.normal[0] = (float)f->dx;
                        v.normal[1] = (float)f->dy;
                        v.normal[2] = (float)f->dz;
                        v.uv[0] = (f->uv[ci][0] > 0.5f) ? u1 : u0;
                        v.uv[1] = (f->uv[ci][1] > 0.5f) ? v1 : v0;
                        float ao = (emissive > 0.5f || type == VOX_GLASS || type == VOX_WATER) ? 1.0f : compute_vertex_ao(vw, x, y, z, f, f->corners[ci]);
                        v.color[0] = sw_clampf(rgba[0] * var * ao, 0.0f, 1.0f);
                        v.color[1] = sw_clampf(rgba[1] * var * ao, 0.0f, 1.0f);
                        v.color[2] = sw_clampf(rgba[2] * var * ao, 0.0f, 1.0f);
                        v.color[3] = rgba[3];
                        v.emissive = emissive;
                        out_mesh->vertices[out_mesh->vertex_count++] = v;
                    }
                    out_mesh->indices[out_mesh->index_count++] = base + 0;
                    out_mesh->indices[out_mesh->index_count++] = base + 1;
                    out_mesh->indices[out_mesh->index_count++] = base + 2;
                    out_mesh->indices[out_mesh->index_count++] = base + 0;
                    out_mesh->indices[out_mesh->index_count++] = base + 2;
                    out_mesh->indices[out_mesh->index_count++] = base + 3;
                }
            }
        }
    }
    return true;
}

bool voxel_build_chunk_mesh(const VoxelWorld *vw, int chunk_idx, VoxelMesh *out_mesh) {
    if (!vw || !out_mesh || chunk_idx < 0 || chunk_idx >= VOX_CHUNK_COUNT) return false;
    out_mesh->vertex_count = 0;
    out_mesh->index_count = 0;
    int cx, cy, cz;
    voxel_chunk_coords(chunk_idx, &cx, &cy, &cz);
    int x0 = cx * VOX_CHUNK_SIZE;
    int y0 = cy * VOX_CHUNK_SIZE;
    int z0 = cz * VOX_CHUNK_SIZE;
    int x1 = x0 + VOX_CHUNK_SIZE;
    int y1 = y0 + VOX_CHUNK_SIZE;
    int z1 = z0 + VOX_CHUNK_SIZE;
    if (y1 > VOX_WORLD_Y) y1 = VOX_WORLD_Y;
    return append_region_to_mesh(vw, x0, y0, z0, x1, y1, z1, out_mesh);
}

bool voxel_build_mesh(const VoxelWorld *vw, VoxelMesh *out_mesh) {
    if (!vw || !out_mesh) return false;
    out_mesh->vertex_count = 0;
    out_mesh->index_count = 0;
    return append_region_to_mesh(vw, 0, 0, 0, VOX_WORLD_X, VOX_WORLD_Y, VOX_WORLD_Z, out_mesh);
}
