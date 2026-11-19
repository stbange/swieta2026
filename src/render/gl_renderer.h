#ifndef SWIETA_GL_RENDERER_H
#define SWIETA_GL_RENDERER_H

#include "platform/touch.h"
#include "render/gl_loader.h"
#include "sim/world.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLsizei index_count;
} ChunkGpu;

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLsizei vert_count;
} MeshGpu;

typedef struct {
    bool initialized;
    int retro_w;
    int retro_h;
    bool dither_enabled;

    /* Programs */
    GLuint prog_sky;
    GLuint prog_shadow;
    GLuint prog_world;
    GLuint prog_post;
    GLuint prog_hud;

    /* Framebuffers */
    GLuint shadow_fbo;
    GLuint shadow_tex;
    GLuint retro_fbo;
    GLuint retro_color_tex;
    GLuint retro_depth_tex;

    /* Chunked Voxel GPU Buffers + Reusable Scratch CPU Mesh */
    VoxelMesh scratch_mesh;
    ChunkGpu chunks[VOX_CHUNK_COUNT];

    /* Pre-baked 3D Low-Poly Geometry Library (zero per-draw uploads!) */
    MeshGpu mesh_box;
    MeshGpu mesh_cyl;
    MeshGpu mesh_sphere;
    MeshGpu mesh_cone;

    /* Dynamic Billboard Quad, Fullscreen Quad, 2D HUD */
    GLuint bill_vao;
    GLuint bill_vbo;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint hud_vao;
    GLuint hud_vbo;

    /* Procedural CC BY 4.0 High-Detail Retro Atlases */
    GLuint voxel_atlas_tex;  /* 1024x192 (16x3 tiles of 64x64 diffuse) */
    GLuint voxel_normal_tex; /* 1024x192 (16x3 tiles of 64x64 tangent normal + gloss) */
    GLuint sprite_atlas_tex; /* 1280x128 (20x2 tiles of 64x64 animated sprites) */
    GLuint white_tex;
    GLuint flat_normal_tex;
} GlRenderer;

bool gl_renderer_init(GlRenderer *r, int retro_w, int retro_h);
void gl_renderer_shutdown(GlRenderer *r);
void gl_renderer_set_retro_res(GlRenderer *r, int retro_w, int retro_h);
void gl_renderer_render_frame(GlRenderer *r, GameWorld *w, const TouchController *touch, int win_w, int win_h);

/* WebXR stereo eye render hook */
void gl_renderer_render_xr_eye(GlRenderer *r, GameWorld *w, const Mat4 *proj, const Mat4 *view,
                               GLuint target_fbo, int vx, int vy, int vw, int vh);

#endif /* SWIETA_GL_RENDERER_H */
