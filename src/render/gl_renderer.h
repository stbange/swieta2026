#ifndef SWIETA_GL_RENDERER_H
#define SWIETA_GL_RENDERER_H

#include "platform/touch.h"
#include "render/gl_loader.h"
#include "sim/world.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool initialized;
    int retro_w;
    int retro_h;
    bool dither_enabled;

    /* Programs */
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

    /* Dynamic Voxel Mesh VAO/VBO/EBO */
    VoxelMesh voxel_mesh;
    GLuint voxel_vao;
    GLuint voxel_vbo;
    GLuint voxel_ebo;

    /* Shared Unit Cube (3D meshes) & Quad (billboards & fullscreen) */
    GLuint cube_vao;
    GLuint cube_vbo;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint hud_vao;
    GLuint hud_vbo;

    /* Procedural Retro Pixel-Art Texture Atlas (8x2 tiles of 16x16 = 128x32) */
    GLuint sprite_atlas_tex;
    GLuint white_tex;
} GlRenderer;

bool gl_renderer_init(GlRenderer *r, int retro_w, int retro_h);
void gl_renderer_shutdown(GlRenderer *r);
void gl_renderer_set_retro_res(GlRenderer *r, int retro_w, int retro_h);
void gl_renderer_render_frame(GlRenderer *r, GameWorld *w, const TouchController *touch, int win_w, int win_h);

/* WebXR stereo eye render hook */
void gl_renderer_render_xr_eye(GlRenderer *r, GameWorld *w, const Mat4 *proj, const Mat4 *view,
                               GLuint target_fbo, int vx, int vy, int vw, int vh);

#endif /* SWIETA_GL_RENDERER_H */
