#include "render/voxel_mesher.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static inline size_t vox_idx(int x, int y, int z) {
    return (size_t)((y * VOX_WORLD_Z + z) * VOX_WORLD_X + x);
}

void voxel_world_clear(VoxelWorld *vw) {
    if (!vw) return;
    memset(vw->cells, VOX_AIR, sizeof(vw->cells));
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
        vw->dirty = true;
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
    return t != VOX_AIR && t != VOX_FIRE;
}

int voxel_explode_cracked(VoxelWorld *vw, Vec3 center, float radius) {
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
                if (voxel_get(vw, x, y, z) == VOX_CRACKED) {
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

int voxel_replace_all(VoxelWorld *vw, uint8_t from_type, uint8_t to_type) {
    if (!vw || from_type == to_type) return 0;
    int replaced = 0;
    for (size_t i = 0; i < VOX_TOTAL_CELLS; ++i) {
        if (vw->cells[i] == from_type) {
            vw->cells[i] = to_type;
            ++replaced;
        }
    }
    if (replaced > 0) {
        vw->dirty = true;
    }
    return replaced;
}

bool voxel_raycast(const VoxelWorld *vw, Vec3 origin, Vec3 dir, float max_dist, float *out_dist, Vec3 *out_hit) {
    if (!vw || max_dist <= 0.0f) return false;
    Vec3 d = vec3_norm(dir);
    if (vec3_len_sq(d) < 1e-6f) return false;

    const float step = 0.125f;
    for (float t = 0.0f; t <= max_dist; t += step) {
        Vec3 p = vec3_add(origin, vec3_scale(d, t));
        int vx = (int)floorf(p.x);
        int vy = (int)floorf(p.y);
        int vz = (int)floorf(p.z);
        if (voxel_is_solid(vw, vx, vy, vz)) {
            if (out_dist) *out_dist = t;
            if (out_hit) *out_hit = p;
            return true;
        }
    }
    return false;
}

void voxel_type_color(uint8_t type, float out_rgba[4], float *out_emissive) {
    float r = 0.6f, g = 0.6f, b = 0.6f, e = 0.0f;
    switch (type) {
        case VOX_STONE:        r = 0.42f; g = 0.44f; b = 0.48f; break;
        case VOX_WOOD:         r = 0.46f; g = 0.27f; b = 0.14f; break;
        case VOX_SNOW:         r = 0.90f; g = 0.94f; b = 0.99f; break;
        case VOX_BRICK:        r = 0.58f; g = 0.22f; b = 0.18f; break;
        case VOX_CRACKED:      r = 0.72f; g = 0.36f; b = 0.22f; e = 0.18f; break;
        case VOX_NEON_PINK:    r = 1.00f; g = 0.20f; b = 0.72f; e = 0.95f; break;
        case VOX_NEON_CYAN:    r = 0.15f; g = 0.92f; b = 1.00f; e = 0.95f; break;
        case VOX_GOLD:         r = 1.00f; g = 0.82f; b = 0.20f; e = 0.55f; break;
        case VOX_LEAVES:       r = 0.12f; g = 0.44f; b = 0.20f; break;
        case VOX_FIRE:         r = 1.00f; g = 0.48f; b = 0.10f; e = 1.00f; break;
        case VOX_ASPHALT:      r = 0.20f; g = 0.21f; b = 0.25f; break;
        case VOX_METAL:        r = 0.52f; g = 0.58f; b = 0.64f; break;
        case VOX_PORTAL_FRAME: r = 0.28f; g = 0.26f; b = 0.36f; break;
        case VOX_CARPET:       r = 0.62f; g = 0.11f; b = 0.14f; break;
        case VOX_CINEMA_STATIC:r = 0.55f; g = 0.56f; b = 0.58f; e = 0.35f; break;
        case VOX_CINEMA_LIT:   r = 1.00f; g = 0.88f; b = 0.45f; e = 0.92f; break;
        default: break;
    }
    out_rgba[0] = r;
    out_rgba[1] = g;
    out_rgba[2] = b;
    out_rgba[3] = 1.0f;
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
        size_t new_cap = mesh->vertex_cap ? mesh->vertex_cap * 2 : 4096;
        while (new_cap < mesh->vertex_count + add_verts) new_cap *= 2;
        VoxelVertex *nv = (VoxelVertex *)realloc(mesh->vertices, new_cap * sizeof(VoxelVertex));
        if (!nv) return false;
        mesh->vertices = nv;
        mesh->vertex_cap = new_cap;
    }
    if (mesh->index_count + add_inds > mesh->index_cap) {
        size_t new_cap = mesh->index_cap ? mesh->index_cap * 2 : 6144;
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
    /* +X */ { 1, 0, 0, {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}, {{0,0},{1,0},{1,1},{0,1}} },
    /* -X */ {-1, 0, 0, {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, {{0,0},{1,0},{1,1},{0,1}} },
    /* +Y */ { 0, 1, 0, {{0,1,1},{1,1,1},{1,1,0},{0,1,0}}, {{0,0},{1,0},{1,1},{0,1}} },
    /* -Y */ { 0,-1, 0, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, {{0,0},{1,0},{1,1},{0,1}} },
    /* +Z */ { 0, 0, 1, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{0,0},{1,0},{1,1},{0,1}} },
    /* -Z */ { 0, 0,-1, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, {{0,0},{1,0},{1,1},{0,1}} }
};

/* Subtle per-vertex ambient occlusion based on neighboring solid blocks */
static float compute_vertex_ao(const VoxelWorld *vw, int x, int y, int z, const FaceDef *f, const float c[3]) {
    int nx = x + f->dx;
    int ny = y + f->dy;
    int nz = z + f->dz;
    int sx = (c[0] > 0.5f) ? 1 : -1;
    int sy = (c[1] > 0.5f) ? 1 : -1;
    int sz = (c[2] > 0.5f) ? 1 : -1;

    int occ = 0;
    if (f->dx != 0) {
        if (voxel_is_solid(vw, nx, ny + sy, nz)) ++occ;
        if (voxel_is_solid(vw, nx, ny, nz + sz)) ++occ;
        if (voxel_is_solid(vw, nx, ny + sy, nz + sz)) ++occ;
    } else if (f->dy != 0) {
        if (voxel_is_solid(vw, nx + sx, ny, nz)) ++occ;
        if (voxel_is_solid(vw, nx, ny, nz + sz)) ++occ;
        if (voxel_is_solid(vw, nx + sx, ny, nz + sz)) ++occ;
    } else {
        if (voxel_is_solid(vw, nx + sx, ny, nz)) ++occ;
        if (voxel_is_solid(vw, nx, ny + sy, nz)) ++occ;
        if (voxel_is_solid(vw, nx + sx, ny + sy, nz)) ++occ;
    }
    static const float kAoTable[4] = {1.0f, 0.84f, 0.68f, 0.52f};
    return kAoTable[occ];
}

bool voxel_build_mesh(const VoxelWorld *vw, VoxelMesh *out_mesh) {
    if (!vw || !out_mesh) return false;
    out_mesh->vertex_count = 0;
    out_mesh->index_count = 0;

    for (int y = 0; y < VOX_WORLD_Y; ++y) {
        for (int z = 0; z < VOX_WORLD_Z; ++z) {
            for (int x = 0; x < VOX_WORLD_X; ++x) {
                uint8_t type = voxel_get(vw, x, y, z);
                if (type == VOX_AIR) continue;

                float rgba[4];
                float emissive = 0.0f;
                voxel_type_color(type, rgba, &emissive);

                /* Slight deterministic block tint variation for retro texture richness */
                float hash = (float)(((x * 73856093) ^ (y * 19349663) ^ (z * 83492791)) & 15) / 15.0f;
                float var = 0.92f + hash * 0.14f;

                for (int fi = 0; fi < 6; ++fi) {
                    const FaceDef *f = &kFaces[fi];
                    uint8_t neighbor = voxel_get(vw, x + f->dx, y + f->dy, z + f->dz);
                    if (neighbor != VOX_AIR && neighbor != VOX_FIRE) {
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
                        v.uv[0] = f->uv[ci][0];
                        v.uv[1] = f->uv[ci][1];
                        float ao = (emissive > 0.7f) ? 1.0f : compute_vertex_ao(vw, x, y, z, f, f->corners[ci]);
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
