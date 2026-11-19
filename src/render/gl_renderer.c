#include "render/gl_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#define GLSL_HEADER "#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n"
#else
#define GLSL_HEADER "#version 330 core\n"
#endif

#define SHADOW_SIZE 512

static const char *kShadowVS =
    GLSL_HEADER
    "layout(location=0) in vec3 aPos;\n"
    "uniform mat4 uLightVP;\n"
    "uniform mat4 uModel;\n"
    "void main() {\n"
    "  gl_Position = uLightVP * uModel * vec4(aPos, 1.0);\n"
    "}\n";

static const char *kShadowFS =
    GLSL_HEADER
    "void main() {}\n";

static const char *kWorldVS =
    GLSL_HEADER
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNormal;\n"
    "layout(location=2) in vec2 aUV;\n"
    "layout(location=3) in vec4 aColor;\n"
    "layout(location=4) in float aEmissive;\n"
    "uniform mat4 uProj;\n"
    "uniform mat4 uView;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uLightVP;\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vNormal;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "out float vEmissive;\n"
    "out vec4 vShadowPos;\n"
    "void main() {\n"
    "  vec4 wp = uModel * vec4(aPos, 1.0);\n"
    "  vWorldPos = wp.xyz;\n"
    "  vNormal = normalize(mat3(uModel) * aNormal);\n"
    "  vUV = aUV;\n"
    "  vColor = aColor;\n"
    "  vEmissive = aEmissive;\n"
    "  vShadowPos = uLightVP * wp;\n"
    "  gl_Position = uProj * uView * wp;\n"
    "}\n";

static const char *kWorldFS =
    GLSL_HEADER
    "in vec3 vWorldPos;\n"
    "in vec3 vNormal;\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "in float vEmissive;\n"
    "in vec4 vShadowPos;\n"
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uShadowMap;\n"
    "uniform vec3 uCamPos;\n"
    "uniform vec3 uSunDir;\n"
    "uniform vec3 uSunColor;\n"
    "uniform vec3 uAmbient;\n"
    "uniform vec3 uFogColor;\n"
    "uniform float uFogDensity;\n"
    "uniform int uLightCount;\n"
    "uniform vec4 uLightPosRad[12];\n"
    "uniform vec4 uLightColInt[12];\n"
    "uniform int uUseTex;\n"
    "out vec4 FragColor;\n"
    "float sampleShadow(vec4 sp, float ndotl) {\n"
    "  vec3 proj = sp.xyz / max(sp.w, 0.0001);\n"
    "  proj = proj * 0.5 + 0.5;\n"
    "  if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;\n"
    "  float bias = max(0.004 * (1.0 - ndotl), 0.0018);\n"
    "  float texel = 1.0 / 512.0;\n"
    "  float lit = 0.0;\n"
    "  for (int x = 0; x <= 1; ++x) {\n"
    "    for (int y = 0; y <= 1; ++y) {\n"
    "      float d = texture(uShadowMap, proj.xy + vec2(float(x)-0.5, float(y)-0.5) * texel).r;\n"
    "      lit += (proj.z - bias <= d) ? 1.0 : 0.25;\n"
    "    }\n"
    "  }\n"
    "  return lit * 0.25;\n"
    "}\n"
    "void main() {\n"
    "  vec4 baseCol = vColor;\n"
    "  if (uUseTex == 1) {\n"
    "    vec4 texCol = texture(uTex, vUV);\n"
    "    if (texCol.a < 0.35) discard;\n"
    "    baseCol *= texCol;\n"
    "  } else {\n"
    "    /* Subtle procedural retro pixel grain on voxels/meshes */\n"
    "    vec2 g = floor(vUV * 8.0);\n"
    "    float checker = mod(g.x + g.y, 2.0);\n"
    "    baseCol.rgb *= (0.94 + 0.06 * checker);\n"
    "  }\n"
    "  vec3 N = normalize(vNormal);\n"
    "  float ndotl = max(dot(N, normalize(uSunDir)), 0.0);\n"
    "  float sh = sampleShadow(vShadowPos, ndotl);\n"
    "  vec3 lighting = uAmbient + uSunColor * ndotl * sh;\n"
    "  for (int i = 0; i < 12; ++i) {\n"
    "    if (i >= uLightCount) break;\n"
    "    vec3 toL = uLightPosRad[i].xyz - vWorldPos;\n"
    "    float dist = length(toL);\n"
    "    float rad = uLightPosRad[i].w;\n"
    "    if (dist < rad) {\n"
    "      float atten = 1.0 - (dist / rad);\n"
    "      atten = atten * atten;\n"
    "      /* Stepped retro light banding */\n"
    "      atten = floor(atten * 10.0) / 10.0;\n"
    "      float diff = max(dot(N, toL / max(dist, 0.001)), 0.25);\n"
    "      lighting += uLightColInt[i].rgb * uLightColInt[i].w * diff * atten;\n"
    "    }\n"
    "  }\n"
    "  vec3 litRgb = baseCol.rgb * lighting + baseCol.rgb * vEmissive * 1.15;\n"
    "  float camDist = length(uCamPos - vWorldPos);\n"
    "  float fog = 1.0 - exp(-camDist * uFogDensity);\n"
    "  vec3 finalRgb = mix(litRgb, uFogColor, clamp(fog, 0.0, 0.92));\n"
    "  FragColor = vec4(finalRgb, baseCol.a);\n"
    "}\n";

static const char *kPostVS =
    GLSL_HEADER
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "  vUV = aUV;\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char *kPostFS =
    GLSL_HEADER
    "in vec2 vUV;\n"
    "uniform sampler2D uScene;\n"
    "uniform vec2 uRetroSize;\n"
    "uniform int uDither;\n"
    "out vec4 FragColor;\n"
    "float bayer4(vec2 p) {\n"
    "  int x = int(mod(p.x, 4.0));\n"
    "  int y = int(mod(p.y, 4.0));\n"
    "  int idx = y * 4 + x;\n"
    "  if (idx == 0) return 0.0/16.0;  if (idx == 1) return 8.0/16.0;\n"
    "  if (idx == 2) return 2.0/16.0;  if (idx == 3) return 10.0/16.0;\n"
    "  if (idx == 4) return 12.0/16.0; if (idx == 5) return 4.0/16.0;\n"
    "  if (idx == 6) return 14.0/16.0; if (idx == 7) return 6.0/16.0;\n"
    "  if (idx == 8) return 3.0/16.0;  if (idx == 9) return 11.0/16.0;\n"
    "  if (idx == 10) return 1.0/16.0; if (idx == 11) return 9.0/16.0;\n"
    "  if (idx == 12) return 15.0/16.0;if (idx == 13) return 7.0/16.0;\n"
    "  if (idx == 14) return 13.0/16.0;return 5.0/16.0;\n"
    "}\n"
    "void main() {\n"
    "  vec3 col = texture(uScene, vUV).rgb;\n"
    "  if (uDither == 1) {\n"
    "    vec2 px = floor(vUV * uRetroSize);\n"
    "    float d = (bayer4(px) - 0.5) / 28.0;\n"
    "    col = floor(clamp(col + d, 0.0, 1.0) * 31.0 + 0.5) / 31.0;\n"
    "    float scan = 0.96 + 0.04 * mod(px.y, 2.0);\n"
    "    col *= scan;\n"
    "  }\n"
    "  FragColor = vec4(col, 1.0);\n"
    "}\n";

static const char *kHudVS =
    GLSL_HEADER
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec4 aCol;\n"
    "out vec4 vCol;\n"
    "void main() {\n"
    "  vCol = aCol;\n"
    "  gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);\n"
    "  gl_Position.y = -gl_Position.y;\n"
    "}\n";

static const char *kHudFS =
    GLSL_HEADER
    "in vec4 vCol;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "  FragColor = vCol;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(const char *vs_src, const char *fs_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* Procedural CC BY 4.0 retro 16x16 pixel-art sprite atlas (8 tiles wide: 128x16)
 * Tile 0: Alien Trooper | Tile 1: Pig-Guard | Tile 2: Holo-Decoy
 * Tile 3: Medkit        | Tile 4: Pickup/Orb | Tile 5: Fire/Explosion
 * Tile 6: Golden Shard  | Tile 7: Portal Swirl
 */
static void create_procedural_sprite_atlas(GlRenderer *r) {
    const int W = 128, H = 16;
    uint8_t px[128 * 16 * 4];
    memset(px, 0, sizeof(px));

    for (int tile = 0; tile < 8; ++tile) {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                float u = ((float)x + 0.5f) / 16.0f - 0.5f;
                float v = ((float)y + 0.5f) / 16.0f - 0.5f;
                float d = sqrtf(u * u + v * v);
                uint8_t cr = 0, cg = 0, cb = 0, ca = 0;

                if (tile == 0) { /* Alien Trooper */
                    if ((y >= 1 && y <= 4 && x >= 5 && x <= 10) ||
                        (y >= 5 && y <= 11 && x >= 4 && x <= 11) ||
                        (y >= 12 && y <= 15 && ((x >= 4 && x <= 6) || (x >= 9 && x <= 11)))) {
                        cr = 70; cg = 210; cb = 90; ca = 255;
                        if (y == 2 && (x == 6 || x == 9)) { cr = 255; cg = 40; cb = 40; }
                        if (y >= 6 && y <= 9 && x >= 6 && x <= 9) { cr = 180; cg = 45; cb = 65; }
                    }
                } else if (tile == 1) { /* Boar-Cop Enforcer */
                    if ((y >= 1 && y <= 5 && x >= 4 && x <= 11) ||
                        (y >= 6 && y <= 12 && x >= 3 && x <= 12) ||
                        (y >= 13 && y <= 15 && ((x >= 3 && x <= 6) || (x >= 9 && x <= 12)))) {
                        cr = 210; cg = 135; cb = 140; ca = 255;
                        if (y >= 6 && y <= 11) { cr = 65; cg = 85; cb = 155; }
                        if (y == 3 && (x == 6 || x == 9)) { cr = 255; cg = 230; cb = 50; }
                    }
                } else if (tile == 2) { /* Holo-Decoy */
                    if (((y >= 2 && y <= 14 && x >= 5 && x <= 10) || (y >= 5 && y <= 9 && x >= 3 && x <= 12)) && (y % 2 == 0)) {
                        cr = 70; cg = 235; cb = 255; ca = 220;
                    }
                } else if (tile == 3) { /* Medkit */
                    if (x >= 2 && x <= 13 && y >= 3 && y <= 12) {
                        cr = 235; cg = 235; cb = 240; ca = 255;
                        if ((x >= 7 && x <= 8 && y >= 5 && y <= 10) || (y >= 7 && y <= 8 && x >= 5 && x <= 10)) {
                            cr = 220; cg = 30; cb = 40;
                        }
                    }
                } else if (tile == 4) { /* Glowing Orb / Pickup */
                    if (d < 0.42f) {
                        float glow = 1.0f - d / 0.42f;
                        cr = (uint8_t)(120 + 135 * glow);
                        cg = (uint8_t)(220 + 35 * glow);
                        cb = (uint8_t)(120 + 135 * glow);
                        ca = 255;
                    }
                } else if (tile == 5) { /* Fire / Explosion */
                    if (d < 0.46f) {
                        float t = 1.0f - d / 0.46f;
                        cr = 255;
                        cg = (uint8_t)(80 + 170 * t);
                        cb = (uint8_t)(20 + 100 * t * t);
                        ca = 255;
                    }
                } else if (tile == 6) { /* Golden Shades / Memory Shard */
                    if (fabsf(u) + fabsf(v) < 0.42f) {
                        cr = 255; cg = 215; cb = 60; ca = 255;
                        if (y == 7 && x >= 5 && x <= 10) { cr = 40; cg = 25; cb = 45; }
                    }
                } else if (tile == 7) { /* Portal Swirl */
                    if (d < 0.48f) {
                        float ang = atan2f(v, u) + d * 10.0f;
                        float sw = 0.5f + 0.5f * sinf(ang * 3.0f);
                        cr = (uint8_t)(150 + 105 * sw);
                        cg = (uint8_t)(170 + 85 * sw);
                        cb = 255;
                        ca = (uint8_t)(220 * (1.0f - d * 1.4f));
                    }
                }

                int idx = (y * W + tile * 16 + x) * 4;
                px[idx + 0] = cr;
                px[idx + 1] = cg;
                px[idx + 2] = cb;
                px[idx + 3] = ca;
            }
        }
    }

    glGenTextures(1, &r->sprite_atlas_tex);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    uint8_t white[4] = {255, 255, 255, 255};
    glGenTextures(1, &r->white_tex);
    glBindTexture(GL_TEXTURE_2D, r->white_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static void setup_retro_fbo(GlRenderer *r, int w, int h) {
    r->retro_w = w;
    r->retro_h = h;
    if (r->retro_fbo) {
        glDeleteFramebuffers(1, &r->retro_fbo);
        glDeleteTextures(1, &r->retro_color_tex);
        glDeleteTextures(1, &r->retro_depth_tex);
    }
    glGenFramebuffers(1, &r->retro_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->retro_fbo);

    glGenTextures(1, &r->retro_color_tex);
    glBindTexture(GL_TEXTURE_2D, r->retro_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->retro_color_tex, 0);

    glGenTextures(1, &r->retro_depth_tex);
    glBindTexture(GL_TEXTURE_2D, r->retro_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r->retro_depth_tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void setup_shadow_fbo(GlRenderer *r) {
    glGenFramebuffers(1, &r->shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->shadow_fbo);

    glGenTextures(1, &r->shadow_tex);
    glBindTexture(GL_TEXTURE_2D, r->shadow_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r->shadow_tex, 0);

    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void setup_unit_cube(GlRenderer *r) {
    /* 36 vertices of VoxelVertex layout for 3D low-poly meshes */
    static const float c[36][8] = {
        /* +Z */ {-0.5f,-0.5f, 0.5f, 0,0,1, 0,0}, { 0.5f,-0.5f, 0.5f, 0,0,1, 1,0}, { 0.5f, 0.5f, 0.5f, 0,0,1, 1,1},
                 {-0.5f,-0.5f, 0.5f, 0,0,1, 0,0}, { 0.5f, 0.5f, 0.5f, 0,0,1, 1,1}, {-0.5f, 0.5f, 0.5f, 0,0,1, 0,1},
        /* -Z */ { 0.5f,-0.5f,-0.5f, 0,0,-1, 0,0}, {-0.5f,-0.5f,-0.5f, 0,0,-1, 1,0}, {-0.5f, 0.5f,-0.5f, 0,0,-1, 1,1},
                 { 0.5f,-0.5f,-0.5f, 0,0,-1, 0,0}, {-0.5f, 0.5f,-0.5f, 0,0,-1, 1,1}, { 0.5f, 0.5f,-0.5f, 0,0,-1, 0,1},
        /* +X */ { 0.5f,-0.5f, 0.5f, 1,0,0, 0,0}, { 0.5f,-0.5f,-0.5f, 1,0,0, 1,0}, { 0.5f, 0.5f,-0.5f, 1,0,0, 1,1},
                 { 0.5f,-0.5f, 0.5f, 1,0,0, 0,0}, { 0.5f, 0.5f,-0.5f, 1,0,0, 1,1}, { 0.5f, 0.5f, 0.5f, 1,0,0, 0,1},
        /* -X */ {-0.5f,-0.5f,-0.5f,-1,0,0, 0,0}, {-0.5f,-0.5f, 0.5f,-1,0,0, 1,0}, {-0.5f, 0.5f, 0.5f,-1,0,0, 1,1},
                 {-0.5f,-0.5f,-0.5f,-1,0,0, 0,0}, {-0.5f, 0.5f, 0.5f,-1,0,0, 1,1}, {-0.5f, 0.5f,-0.5f,-1,0,0, 0,1},
        /* +Y */ {-0.5f, 0.5f, 0.5f, 0,1,0, 0,0}, { 0.5f, 0.5f, 0.5f, 0,1,0, 1,0}, { 0.5f, 0.5f,-0.5f, 0,1,0, 1,1},
                 {-0.5f, 0.5f, 0.5f, 0,1,0, 0,0}, { 0.5f, 0.5f,-0.5f, 0,1,0, 1,1}, {-0.5f, 0.5f,-0.5f, 0,1,0, 0,1},
        /* -Y */ {-0.5f,-0.5f,-0.5f, 0,-1,0, 0,0}, { 0.5f,-0.5f,-0.5f, 0,-1,0, 1,0}, { 0.5f,-0.5f, 0.5f, 0,-1,0, 1,1},
                 {-0.5f,-0.5f,-0.5f, 0,-1,0, 0,0}, { 0.5f,-0.5f, 0.5f, 0,-1,0, 1,1}, {-0.5f,-0.5f, 0.5f, 0,-1,0, 0,1}
    };
    VoxelVertex verts[36];
    for (int i = 0; i < 36; ++i) {
        verts[i].pos[0] = c[i][0]; verts[i].pos[1] = c[i][1]; verts[i].pos[2] = c[i][2];
        verts[i].normal[0] = c[i][3]; verts[i].normal[1] = c[i][4]; verts[i].normal[2] = c[i][5];
        verts[i].uv[0] = c[i][6]; verts[i].uv[1] = c[i][7];
        verts[i].color[0] = 1.0f; verts[i].color[1] = 1.0f; verts[i].color[2] = 1.0f; verts[i].color[3] = 1.0f;
        verts[i].emissive = 0.0f;
    }
    glGenVertexArrays(1, &r->cube_vao);
    glGenBuffers(1, &r->cube_vbo);
    glBindVertexArray(r->cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(12 * sizeof(float)));
    glBindVertexArray(0);
}

bool gl_renderer_init(GlRenderer *r, int retro_w, int retro_h) {
    if (!r) return false;
    memset(r, 0, sizeof(*r));
    if (!sw_gl_load_functions()) {
        fprintf(stderr, "Failed to load OpenGL 3.3 Core functions\n");
        return false;
    }

    r->dither_enabled = true;
    r->prog_shadow = link_program(kShadowVS, kShadowFS);
    r->prog_world  = link_program(kWorldVS,  kWorldFS);
    r->prog_post   = link_program(kPostVS,   kPostFS);
    r->prog_hud    = link_program(kHudVS,    kHudFS);
    if (!r->prog_shadow || !r->prog_world || !r->prog_post || !r->prog_hud) {
        return false;
    }

    voxel_mesh_init(&r->voxel_mesh);
    glGenVertexArrays(1, &r->voxel_vao);
    glGenBuffers(1, &r->voxel_vbo);
    glGenBuffers(1, &r->voxel_ebo);
    glBindVertexArray(r->voxel_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->voxel_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->voxel_ebo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(12 * sizeof(float)));
    glBindVertexArray(0);

    setup_unit_cube(r);

    static const float quad_verts[6 * 4] = {
        -1,-1, 0,0,   1,-1, 1,0,   1, 1, 1,1,
        -1,-1, 0,0,   1, 1, 1,1,  -1, 1, 0,1
    };
    glGenVertexArrays(1, &r->quad_vao);
    glGenBuffers(1, &r->quad_vbo);
    glBindVertexArray(r->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenVertexArrays(1, &r->hud_vao);
    glGenBuffers(1, &r->hud_vbo);
    glBindVertexArray(r->hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->hud_vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);

    create_procedural_sprite_atlas(r);
    setup_shadow_fbo(r);
    setup_retro_fbo(r, retro_w > 0 ? retro_w : 400, retro_h > 0 ? retro_h : 240);

    r->initialized = true;
    return true;
}

void gl_renderer_set_retro_res(GlRenderer *r, int retro_w, int retro_h) {
    if (!r || !r->initialized) return;
    if (retro_w != r->retro_w || retro_h != r->retro_h) {
        setup_retro_fbo(r, retro_w, retro_h);
    }
}

void gl_renderer_shutdown(GlRenderer *r) {
    if (!r || !r->initialized) return;
    voxel_mesh_free(&r->voxel_mesh);
    glDeleteProgram(r->prog_shadow);
    glDeleteProgram(r->prog_world);
    glDeleteProgram(r->prog_post);
    glDeleteProgram(r->prog_hud);
    glDeleteFramebuffers(1, &r->shadow_fbo);
    glDeleteFramebuffers(1, &r->retro_fbo);
    glDeleteTextures(1, &r->shadow_tex);
    glDeleteTextures(1, &r->retro_color_tex);
    glDeleteTextures(1, &r->retro_depth_tex);
    glDeleteTextures(1, &r->sprite_atlas_tex);
    glDeleteTextures(1, &r->white_tex);
    glDeleteVertexArrays(1, &r->voxel_vao);
    glDeleteBuffers(1, &r->voxel_vbo);
    glDeleteBuffers(1, &r->voxel_ebo);
    glDeleteVertexArrays(1, &r->cube_vao);
    glDeleteBuffers(1, &r->cube_vbo);
    glDeleteVertexArrays(1, &r->quad_vao);
    glDeleteBuffers(1, &r->quad_vbo);
    glDeleteVertexArrays(1, &r->hud_vao);
    glDeleteBuffers(1, &r->hud_vbo);
    r->initialized = false;
}

static void sync_voxel_mesh_if_dirty(GlRenderer *r, GameWorld *w) {
    if (!w->voxels.dirty) return;
    if (voxel_build_mesh(&w->voxels, &r->voxel_mesh)) {
        glBindBuffer(GL_ARRAY_BUFFER, r->voxel_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(r->voxel_mesh.vertex_count * sizeof(VoxelVertex)),
                     r->voxel_mesh.vertices, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->voxel_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(r->voxel_mesh.index_count * sizeof(uint32_t)),
                     r->voxel_mesh.indices, GL_DYNAMIC_DRAW);
        w->voxels.dirty = false;
    }
}

static void draw_colored_box(GlRenderer *r, GLuint prog, Mat4 model, float cr, float cg, float cb, float ca, float emissive) {
    static const float c[36][8] = {
        {-0.5f,-0.5f, 0.5f, 0,0,1, 0,0}, { 0.5f,-0.5f, 0.5f, 0,0,1, 1,0}, { 0.5f, 0.5f, 0.5f, 0,0,1, 1,1},
        {-0.5f,-0.5f, 0.5f, 0,0,1, 0,0}, { 0.5f, 0.5f, 0.5f, 0,0,1, 1,1}, {-0.5f, 0.5f, 0.5f, 0,0,1, 0,1},
        { 0.5f,-0.5f,-0.5f, 0,0,-1, 0,0}, {-0.5f,-0.5f,-0.5f, 0,0,-1, 1,0}, {-0.5f, 0.5f,-0.5f, 0,0,-1, 1,1},
        { 0.5f,-0.5f,-0.5f, 0,0,-1, 0,0}, {-0.5f, 0.5f,-0.5f, 0,0,-1, 1,1}, { 0.5f, 0.5f,-0.5f, 0,0,-1, 0,1},
        { 0.5f,-0.5f, 0.5f, 1,0,0, 0,0}, { 0.5f,-0.5f,-0.5f, 1,0,0, 1,0}, { 0.5f, 0.5f,-0.5f, 1,0,0, 1,1},
        { 0.5f,-0.5f, 0.5f, 1,0,0, 0,0}, { 0.5f, 0.5f,-0.5f, 1,0,0, 1,1}, { 0.5f, 0.5f, 0.5f, 1,0,0, 0,1},
        {-0.5f,-0.5f,-0.5f,-1,0,0, 0,0}, {-0.5f,-0.5f, 0.5f,-1,0,0, 1,0}, {-0.5f, 0.5f, 0.5f,-1,0,0, 1,1},
        {-0.5f,-0.5f,-0.5f,-1,0,0, 0,0}, {-0.5f, 0.5f, 0.5f,-1,0,0, 1,1}, {-0.5f, 0.5f,-0.5f,-1,0,0, 0,1},
        {-0.5f, 0.5f, 0.5f, 0,1,0, 0,0}, { 0.5f, 0.5f, 0.5f, 0,1,0, 1,0}, { 0.5f, 0.5f,-0.5f, 0,1,0, 1,1},
        {-0.5f, 0.5f, 0.5f, 0,1,0, 0,0}, { 0.5f, 0.5f,-0.5f, 0,1,0, 1,1}, {-0.5f, 0.5f,-0.5f, 0,1,0, 0,1},
        {-0.5f,-0.5f,-0.5f, 0,-1,0, 0,0}, { 0.5f,-0.5f,-0.5f, 0,-1,0, 1,0}, { 0.5f,-0.5f, 0.5f, 0,-1,0, 1,1},
        {-0.5f,-0.5f,-0.5f, 0,-1,0, 0,0}, { 0.5f,-0.5f, 0.5f, 0,-1,0, 1,1}, {-0.5f,-0.5f, 0.5f, 0,-1,0, 0,1}
    };
    VoxelVertex verts[36];
    for (int i = 0; i < 36; ++i) {
        verts[i].pos[0] = c[i][0]; verts[i].pos[1] = c[i][1]; verts[i].pos[2] = c[i][2];
        verts[i].normal[0] = c[i][3]; verts[i].normal[1] = c[i][4]; verts[i].normal[2] = c[i][5];
        verts[i].uv[0] = c[i][6]; verts[i].uv[1] = c[i][7];
        verts[i].color[0] = cr; verts[i].color[1] = cg; verts[i].color[2] = cb; verts[i].color[3] = ca;
        verts[i].emissive = emissive;
    }
    glUniformMatrix4fv(glGetUniformLocation(prog, "uModel"), 1, GL_FALSE, model.m);
    glUniform1i(glGetUniformLocation(prog, "uUseTex"), 0);
    glBindVertexArray(r->cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_billboard_sprite(GlRenderer *r, Vec3 pos, float w, float h, int tile, float cam_yaw,
                                  float cr, float cg, float cb, float emissive) {
    float u0 = (float)tile / 8.0f;
    float u1 = (float)(tile + 1) / 8.0f;
    VoxelVertex v[6];
    const float q[6][4] = {
        {-0.5f, 0.0f, u0, 1.0f}, { 0.5f, 0.0f, u1, 1.0f}, { 0.5f, 1.0f, u1, 0.0f},
        {-0.5f, 0.0f, u0, 1.0f}, { 0.5f, 1.0f, u1, 0.0f}, {-0.5f, 1.0f, u0, 0.0f}
    };
    for (int i = 0; i < 6; ++i) {
        v[i].pos[0] = q[i][0] * w; v[i].pos[1] = q[i][1] * h; v[i].pos[2] = 0.0f;
        v[i].normal[0] = 0.0f; v[i].normal[1] = 0.2f; v[i].normal[2] = 1.0f;
        v[i].uv[0] = q[i][2]; v[i].uv[1] = q[i][3];
        v[i].color[0] = cr; v[i].color[1] = cg; v[i].color[2] = cb; v[i].color[3] = 1.0f;
        v[i].emissive = emissive;
    }
    Mat4 model = mat4_mul(mat4_translate(pos), mat4_rotate_y(-cam_yaw));
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, model.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas_tex);
    glBindVertexArray(r->cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static Mat4 compute_light_vp(const GameWorld *w, Vec3 *out_sun_dir) {
    Vec3 sun_dir = (w->scene == SCENE_DAY01)
        ? vec3_norm(vec3_make(-0.45f, 0.85f, 0.35f))
        : vec3_norm(vec3_make(0.35f, 0.90f, 0.40f));
    if (out_sun_dir) *out_sun_dir = sun_dir;
    Vec3 center = vec3_make(24.0f, 3.0f, 24.0f);
    Vec3 eye = vec3_add(center, vec3_scale(sun_dir, 26.0f));
    Mat4 view = mat4_look_at(eye, center, vec3_make(0, 1, 0));
    Mat4 proj = mat4_ortho(-28.0f, 28.0f, -28.0f, 28.0f, 1.0f, 65.0f);
    return mat4_mul(proj, view);
}

static void render_shadow_pass(GlRenderer *r, const GameWorld *w, Mat4 light_vp) {
    glBindFramebuffer(GL_FRAMEBUFFER, r->shadow_fbo);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(r->prog_shadow);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uLightVP"), 1, GL_FALSE, light_vp.m);

    Mat4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, ident.m);
    glBindVertexArray(r->voxel_vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)r->voxel_mesh.index_count, GL_UNSIGNED_INT, 0);

    /* Cast dynamic shadows from 3D Boss Mech & Players */
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float sc = (e->kind == ENEMY_BOSS_MECH) ? 1.8f : (e->shrunk_timer > 0.0f ? 0.35f : 0.9f);
        Mat4 m = mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, sc * 0.6f, 0))),
                          mat4_scale(vec3_make(sc * 0.7f, sc * 1.2f, sc * 0.7f)));
        glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, m.m);
        glBindVertexArray(r->cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void render_scene_view(GlRenderer *r, const GameWorld *w, Mat4 proj, Mat4 view,
                              Vec3 cam_pos, float cam_yaw, int viewer_slot, Mat4 light_vp, Vec3 sun_dir) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (w->scene == SCENE_DAY01) {
        glClearColor(0.06f, 0.06f, 0.14f, 1.0f);
    } else if (w->scene == SCENE_PREVIEW) {
        glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
    } else {
        glClearColor(0.10f, 0.06f, 0.05f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(r->prog_world);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uProj"), 1, GL_FALSE, proj.m);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uLightVP"), 1, GL_FALSE, light_vp.m);
    glUniform3f(glGetUniformLocation(r->prog_world, "uCamPos"), cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunDir"), sun_dir.x, sun_dir.y, sun_dir.z);

    if (w->scene == SCENE_DAY01) {
        glUniform3f(glGetUniformLocation(r->prog_world, "uSunColor"), 0.52f, 0.58f, 0.80f);
        glUniform3f(glGetUniformLocation(r->prog_world, "uAmbient"),  0.22f, 0.22f, 0.30f);
        glUniform3f(glGetUniformLocation(r->prog_world, "uFogColor"), 0.06f, 0.06f, 0.14f);
        glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), 0.022f);
    } else {
        glUniform3f(glGetUniformLocation(r->prog_world, "uSunColor"), 0.65f, 0.56f, 0.45f);
        glUniform3f(glGetUniformLocation(r->prog_world, "uAmbient"),  0.28f, 0.23f, 0.20f);
        glUniform3f(glGetUniformLocation(r->prog_world, "uFogColor"), 0.11f, 0.07f, 0.06f);
        glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), 0.018f);
    }

    int lcount = w->light_count > 12 ? 12 : w->light_count;
    float lpos[12 * 4] = {0};
    float lcol[12 * 4] = {0};
    for (int i = 0; i < lcount; ++i) {
        lpos[i * 4 + 0] = w->lights[i].pos.x;
        lpos[i * 4 + 1] = w->lights[i].pos.y;
        lpos[i * 4 + 2] = w->lights[i].pos.z;
        lpos[i * 4 + 3] = w->lights[i].radius;
        lcol[i * 4 + 0] = w->lights[i].r;
        lcol[i * 4 + 1] = w->lights[i].g;
        lcol[i * 4 + 2] = w->lights[i].b;
        lcol[i * 4 + 3] = w->lights[i].intensity;
    }
    glUniform1i(glGetUniformLocation(r->prog_world, "uLightCount"), lcount);
    glUniform4fv(glGetUniformLocation(r->prog_world, "uLightPosRad"), 12, lpos);
    glUniform4fv(glGetUniformLocation(r->prog_world, "uLightColInt"), 12, lcol);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r->shadow_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uShadowMap"), 1);
    glUniform1i(glGetUniformLocation(r->prog_world, "uTex"), 0);

    /* 1. Draw Voxel World */
    Mat4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, ident.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 0);
    glBindVertexArray(r->voxel_vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)r->voxel_mesh.index_count, GL_UNSIGNED_INT, 0);

    /* 2. Draw 3D Meshes: 24 Hub Portals OR Return Portal, Spinning Star, Boss Mech, Other Players */
    if (w->scene == SCENE_HUB) {
        /* Spinning golden Christmas Tree Star */
        Mat4 star_m = mat4_mul(mat4_translate(vec3_make(24.5f, 10.1f, 24.5f)),
                      mat4_mul(mat4_rotate_y(w->time_sec * 1.4f), mat4_scale(vec3_make(0.8f, 0.8f, 0.8f))));
        draw_colored_box(r, r->prog_world, star_m, 1.0f, 0.9f, 0.25f, 1.0f, 0.95f);

        for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
            const PortalMeta *meta = swieta_portal_meta(w->portals[i].day);
            bool unlocked = swieta_portal_is_unlocked(w->portals[i].day, w->now, w->unlock_all);
            bool done = (w->shard_mask & (1u << (unsigned)i)) != 0u;
            float pr = unlocked ? (meta ? meta->color_r : 0.4f) : 0.35f;
            float pg = unlocked ? (meta ? meta->color_g : 0.7f) : 0.48f;
            float pb = unlocked ? (meta ? meta->color_b : 1.0f) : 0.62f;
            if (done) { pr = 1.0f; pg = 0.86f; pb = 0.25f; }
            Vec3 p = vec3_add(w->portals[i].pos, vec3_make(0.5f, 1.5f, 0.5f));
            bool x_wall = (w->portals[i].pos.x <= 5.0f || w->portals[i].pos.x >= 42.0f);
            Vec3 sc = x_wall ? vec3_make(0.18f, 2.6f, 1.6f) : vec3_make(1.6f, 2.6f, 0.18f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(p), mat4_scale(sc)),
                             pr, pg, pb, unlocked ? 0.88f : 0.55f, unlocked ? 0.85f : 0.15f);
        }
    } else {
        /* Glowing Return Portal */
        Vec3 rp = vec3_add(w->return_portal_pos, vec3_make(0, 1.4f, 0));
        draw_billboard_sprite(r, rp, 1.8f, 2.6f, 7, cam_yaw, 0.3f, 0.9f, 1.0f, 0.95f);
    }

    /* Remote / Split-screen Co-op Player Avatars (3D Low-Poly + Santa Hat!) */
    for (int i = 0; i < w->player_count; ++i) {
        if (i == viewer_slot || !w->players[i].active || w->players[i].hp <= 0) continue;
        Vec3 pp = w->players[i].pos;
        float colors[4][3] = {{0.2f,0.7f,1.0f},{1.0f,0.35f,0.35f},{0.35f,0.95f,0.45f},{1.0f,0.85f,0.25f}};
        draw_colored_box(r, r->prog_world,
                         mat4_mul(mat4_translate(vec3_add(pp, vec3_make(0, 0.75f, 0))),
                         mat4_mul(mat4_rotate_y(-w->players[i].yaw), mat4_scale(vec3_make(0.65f, 1.15f, 0.45f)))),
                         colors[i][0], colors[i][1], colors[i][2], 1.0f, 0.1f);
        /* Santa hat */
        draw_colored_box(r, r->prog_world,
                         mat4_mul(mat4_translate(vec3_add(pp, vec3_make(0, 1.55f, 0))),
                                  mat4_scale(vec3_make(0.45f, 0.42f, 0.45f))),
                         0.92f, 0.14f, 0.18f, 1.0f, 0.15f);
    }

    /* 3. Draw Enemies (Sprites for Troopers/Pig-Guards, Articulated 3D Mesh for Boss Mech) */
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float fl = (e->hit_flash > 0.0f) ? 1.0f : 0.0f;
        if (e->kind == ENEMY_BOSS_MECH) {
            float bob = 0.08f * sinf(e->anim_time * 5.0f);
            /* Articulated 3D low-poly Battle-Mech */
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, 1.6f + bob, 0))),
                             mat4_mul(mat4_rotate_y(-e->yaw), mat4_scale(vec3_make(1.7f, 1.4f, 1.3f)))),
                             0.78f + fl * 0.22f, 0.18f + fl * 0.5f, 0.22f + fl * 0.5f, 1.0f, 0.25f + fl * 0.5f);
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(-0.55f, 0.6f, 0))),
                                      mat4_scale(vec3_make(0.45f, 1.2f, 0.55f))),
                             0.35f, 0.38f, 0.42f, 1.0f, 0.05f);
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0.55f, 0.6f, 0))),
                                      mat4_scale(vec3_make(0.45f, 1.2f, 0.55f))),
                             0.35f, 0.38f, 0.42f, 1.0f, 0.05f);
            /* Glowing visor */
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, 1.95f + bob, 0))),
                                      mat4_scale(vec3_make(1.0f, 0.28f, 1.42f))),
                             1.0f, 0.2f, 0.15f, 1.0f, 0.95f);
        } else {
            float sc = (e->shrunk_timer > 0.0f) ? 0.35f : 1.0f;
            int tile = (e->kind == ENEMY_PIGGUARD) ? 1 : 0;
            draw_billboard_sprite(r, e->pos, 1.2f * sc, 1.75f * sc, tile, cam_yaw,
                                  1.0f, 1.0f - fl * 0.3f, 1.0f - fl * 0.3f, fl * 0.7f);
        }
    }

    /* Holo-Decoy Sprite */
    if (w->decoy.active) {
        draw_billboard_sprite(r, w->decoy.pos, 1.1f, 1.7f, 2, cam_yaw, 0.4f, 0.95f, 1.0f, 0.9f);
    }

    /* Pickups & Projectiles & Explosions */
    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        const Pickup *pk = &w->pickups[i];
        if (!pk->active) continue;
        Vec3 pos = vec3_add(pk->pos, vec3_make(0, 0.15f * sinf(pk->bob_phase), 0));
        int tile = (pk->kind == PICKUP_MEDKIT) ? 3 : (pk->kind == PICKUP_SHARD ? 6 : 4);
        float cr = (pk->kind == PICKUP_PIPEBOMB) ? 1.0f : (pk->kind == PICKUP_ARMOR ? 0.4f : 1.0f);
        float cg = (pk->kind == PICKUP_PIPEBOMB) ? 0.45f : (pk->kind == PICKUP_ARMOR ? 0.8f : 1.0f);
        float cb = (pk->kind == PICKUP_SHRINK_AMMO) ? 0.4f : 1.0f;
        draw_billboard_sprite(r, pos, 0.8f, 0.8f, tile, cam_yaw, cr, cg, cb, 0.65f);
    }

    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        const Projectile *pr = &w->projectiles[i];
        if (!pr->active) continue;
        if (pr->kind == PROJ_PIPEBOMB) {
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(pr->pos), mat4_scale(vec3_make(0.28f, 0.18f, 0.42f))),
                             0.95f, 0.25f, 0.2f, 1.0f, 0.75f);
        } else if (pr->kind == PROJ_SHRINK_ORB) {
            draw_billboard_sprite(r, pr->pos, 0.6f, 0.6f, 4, cam_yaw, 0.25f, 1.0f, 0.35f, 1.0f);
        } else {
            draw_billboard_sprite(r, pr->pos, 0.55f, 0.55f, 5, cam_yaw, 1.0f, 0.35f, 0.2f, 1.0f);
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (!w->explosions[i].active) continue;
        float t = w->explosions[i].timer / 0.45f;
        float sz = w->explosions[i].radius * (1.35f - t * 0.5f);
        draw_billboard_sprite(r, vec3_sub(w->explosions[i].pos, vec3_make(0, sz * 0.4f, 0)),
                              sz, sz, 5, cam_yaw, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b, 1.0f);
    }

    /* First-person retro 3D Weapon Viewmodel (flat-screen only) */
    if (viewer_slot >= 0 && viewer_slot < w->player_count && w->scene == SCENE_DAY01) {
        const Player *p = &w->players[viewer_slot];
        if (p->active && p->hp > 0) {
            glClear(GL_DEPTH_BUFFER_BIT);
            Mat4 id_view = mat4_identity();
            glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uView"), 1, GL_FALSE, id_view.m);
            float kick = p->fire_cooldown > 0.0f ? p->fire_cooldown * 0.35f : 0.0f;
            Vec3 wpos = vec3_make(0.26f, -0.24f - kick * 0.15f, -0.52f + kick * 0.25f);
            float wr = (p->weapon == WEAP_SHRINKER) ? 0.25f : (p->weapon == WEAP_PIPEBOMB ? 0.90f : 0.65f);
            float wg = (p->weapon == WEAP_SHRINKER) ? 0.95f : (p->weapon == WEAP_PIPEBOMB ? 0.30f : 0.68f);
            float wb = (p->weapon == WEAP_SHRINKER) ? 0.45f : (p->weapon == WEAP_PIPEBOMB ? 0.20f : 0.74f);
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(wpos), mat4_scale(vec3_make(0.11f, 0.12f, 0.38f))),
                             wr, wg, wb, 1.0f, p->muzzle_flash > 0.0f ? 0.9f : 0.15f);
        }
    }
}

/* Minimal 2D HUD batcher + 5x7 uppercase bitmap font */
typedef struct { float x, y, r, g, b, a; } HudVert;
static HudVert g_hud_buf[16384];
static int g_hud_count = 0;

static void hud_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    if (g_hud_count + 6 > 16384) return;
    HudVert v[6] = {
        {x, y, r, g, b, a}, {x + w, y, r, g, b, a}, {x + w, y + h, r, g, b, a},
        {x, y, r, g, b, a}, {x + w, y + h, r, g, b, a}, {x, y + h, r, g, b, a}
    };
    memcpy(&g_hud_buf[g_hud_count], v, sizeof(v));
    g_hud_count += 6;
}

static const uint8_t *glyph_rows(char c) {
    /* 3x5 ultra-crisp retro pixel font (3 bits per row, 5 rows) */
    static const uint8_t kSpace[5] = {0,0,0,0,0};
    static const uint8_t kA[5]={2,5,7,5,5}, kB[5]={6,5,6,5,6}, kC[5]={3,4,4,4,3}, kD[5]={6,5,5,5,6};
    static const uint8_t kE[5]={7,4,6,4,7}, kF[5]={7,4,6,4,4}, kG[5]={3,4,5,5,3}, kH[5]={5,5,7,5,5};
    static const uint8_t kI[5]={7,2,2,2,7}, kJ[5]={1,1,1,5,2}, kK[5]={5,5,6,5,5}, kL[5]={4,4,4,4,7};
    static const uint8_t kM[5]={5,7,7,5,5}, kN[5]={5,7,7,7,5}, kO[5]={2,5,5,5,2}, kP[5]={6,5,6,4,4};
    static const uint8_t kQ[5]={2,5,5,7,3}, kR[5]={6,5,6,5,5}, kS[5]={3,4,2,1,6}, kT[5]={7,2,2,2,2};
    static const uint8_t kU[5]={5,5,5,5,7}, kV[5]={5,5,5,5,2}, kW[5]={5,5,7,7,5}, kX[5]={5,5,2,5,5};
    static const uint8_t kY[5]={5,5,2,2,2}, kZ[5]={7,1,2,4,7};
    static const uint8_t k0[5]={7,5,5,5,7}, k1[5]={2,6,2,2,7}, k2[5]={7,1,7,4,7}, k3[5]={7,1,7,1,7};
    static const uint8_t k4[5]={5,5,7,1,1}, k5[5]={7,4,7,1,7}, k6[5]={7,4,7,5,7}, k7[5]={7,1,1,2,2};
    static const uint8_t k8[5]={7,5,7,5,7}, k9[5]={7,5,7,1,7};
    static const uint8_t kColon[5]={0,2,0,2,0}, kSlash[5]={1,1,2,4,4}, kDash[5]={0,0,7,0,0}, kExcl[5]={2,2,2,0,2}, kDot[5]={0,0,0,0,2};
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    switch (c) {
        case 'A': return kA; case 'B': return kB; case 'C': return kC; case 'D': return kD;
        case 'E': return kE; case 'F': return kF; case 'G': return kG; case 'H': return kH;
        case 'I': return kI; case 'J': return kJ; case 'K': return kK; case 'L': return kL;
        case 'M': return kM; case 'N': return kN; case 'O': return kO; case 'P': return kP;
        case 'Q': return kQ; case 'R': return kR; case 'S': return kS; case 'T': return kT;
        case 'U': return kU; case 'V': return kV; case 'W': return kW; case 'X': return kX;
        case 'Y': return kY; case 'Z': return kZ;
        case '0': return k0; case '1': return k1; case '2': return k2; case '3': return k3;
        case '4': return k4; case '5': return k5; case '6': return k6; case '7': return k7;
        case '8': return k8; case '9': return k9;
        case ':': return kColon; case '/': return kSlash; case '-': return kDash;
        case '!': return kExcl;  case '.': return kDot;
        default: return kSpace;
    }
}

static void hud_text(float x, float y, float scale_x, float scale_y, const char *s, float r, float g, float b, float a) {
    float cx = x;
    for (; *s; ++s) {
        const uint8_t *rows = glyph_rows(*s);
        for (int ry = 0; ry < 5; ++ry) {
            for (int rx = 0; rx < 3; ++rx) {
                if (rows[ry] & (1u << (2 - rx))) {
                    hud_rect(cx + (float)rx * scale_x, y + (float)ry * scale_y, scale_x, scale_y, r, g, b, a);
                }
            }
        }
        cx += scale_x * 4.0f;
    }
}

static void flush_hud(GlRenderer *r) {
    if (g_hud_count == 0) return;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(r->prog_hud);
    glBindVertexArray(r->hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_hud_count * (int)sizeof(HudVert)), g_hud_buf, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, g_hud_count);
    g_hud_count = 0;
}

static void draw_player_hud(GlRenderer *r, const GameWorld *w, int pi) {
    const Player *p = &w->players[pi];
    g_hud_count = 0;

    /* Damage / stomp flash */
    if (p->damage_flash > 0.0f) {
        hud_rect(0, 0, 1, 1, 0.9f, 0.1f, 0.1f, p->damage_flash * 0.6f);
    }
    if (p->stomp_flash > 0.0f) {
        hud_rect(0, 0, 1, 1, 0.2f, 1.0f, 0.35f, p->stomp_flash * 0.45f);
    }

    /* Crosshair */
    hud_rect(0.495f, 0.485f, 0.010f, 0.030f, 1.0f, 0.95f, 0.3f, 0.85f);
    hud_rect(0.488f, 0.495f, 0.024f, 0.010f, 1.0f, 0.95f, 0.3f, 0.85f);

    /* Top banner */
    if (w->banner_timer > 0.0f && w->banner[0] != '\0') {
        hud_rect(0.04f, 0.03f, 0.92f, 0.075f, 0.04f, 0.04f, 0.09f, 0.78f);
        hud_text(0.06f, 0.05f, 0.0042f, 0.0075f, w->banner, 1.0f, 0.92f, 0.45f, 1.0f);
    }

    /* Nearby Portal prompt in Grand Festive Hall */
    if (w->scene == SCENE_HUB) {
        const PortalGate *near = world_find_nearby_portal(w, p->pos, 4.0f);
        if (near) {
            const PortalMeta *meta = swieta_portal_meta(near->day);
            bool unlocked = swieta_portal_is_unlocked(near->day, w->now, w->unlock_all);
            char tip[128];
            snprintf(tip, sizeof(tip), "DAY %02d: %s - %s [%s]",
                     near->day,
                     meta ? meta->portal_title : "?",
                     meta ? meta->inspiration : "?",
                     unlocked ? "ENTER" : "LOCKED");
            hud_rect(0.10f, 0.13f, 0.80f, 0.065f, 0.05f, 0.07f, 0.14f, 0.82f);
            hud_text(0.12f, 0.148f, 0.0045f, 0.0075f, tip,
                     unlocked ? 0.35f : 0.95f, unlocked ? 1.0f : 0.45f, unlocked ? 0.55f : 0.45f, 1.0f);
        }
    }

    /* Bottom status bar */
    hud_rect(0.02f, 0.88f, 0.96f, 0.10f, 0.05f, 0.05f, 0.10f, 0.80f);
    const char *wname = (p->weapon == WEAP_PISTOL) ? "PISTOL" :
                        (p->weapon == WEAP_SHRINKER ? "SHRINK" : "PIPEBOMB");
    char stats[128];
    snprintf(stats, sizeof(stats), "P%d HP:%d ARM:%d JET:%d  %s SH:%d PB:%d  SCR:%d",
             pi + 1, p->hp, p->armor, (int)p->jetpack_fuel, wname, p->ammo_shrink, p->ammo_pipebomb, p->score);
    hud_text(0.04f, 0.91f, 0.0048f, 0.0085f, stats, 0.95f, 0.96f, 1.0f, 1.0f);

    flush_hud(r);
}

static void draw_touch_overlay(GlRenderer *r, const TouchController *tc) {
    if (!tc || !tc->enabled) return;
    g_hud_count = 0;

    /* Left virtual stick */
    float sx = tc->stick_origin_x;
    float sy = tc->stick_origin_y;
    hud_rect(sx - 0.08f, sy - 0.11f, 0.16f, 0.22f, 0.2f, 0.8f, 1.0f, 0.22f);
    float kx = sx + tc->move_x * 0.06f;
    float ky = sy - tc->move_y * 0.08f;
    hud_rect(kx - 0.03f, ky - 0.04f, 0.06f, 0.08f, 0.4f, 0.95f, 1.0f, 0.65f);

    /* Right action buttons */
    int count = 0;
    const TouchButtonRect *btns = touch_get_layout(&count);
    for (int i = 0; i < count; ++i) {
        bool pressed = (tc->buttons & btns[i].bit) != 0u;
        float bw = btns[i].radius * 1.55f;
        float bh = btns[i].radius * 1.85f;
        hud_rect(btns[i].cx - bw * 0.5f, btns[i].cy - bh * 0.5f, bw, bh,
                 pressed ? 1.0f : 0.95f, pressed ? 0.3f : 0.85f, pressed ? 0.7f : 0.35f,
                 pressed ? 0.68f : 0.30f);
        hud_text(btns[i].cx - bw * 0.36f, btns[i].cy - 0.018f, 0.0045f, 0.0075f,
                 btns[i].label, 1.0f, 1.0f, 1.0f, 0.92f);
    }
    flush_hud(r);
}

void gl_renderer_render_frame(GlRenderer *r, GameWorld *w, const TouchController *touch, int win_w, int win_h) {
    if (!r || !r->initialized || !w || win_w <= 0 || win_h <= 0) return;
    sync_voxel_mesh_if_dirty(r, w);

    Vec3 sun_dir;
    Mat4 light_vp = compute_light_vp(w, &sun_dir);
    render_shadow_pass(r, w, light_vp);

    int pcount = w->player_count;
    if (pcount < 1) pcount = 1;
    if (pcount > 4) pcount = 4;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (int pi = 0; pi < pcount; ++pi) {
        const Player *p = &w->players[pi];
        int vx = 0, vy = 0, vw = win_w, vh = win_h;
        if (pcount == 2) {
            vw = win_w / 2;
            vx = pi * vw;
        } else if (pcount > 2) {
            vw = win_w / 2;
            vh = win_h / 2;
            vx = (pi % 2) * vw;
            vy = (1 - (pi / 2)) * vh;
        }

        /* Pass 2: Render player view into low-res retro FBO */
        glBindFramebuffer(GL_FRAMEBUFFER, r->retro_fbo);
        glViewport(0, 0, r->retro_w, r->retro_h);
        Vec3 eye = player_eye_pos(p);
        Vec3 fwd = player_forward(p);
        float aspect = (float)vw / (float)(vh > 0 ? vh : 1);
        Mat4 proj = mat4_perspective(70.0f * SW_PI / 180.0f, aspect, 0.1f, 90.0f);
        Mat4 view = mat4_look_at(eye, vec3_add(eye, fwd), vec3_make(0, 1, 0));
        render_scene_view(r, w, proj, view, eye, p->yaw, pi, light_vp, sun_dir);

        /* Pass 3: Crisp Nearest-Neighbor Retro Present + Bayer Dither to Player Viewport */
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vx, vy, vw, vh);
        glEnable(GL_SCISSOR_TEST);
        glScissor(vx, vy, vw, vh);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(r->prog_post);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->retro_color_tex);
        glUniform1i(glGetUniformLocation(r->prog_post, "uScene"), 0);
        glUniform2f(glGetUniformLocation(r->prog_post, "uRetroSize"), (float)r->retro_w, (float)r->retro_h);
        glUniform1i(glGetUniformLocation(r->prog_post, "uDither"), r->dither_enabled ? 1 : 0);
        glBindVertexArray(r->quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        /* Pass 4: Crisp HUD per player */
        draw_player_hud(r, w, pi);
        glDisable(GL_SCISSOR_TEST);
    }

    /* Full-screen Adaptive Mobile Touch Overlay (when active) */
    if (touch && touch->enabled) {
        glViewport(0, 0, win_w, win_h);
        draw_touch_overlay(r, touch);
    }
}

void gl_renderer_render_xr_eye(GlRenderer *r, GameWorld *w, const Mat4 *proj, const Mat4 *view,
                               GLuint target_fbo, int vx, int vy, int vw, int vh) {
    if (!r || !r->initialized || !w || !proj || !view) return;
    sync_voxel_mesh_if_dirty(r, w);

    Vec3 sun_dir;
    Mat4 light_vp = compute_light_vp(w, &sun_dir);
    render_shadow_pass(r, w, light_vp);

    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);

    Vec3 eye = player_eye_pos(&w->players[0]);
    render_scene_view(r, w, *proj, *view, eye, w->players[0].yaw, -1, light_vp, sun_dir);
    glDisable(GL_SCISSOR_TEST);
}
