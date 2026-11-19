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
#define SPRITE_TILES 12

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
    "      lit += (proj.z - bias <= d) ? 1.0 : 0.28;\n"
    "    }\n"
    "  }\n"
    "  return lit * 0.25;\n"
    "}\n"
    "void main() {\n"
    "  vec4 baseCol = vColor;\n"
    "  if (uUseTex == 1) {\n"
    "    vec4 texCol = texture(uTex, vUV);\n"
    "    if (texCol.a < 0.30) discard;\n"
    "    baseCol *= texCol;\n"
    "  } else if (uUseTex == 2) {\n"
    "    vec4 texCol = texture(uTex, vUV);\n"
    "    baseCol.rgb *= texCol.rgb;\n"
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
    "      atten = floor(atten * atten * 12.0) / 12.0;\n"
    "      float diff = max(dot(N, toL / max(dist, 0.001)), 0.28);\n"
    "      lighting += uLightColInt[i].rgb * uLightColInt[i].w * diff * atten;\n"
    "    }\n"
    "  }\n"
    "  vec3 litRgb = baseCol.rgb * lighting + baseCol.rgb * vEmissive * 1.15;\n"
    "  float camDist = length(uCamPos - vWorldPos);\n"
    "  float fog = 1.0 - exp(-camDist * uFogDensity);\n"
    "  vec3 finalRgb = mix(litRgb, uFogColor, clamp(fog, 0.0, 0.90));\n"
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

static uint8_t clamp_u8(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

/* Procedural CC BY 4.0 512x64 (16x2 tiles of 32x32) retro pixel-art voxel texture atlas */
static void create_voxel_texture_atlas(GlRenderer *r) {
    const int TW = 32, TH = 32;
    const int W = VOX_ATLAS_COLS * TW; /* 512 */
    const int H = VOX_ATLAS_ROWS * TH; /* 64 */
    uint8_t *px = (uint8_t *)calloc((size_t)(W * H * 4), 1);
    if (!px) return;

    for (int tile = 0; tile < VOX_TYPE_COUNT; ++tile) {
        int tx = (tile % VOX_ATLAS_COLS) * TW;
        int ty = (tile / VOX_ATLAS_COLS) * TH;
        for (int y = 0; y < TH; ++y) {
            for (int x = 0; x < TW; ++x) {
                int h = ((x * 73 ^ y * 151 ^ tile * 37) & 31) - 16;
                int cr = 160, cg = 160, cb = 160;
                switch (tile) {
                    case VOX_STONE: {
                        bool seam = (y % 8 == 0) || ((x + (y / 8) * 8) % 16 == 0);
                        int b = seam ? 72 : (116 + h);
                        cr = b; cg = b + 2; cb = b + 8;
                        break;
                    }
                    case VOX_WOOD: {
                        int plank = (y % 8 == 0) ? -32 : (((x + y * 2) % 7 == 0) ? 10 : 0);
                        cr = 128 + plank + h / 2; cg = 78 + plank + h / 3; cb = 42 + plank / 2;
                        break;
                    }
                    case VOX_SNOW: {
                        int sp = ((x * 19 + y * 37) % 17 == 0) ? 16 : 0;
                        cr = 225 + sp; cg = 236 + sp; cb = 250;
                        break;
                    }
                    case VOX_BRICK: {
                        bool mortar = (y % 8 == 0) || ((x + (y / 8) * 8) % 16 == 0);
                        if (mortar) { cr = 74; cg = 66; cb = 64; }
                        else { cr = 152 + h; cg = 62 + h / 2; cb = 52 + h / 2; }
                        break;
                    }
                    case VOX_CRACKED: {
                        bool crack = (abs(x - y) <= 1 || abs(x + y - 31) <= 1 || abs(x - 16) <= 1) && (x > 3 && x < 28 && y > 3 && y < 28);
                        if (crack) { cr = 28; cg = 16; cb = 14; }
                        else if (abs(x - y) <= 2) { cr = 235; cg = 140; cb = 50; }
                        else { cr = 164 + h; cg = 84 + h / 2; cb = 54; }
                        break;
                    }
                    case VOX_NEON_PINK: {
                        bool bar = (y % 4 == 0);
                        cr = 255; cg = bar ? 160 : 50; cb = bar ? 235 : 195;
                        break;
                    }
                    case VOX_NEON_CYAN: {
                        bool bar = (y % 4 == 0);
                        cr = bar ? 160 : 40; cg = 245; cb = 255;
                        break;
                    }
                    case VOX_GOLD: {
                        bool rim = (x < 3 || x > 28 || y < 3 || y > 28);
                        cr = rim ? 210 : 255; cg = rim ? 165 : 225; cb = rim ? 40 : 85;
                        break;
                    }
                    case VOX_LEAVES: {
                        bool pine = ((x + y * 3) % 5 == 0);
                        cr = pine ? 26 : 42 + h / 2; cg = pine ? 92 : 128 + h; cb = pine ? 38 : 56 + h / 2;
                        break;
                    }
                    case VOX_FIRE: {
                        int glow = (31 - y) * 4;
                        cr = 255; cg = 100 + glow; cb = 20 + glow / 3;
                        break;
                    }
                    case VOX_ASPHALT: {
                        int g = 52 + h;
                        cr = g; cg = g + 2; cb = g + 8;
                        break;
                    }
                    case VOX_METAL: {
                        bool rivet = ((x == 4 || x == 27) && (y == 4 || y == 27));
                        bool border = (x == 0 || y == 0 || x == 31 || y == 31);
                        int g = border ? 75 : (rivet ? 210 : 138 + h / 2);
                        cr = g; cg = g + 8; cb = g + 16;
                        break;
                    }
                    case VOX_PORTAL_FRAME: {
                        bool rune = (x >= 12 && x <= 19 && (y % 6 < 2));
                        cr = rune ? 130 : 78 + h / 2; cg = rune ? 150 : 74 + h / 2; cb = rune ? 225 : 104 + h / 2;
                        break;
                    }
                    case VOX_CARPET: {
                        bool pat = ((x + y) % 8 == 0 || (x - y + 32) % 8 == 0);
                        cr = pat ? 195 : 148 + h / 2; cg = pat ? 58 : 28; cb = pat ? 42 : 34;
                        break;
                    }
                    case VOX_CINEMA_STATIC: {
                        int s = ((x * 97 ^ y * 211) & 1) ? 205 : 70;
                        cr = s; cg = s; cb = s + 12;
                        break;
                    }
                    case VOX_CINEMA_LIT: {
                        bool frame = (x < 2 || x > 29 || y < 2 || y > 29);
                        cr = frame ? 180 : 255; cg = frame ? 130 : 230; cb = frame ? 60 : 125;
                        break;
                    }
                    case VOX_GLASS: {
                        bool frame = (x < 2 || x > 29 || y < 2 || y > 29);
                        bool sheen = ((x + y) % 12 == 0);
                        cr = frame ? 85 : (sheen ? 235 : 155);
                        cg = frame ? 115 : (sheen ? 250 : 225);
                        cb = frame ? 140 : 255;
                        break;
                    }
                    case VOX_MIRROR: {
                        bool border = (x < 3 || x > 28 || y < 3 || y > 28);
                        bool glint = (abs((x + y) - 24) <= 2 || abs((x + y) - 42) <= 1);
                        cr = border ? 140 : (glint ? 255 : 165);
                        cg = border ? 160 : (glint ? 255 : 238);
                        cb = border ? 185 : 255;
                        break;
                    }
                    case VOX_VENT: {
                        bool slat = (y % 4 == 0);
                        int g = slat ? 48 : 138;
                        cr = g; cg = g + 6; cb = g + 12;
                        break;
                    }
                    case VOX_HAZARD: {
                        bool stripe = (((x + y) / 6) & 1) == 0;
                        cr = stripe ? 238 : 34; cg = stripe ? 195 : 34; cb = stripe ? 28 : 38;
                        break;
                    }
                    case VOX_CONCRETE: {
                        bool tile_edge = (x == 0 || y == 0 || x == 16 || y == 16);
                        int g = tile_edge ? 92 : 128 + h / 2;
                        cr = g; cg = g + 2; cb = g + 4;
                        break;
                    }
                    case VOX_ROAD_STRIPE: {
                        bool dash = (x >= 13 && x <= 18 && y >= 6 && y <= 25);
                        cr = dash ? 238 : 54 + h; cg = dash ? 198 : 56 + h; cb = dash ? 48 : 62 + h;
                        break;
                    }
                    case VOX_DOOR_BLUE:
                    case VOX_DOOR_RED:
                    case VOX_DOOR_LOCKED: {
                        bool frame = (x < 3 || x > 28 || y < 3 || y > 28);
                        bool lock = (x >= 11 && x <= 20 && y >= 11 && y <= 20);
                        int b = frame ? 60 : (lock ? 245 : 145);
                        if (tile == VOX_DOOR_BLUE) { cr = b / 3; cg = b * 2 / 3; cb = b; }
                        else if (tile == VOX_DOOR_RED) { cr = b; cg = b / 3; cb = b / 3; }
                        else { cr = b; cg = b * 3 / 4; cb = b / 4; }
                        break;
                    }
                    case VOX_SEAT: {
                        bool stitch = (y == 12 || x == 2 || x == 29);
                        cr = stitch ? 120 : 188; cg = stitch ? 24 : 38; cb = stitch ? 32 : 48;
                        break;
                    }
                    case VOX_GIFT: {
                        bool ribbon = (x >= 13 && x <= 18) || (y >= 13 && y <= 18);
                        cr = ribbon ? 250 : 195; cg = ribbon ? 215 : 36; cb = ribbon ? 55 : 46;
                        break;
                    }
                    case VOX_RELAY_CORE: {
                        bool pulse = ((y / 4) & 1) == 0;
                        cr = pulse ? 255 : 130; cg = pulse ? 55 : 22; cb = pulse ? 95 : 45;
                        break;
                    }
                    case VOX_SANDSTONE: {
                        bool seam = (y % 8 == 0) || ((x + (y / 8) * 10) % 16 == 0);
                        bool glyph = (y >= 11 && y <= 20 && (x == 10 || x == 21 || y == 15));
                        if (seam) { cr = 132; cg = 104; cb = 68; }
                        else if (glyph) { cr = 165; cg = 132; cb = 78; }
                        else { cr = 212 + h / 2; cg = 178 + h / 2; cb = 122 + h / 3; }
                        break;
                    }
                    case VOX_WATER: {
                        bool wave = (((x + y * 2) % 9) < 2);
                        cr = wave ? 120 : 38; cg = wave ? 235 : 165 + h; cb = wave ? 255 : 225;
                        break;
                    }
                    case VOX_LAVA: {
                        bool vein = (((x * 3 + y * 5) ^ (x * y)) & 15) < 4;
                        cr = 255; cg = vein ? 210 : 85 + h; cb = vein ? 65 : 14;
                        break;
                    }
                    case VOX_SLIME: {
                        bool bub = ((x - 10) * (x - 10) + (y - 12) * (y - 12) < 12) ||
                                   ((x - 22) * (x - 22) + (y - 24) * (y - 24) < 9);
                        cr = bub ? 165 : 42; cg = bub ? 255 : 195 + h; cb = bub ? 90 : 36;
                        break;
                    }
                    case VOX_RUST_BRASS: {
                        bool rivet = ((x == 3 || x == 28) && (y == 3 || y == 28));
                        bool seam = (x == 0 || y == 0 || x == 16 || y == 16);
                        int rust = ((x * 13 ^ y * 29) & 7) == 0 ? -24 : 0;
                        if (seam) { cr = 64; cg = 44; cb = 28; }
                        else if (rivet) { cr = 215; cg = 175; cb = 108; }
                        else { cr = 138 + rust + h / 2; cg = 96 + rust / 2 + h / 3; cb = 54 + h / 4; }
                        break;
                    }
                    case VOX_CRATE: {
                        bool frame = (x < 4 || x > 27 || y < 4 || y > 27);
                        bool brace = (abs(x - y) <= 2);
                        int p = (frame || brace) ? 18 : 0;
                        cr = 158 + p + h / 2; cg = 114 + p + h / 3; cb = 64 + p / 2;
                        break;
                    }
                    case VOX_DOOR_YELLOW: {
                        bool frame = (x < 3 || x > 28 || y < 3 || y > 28);
                        bool lock = (x >= 11 && x <= 20 && y >= 11 && y <= 20);
                        int b = frame ? 68 : (lock ? 255 : 185);
                        cr = b; cg = (b * 9) / 10; cb = b / 5;
                        break;
                    }
                    case VOX_RED_CARNIVAL: {
                        bool red = ((x / 8) & 1) == 0;
                        cr = red ? 198 + h / 2 : 228 + h / 3;
                        cg = red ? 32 : 212 + h / 3;
                        cb = red ? 38 : 184;
                        break;
                    }
                    case VOX_CRYPT_STONE: {
                        bool seam = (y % 8 == 0) || ((x + (y / 8) * 6) % 16 == 0);
                        bool moss = (y > 22 && ((x * 7 + y) % 5 == 0));
                        if (seam) { cr = 34; cg = 36; cb = 44; }
                        else if (moss) { cr = 52; cg = 96; cb = 58; }
                        else { cr = 74 + h / 2; cg = 72 + h / 2; cb = 86 + h / 2; }
                        break;
                    }
                    case VOX_TECH_WALL: {
                        bool rib = (x % 8 == 0) || (y == 0 || y == 31 || y == 15);
                        bool led = (y >= 12 && y <= 14 && (x == 4 || x == 20));
                        if (led) { cr = 50; cg = 235; cb = 95; }
                        else if (rib) { cr = 64; cg = 70; cb = 78; }
                        else { cr = 112 + h / 2; cg = 118 + h / 2; cb = 128 + h / 2; }
                        break;
                    }
                    case VOX_PUSH_BLOCK: {
                        bool rim = (x < 4 || x > 27 || y < 4 || y > 27);
                        bool sun = ((x - 16) * (x - 16) + (y - 16) * (y - 16) < 42);
                        if (sun) { cr = 248; cg = 205; cb = 65; }
                        else if (rim) { cr = 148; cg = 112; cb = 52; }
                        else { cr = 192 + h / 2; cg = 152 + h / 2; cb = 86; }
                        break;
                    }
                    case VOX_SPIKES: {
                        bool tip = (x % 8 >= 3 && x % 8 <= 5 && y < 20);
                        cr = tip ? (y < 6 ? 220 : 180) : 45;
                        cg = tip ? (y < 6 ? 35 : 185) : 48;
                        cb = tip ? (y < 6 ? 35 : 195) : 54;
                        break;
                    }
                    case VOX_SLIPGATE: {
                        int dx = x - 16, dy = y - 16;
                        int ring = ((dx * dx + dy * dy) / 14) & 1;
                        cr = ring ? 185 : 112; cg = ring ? 82 : 42; cb = ring ? 255 : 215;
                        break;
                    }
                    default: break;
                }
                int idx = ((ty + y) * W + (tx + x)) * 4;
                px[idx + 0] = clamp_u8(cr);
                px[idx + 1] = clamp_u8(cg);
                px[idx + 2] = clamp_u8(cb);
                px[idx + 3] = 255;
            }
        }
    }

    glGenTextures(1, &r->voxel_atlas_tex);
    glBindTexture(GL_TEXTURE_2D, r->voxel_atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(px);
}

/* Procedural CC BY 4.0 retro 16x16 pixel-art sprite atlas (12 tiles wide = 192x16)
 * 0: Alien Trooper | 1: Pig-Guard | 2: Holo-Decoy | 3: Medkit
 * 4: Orb/Pickup    | 5: Fire/Boom | 6: Golden Shard | 7: Portal Swirl
 * 8: Octabrain     | 9: Turret    | 10: Keycard     | 11: Shotgun
 */
static void create_procedural_sprite_atlas(GlRenderer *r) {
    const int W = SPRITE_TILES * 16, H = 16;
    uint8_t px[SPRITE_TILES * 16 * 16 * 4];
    memset(px, 0, sizeof(px));

    for (int tile = 0; tile < SPRITE_TILES; ++tile) {
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
                        cr = 75; cg = 215; cb = 95; ca = 255;
                        if (y == 2 && (x == 6 || x == 9)) { cr = 255; cg = 40; cb = 40; }
                        if (y >= 6 && y <= 9 && x >= 6 && x <= 9) { cr = 185; cg = 45; cb = 70; }
                    }
                } else if (tile == 1) { /* Boar-Cop Enforcer */
                    if ((y >= 1 && y <= 5 && x >= 4 && x <= 11) ||
                        (y >= 6 && y <= 12 && x >= 3 && x <= 12) ||
                        (y >= 13 && y <= 15 && ((x >= 3 && x <= 6) || (x >= 9 && x <= 12)))) {
                        cr = 215; cg = 138; cb = 142; ca = 255;
                        if (y >= 6 && y <= 11) { cr = 58; cg = 82; cb = 165; }
                        if (y == 3 && (x == 6 || x == 9)) { cr = 255; cg = 235; cb = 55; }
                    }
                } else if (tile == 2) { /* Holo-Decoy */
                    if (((y >= 2 && y <= 14 && x >= 5 && x <= 10) || (y >= 5 && y <= 9 && x >= 3 && x <= 12)) && (y % 2 == 0)) {
                        cr = 70; cg = 240; cb = 255; ca = 225;
                    }
                } else if (tile == 3) { /* Medkit */
                    if (x >= 2 && x <= 13 && y >= 3 && y <= 12) {
                        cr = 238; cg = 238; cb = 244; ca = 255;
                        if ((x >= 7 && x <= 8 && y >= 5 && y <= 10) || (y >= 7 && y <= 8 && x >= 5 && x <= 10)) {
                            cr = 225; cg = 32; cb = 42;
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
                        cr = 255; cg = (uint8_t)(80 + 170 * t); cb = (uint8_t)(20 + 100 * t * t); ca = 255;
                    }
                } else if (tile == 6) { /* Golden Shades / Memory Shard */
                    if (fabsf(u) + fabsf(v) < 0.44f) {
                        cr = 255; cg = 218; cb = 55; ca = 255;
                        if (y == 7 && x >= 5 && x <= 10) { cr = 35; cg = 22; cb = 42; }
                    }
                } else if (tile == 7) { /* Portal Swirl */
                    if (d < 0.48f) {
                        float ang = atan2f(v, u) + d * 10.0f;
                        float sw = 0.5f + 0.5f * sinf(ang * 3.0f);
                        cr = (uint8_t)(150 + 105 * sw); cg = (uint8_t)(170 + 85 * sw); cb = 255;
                        ca = (uint8_t)(220 * (1.0f - d * 1.4f));
                    }
                } else if (tile == 8) { /* Floating Octabrain */
                    bool dome = (y >= 2 && y <= 9 && x >= 3 && x <= 12 && d < 0.45f);
                    bool tent = (y >= 10 && y <= 15 && (x == 4 || x == 7 || x == 9 || x == 12));
                    if (dome || tent) {
                        cr = 175; cg = 72; cb = 215; ca = 255;
                        if (y == 5 && (x == 5 || x == 8 || x == 10)) { cr = 255; cg = 235; cb = 50; }
                    }
                } else if (tile == 9) { /* Automated Laser Turret */
                    if ((y >= 4 && y <= 10 && x >= 4 && x <= 11) || (y >= 11 && y <= 15 && x >= 6 && x <= 9)) {
                        cr = 145; cg = 155; cb = 170; ca = 255;
                        if (y >= 6 && y <= 8 && x >= 6 && x <= 9) { cr = 255; cg = 40; cb = 45; }
                    }
                } else if (tile == 10) { /* Keycard */
                    if (x >= 3 && x <= 12 && y >= 4 && y <= 12) {
                        cr = 245; cg = 245; cb = 255; ca = 255;
                        if (y == 6 || y == 7) { cr = 35; cg = 35; cb = 50; }
                    }
                } else if (tile == 11) { /* Shotgun Pickup */
                    if ((y >= 7 && y <= 9 && x >= 2 && x <= 13) || (y >= 10 && y <= 11 && x >= 2 && x <= 5)) {
                        cr = (x <= 5) ? 155 : 195; cg = (x <= 5) ? 95 : 205; cb = (x <= 5) ? 45 : 220; ca = 255;
                    }
                }

                int idx = (y * W + tile * 16 + x) * 4;
                px[idx + 0] = cr; px[idx + 1] = cg; px[idx + 2] = cb; px[idx + 3] = ca;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r->retro_depth_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
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
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void bind_voxel_vertex_layout(void) {
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(12 * sizeof(float)));
}

static void setup_unit_cube(GlRenderer *r) {
    glGenVertexArrays(1, &r->cube_vao);
    glGenBuffers(1, &r->cube_vbo);
    glBindVertexArray(r->cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->cube_vbo);
    VoxelVertex dummy[36];
    memset(dummy, 0, sizeof(dummy));
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_DYNAMIC_DRAW);
    bind_voxel_vertex_layout();
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

    voxel_mesh_init(&r->scratch_mesh);
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

    create_voxel_texture_atlas(r);
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
    voxel_mesh_free(&r->scratch_mesh);
    for (int i = 0; i < VOX_CHUNK_COUNT; ++i) {
        if (r->chunks[i].vao) {
            glDeleteVertexArrays(1, &r->chunks[i].vao);
            glDeleteBuffers(1, &r->chunks[i].vbo);
            glDeleteBuffers(1, &r->chunks[i].ebo);
        }
    }
    glDeleteProgram(r->prog_shadow);
    glDeleteProgram(r->prog_world);
    glDeleteProgram(r->prog_post);
    glDeleteProgram(r->prog_hud);
    glDeleteFramebuffers(1, &r->shadow_fbo);
    glDeleteFramebuffers(1, &r->retro_fbo);
    glDeleteTextures(1, &r->shadow_tex);
    glDeleteTextures(1, &r->retro_color_tex);
    glDeleteTextures(1, &r->retro_depth_tex);
    glDeleteTextures(1, &r->voxel_atlas_tex);
    glDeleteTextures(1, &r->sprite_atlas_tex);
    glDeleteTextures(1, &r->white_tex);
    glDeleteVertexArrays(1, &r->cube_vao);
    glDeleteBuffers(1, &r->cube_vbo);
    glDeleteVertexArrays(1, &r->quad_vao);
    glDeleteBuffers(1, &r->quad_vbo);
    glDeleteVertexArrays(1, &r->hud_vao);
    glDeleteBuffers(1, &r->hud_vbo);
    r->initialized = false;
}

static void sync_dirty_chunks(GlRenderer *r, GameWorld *w) {
    if (!w->voxels.dirty) return;
    for (int ci = 0; ci < VOX_CHUNK_COUNT; ++ci) {
        if (!w->voxels.chunk_dirty[ci]) continue;
        w->voxels.chunk_dirty[ci] = false;
        if (!voxel_build_chunk_mesh(&w->voxels, ci, &r->scratch_mesh)) continue;

        ChunkGpu *cg = &r->chunks[ci];
        cg->index_count = (GLsizei)r->scratch_mesh.index_count;
        if (cg->index_count == 0) continue;

        if (!cg->vao) {
            glGenVertexArrays(1, &cg->vao);
            glGenBuffers(1, &cg->vbo);
            glGenBuffers(1, &cg->ebo);
            glBindVertexArray(cg->vao);
            glBindBuffer(GL_ARRAY_BUFFER, cg->vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cg->ebo);
            bind_voxel_vertex_layout();
        } else {
            glBindVertexArray(cg->vao);
            glBindBuffer(GL_ARRAY_BUFFER, cg->vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cg->ebo);
        }
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(r->scratch_mesh.vertex_count * sizeof(VoxelVertex)),
                     r->scratch_mesh.vertices, GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(r->scratch_mesh.index_count * sizeof(uint32_t)),
                     r->scratch_mesh.indices, GL_DYNAMIC_DRAW);
        glBindVertexArray(0);
    }
    w->voxels.dirty = false;
}

static void draw_all_chunks(const GlRenderer *r) {
    for (int ci = 0; ci < VOX_CHUNK_COUNT; ++ci) {
        if (r->chunks[ci].index_count > 0 && r->chunks[ci].vao) {
            glBindVertexArray(r->chunks[ci].vao);
            glDrawElements(GL_TRIANGLES, r->chunks[ci].index_count, GL_UNSIGNED_INT, 0);
        }
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
    float u0 = (float)tile / (float)SPRITE_TILES;
    float u1 = (float)(tile + 1) / (float)SPRITE_TILES;
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

static void unbind_all_textures(void) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static Mat4 compute_light_vp(const GameWorld *w, Vec3 focus, Vec3 *out_sun_dir) {
    Vec3 sun_dir = (w->scene == SCENE_DAY01)
        ? vec3_norm(vec3_make(-0.42f, 0.86f, 0.36f))
        : vec3_norm(vec3_make(0.35f, 0.90f, 0.40f));
    if (out_sun_dir) *out_sun_dir = sun_dir;
    Vec3 eye = vec3_add(focus, vec3_scale(sun_dir, 34.0f));
    Mat4 view = mat4_look_at(eye, focus, vec3_make(0, 1, 0));
    Mat4 proj = mat4_ortho(-36.0f, 36.0f, -36.0f, 36.0f, 1.0f, 95.0f);
    return mat4_mul(proj, view);
}

static void render_shadow_pass(GlRenderer *r, const GameWorld *w, Mat4 light_vp) {
    unbind_all_textures();
    glBindFramebuffer(GL_FRAMEBUFFER, r->shadow_fbo);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(r->prog_shadow);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uLightVP"), 1, GL_FALSE, light_vp.m);

    Mat4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, ident.m);
    draw_all_chunks(r);

    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float sc = (e->kind == ENEMY_BOSS_MECH) ? 1.9f : (e->shrunk_timer > 0.0f ? 0.35f : 0.9f);
        Mat4 m = mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, sc * 0.6f, 0))),
                          mat4_scale(vec3_make(sc * 0.7f, sc * 1.2f, sc * 0.7f)));
        glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, m.m);
        glBindVertexArray(r->cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void draw_weapon_viewmodel(GlRenderer *r, const Player *p) {
    glClear(GL_DEPTH_BUFFER_BIT);
    Mat4 id_view = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uView"), 1, GL_FALSE, id_view.m);
    glUniform3f(glGetUniformLocation(r->prog_world, "uCamPos"), 0.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), 0.0f);
    glUniform1i(glGetUniformLocation(r->prog_world, "uLightCount"), 0);
    glUniform3f(glGetUniformLocation(r->prog_world, "uAmbient"), 0.52f, 0.54f, 0.62f);

    float bob_x = sinf(p->walk_bob) * 0.016f;
    float bob_y = fabsf(cosf(p->walk_bob)) * 0.014f;
    float kick = (p->fire_cooldown > 0.0f) ? sw_clampf(p->fire_cooldown * 0.4f, 0.0f, 0.14f) : 0.0f;
    float flash = p->muzzle_flash > 0.0f ? 0.95f : 0.22f;

    /* Mighty Boot Stomp kick animation on left side! */
    if (p->stomp_flash > 0.0f) {
        float kt = p->stomp_flash / 0.32f;
        Vec3 bpos = vec3_make(-0.18f, -0.35f + kt * 0.25f, -0.55f - kt * 0.12f);
        draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(bpos), mat4_scale(vec3_make(0.18f, 0.15f, 0.36f))),
                         0.28f, 0.18f, 0.12f, 1.0f, 0.1f);
    }

    Vec3 base = vec3_make(0.24f + bob_x, -0.24f - bob_y - kick * 0.25f, -0.50f + kick * 0.45f);
    switch (p->weapon) {
        case WEAP_PISTOL:
        case WEAP_USP:
        case WEAP_DEAGLE:
        case WEAP_MAGNUM: {
            float barrel_len = (p->weapon == WEAP_USP) ? 0.42f : 0.34f;
            float cr = (p->weapon == WEAP_MAGNUM || p->weapon == WEAP_DEAGLE) ? 0.80f : 0.62f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.08f, 0.09f, barrel_len))),
                             cr, cr + 0.03f, cr + 0.08f, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, -0.08f, 0.08f))),
                             mat4_scale(vec3_make(0.07f, 0.12f, 0.09f))), 0.22f, 0.22f, 0.25f, 1.0f, 0.08f);
            break;
        }
        case WEAP_DUAL_PISTOLS: {
            /* Iconic Lara twin chrome pistols! */
            Vec3 left = vec3_make(-0.24f - bob_x, base.y, base.z);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.07f, 0.08f, 0.32f))),
                             0.76f, 0.78f, 0.84f, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(left), mat4_scale(vec3_make(0.07f, 0.08f, 0.32f))),
                             0.76f, 0.78f, 0.84f, 1.0f, flash);
            break;
        }
        case WEAP_FLARE: {
            /* Glowing green magnesium flare stick */
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.05f, 0.05f, 0.28f))),
                             0.25f, 0.72f, 0.35f, 1.0f, 0.4f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.01f, -0.15f))),
                             mat4_scale(vec3_make(0.08f, 0.08f, 0.08f))), 0.35f, 1.0f, 0.45f, 1.0f, 1.2f);
            break;
        }
        case WEAP_SHOTGUN:
        case WEAP_SUPER_SHOTGUN: {
            float sc = (p->weapon == WEAP_SUPER_SHOTGUN) ? 1.25f : 1.0f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(-0.03f * sc, 0.02f, -0.08f))),
                             mat4_scale(vec3_make(0.048f * sc, 0.052f, 0.48f))), 0.55f, 0.58f, 0.64f, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0.03f * sc, 0.02f, -0.08f))),
                             mat4_scale(vec3_make(0.048f * sc, 0.052f, 0.48f))), 0.55f, 0.58f, 0.64f, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, -0.05f, 0.12f))),
                             mat4_scale(vec3_make(0.11f, 0.09f, 0.22f))), 0.52f, 0.28f, 0.14f, 1.0f, 0.1f);
            break;
        }
        case WEAP_CARBINE:
        case WEAP_SNIPER: {
            float len = (p->weapon == WEAP_SNIPER) ? 0.62f : 0.46f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.08f, 0.10f, len))),
                             0.26f, 0.30f, 0.28f, 1.0f, flash * 0.7f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.08f, -0.04f))),
                             mat4_scale(vec3_make(0.05f, 0.05f, 0.22f))), 0.16f, 0.18f, 0.22f, 1.0f, 0.15f);
            break;
        }
        case WEAP_FLASH_SMOKE: {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.09f, 0.15f, 0.09f))),
                             0.65f, 0.72f, 0.68f, 1.0f, 0.25f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.08f, 0))),
                             mat4_scale(vec3_make(0.05f, 0.04f, 0.05f))), 0.25f, 0.85f, 1.0f, 1.0f, 0.8f);
            break;
        }
        case WEAP_SHRINKER:
        case WEAP_BFG: {
            float sc = (p->weapon == WEAP_BFG) ? 1.45f : 1.0f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.14f * sc, 0.12f * sc, 0.42f))),
                             0.38f, 0.36f, 0.48f, 1.0f, 0.18f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.06f, -0.12f))),
                             mat4_scale(vec3_make(0.10f * sc, 0.10f * sc, 0.16f))), 0.25f, 1.0f, 0.45f, 1.0f, 1.1f);
            break;
        }
        case WEAP_NAILGUN:
        case WEAP_CHAINGUN: {
            float cr = (p->weapon == WEAP_NAILGUN) ? 0.72f : 0.42f;
            float cg = (p->weapon == WEAP_NAILGUN) ? 0.56f : 0.44f;
            float cb = (p->weapon == WEAP_NAILGUN) ? 0.34f : 0.48f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(-0.04f, 0, -0.06f))),
                             mat4_scale(vec3_make(0.05f, 0.05f, 0.46f))), cr, cg, cb, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0.04f, 0, -0.06f))),
                             mat4_scale(vec3_make(0.05f, 0.05f, 0.46f))), cr, cg, cb, 1.0f, flash);
            break;
        }
        case WEAP_ROCKET: {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.15f, 0.15f, 0.52f))),
                             0.36f, 0.34f, 0.32f, 1.0f, 0.18f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0, -0.25f))),
                             mat4_scale(vec3_make(0.11f, 0.11f, 0.06f))), 0.95f, 0.42f, 0.15f, 1.0f, flash);
            break;
        }
        case WEAP_THUNDERBOLT: {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.12f, 0.10f, 0.44f))),
                             0.64f, 0.48f, 0.30f, 1.0f, 0.2f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.04f, -0.18f))),
                             mat4_scale(vec3_make(0.08f, 0.08f, 0.12f))), 0.35f, 0.92f, 1.0f, 1.0f, 1.2f);
            break;
        }
        case WEAP_PITCHFORK: {
            /* 4-prong crimson-tipped Blood pitchfork! */
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, -0.04f, 0.08f))),
                             mat4_scale(vec3_make(0.05f, 0.05f, 0.40f))), 0.48f, 0.28f, 0.16f, 1.0f, 0.12f);
            for (int k = -1; k <= 1; ++k) {
                draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make((float)k * 0.055f, 0.02f, -0.16f))),
                                 mat4_scale(vec3_make(0.018f, 0.025f, 0.22f))), 0.85f, 0.22f, 0.22f, 1.0f, 0.35f);
            }
            break;
        }
        case WEAP_FLAREGUN: {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.09f, 0.10f, 0.30f))),
                             0.85f, 0.58f, 0.22f, 1.0f, flash);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.02f, -0.15f))),
                             mat4_scale(vec3_make(0.06f, 0.06f, 0.06f))), 1.0f, 0.45f, 0.12f, 1.0f, 1.0f);
            break;
        }
        case WEAP_AEROSOL: {
            /* Red Aerosol can + Zippo flame tip */
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.10f, 0.18f, 0.10f))),
                             0.88f, 0.18f, 0.16f, 1.0f, 0.22f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(-0.08f, 0.06f, -0.10f))),
                             mat4_scale(vec3_make(0.05f, 0.09f, 0.05f))), 1.0f, 0.72f, 0.18f, 1.0f, 1.15f);
            break;
        }
        case WEAP_PIPEBOMB:
        case WEAP_TNT: {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.11f, 0.11f, 0.24f))),
                             0.88f, 0.22f, 0.18f, 1.0f, 0.35f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_make(-0.22f + bob_x, -0.28f - bob_y, -0.46f)),
                             mat4_scale(vec3_make(0.09f, 0.14f, 0.08f))), 0.35f, 0.38f, 0.42f, 1.0f, 0.2f);
            break;
        }
        case WEAP_VOODOO: {
            /* Stitched burlap Voodoo Doll + glowing cursed pin */
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.12f, 0.18f, 0.08f))),
                             0.70f, 0.58f, 0.42f, 1.0f, 0.2f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.12f, 0))),
                             mat4_scale(vec3_make(0.09f, 0.09f, 0.09f))), 0.72f, 0.60f, 0.45f, 1.0f, 0.2f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(-0.08f, 0.02f, -0.05f))),
                             mat4_scale(vec3_make(0.14f, 0.02f, 0.02f))), 0.85f, 0.25f, 1.0f, 1.0f, 1.0f);
            break;
        }
        case WEAP_CHAINSAW: {
            float vib = sinf(p->walk_bob * 4.0f) * 0.008f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(vib, 0, 0.04f))),
                             mat4_scale(vec3_make(0.14f, 0.12f, 0.18f))), 0.92f, 0.74f, 0.14f, 1.0f, 0.25f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(vib, 0.02f, -0.22f))),
                             mat4_scale(vec3_make(0.05f, 0.09f, 0.36f))), 0.72f, 0.75f, 0.80f, 1.0f, 0.30f);
            break;
        }
        default: {
            /* Laser Tripmine emitter */
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(base), mat4_scale(vec3_make(0.14f, 0.09f, 0.18f))),
                             0.25f, 0.28f, 0.34f, 1.0f, 0.15f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(base, vec3_make(0, 0.03f, -0.09f))),
                             mat4_scale(vec3_make(0.05f, 0.05f, 0.05f))), 1.0f, 0.15f, 0.18f, 1.0f, 1.0f);
            break;
        }
    }
}

static void render_scene_view(GlRenderer *r, const GameWorld *w, Mat4 proj, Mat4 view,
                              Vec3 cam_pos, float cam_yaw, int viewer_slot, Mat4 light_vp, Vec3 sun_dir) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Distinct atmospheric sky & lighting mood per Act I realm! */
    float sr = 0.68f, sg = 0.58f, sb = 0.46f;
    float ar = 0.30f, ag = 0.25f, ab = 0.22f;
    float fr = 0.11f, fg = 0.07f, fb = 0.06f, fd = 0.012f;
    if (w->scene == SCENE_DAY01) {
        glClearColor(0.05f, 0.06f, 0.14f, 1.0f);
        sr = 0.56f; sg = 0.62f; sb = 0.86f; ar = 0.26f; ag = 0.26f; ab = 0.34f;
        fr = 0.06f; fg = 0.07f; fb = 0.16f; fd = 0.015f;
    } else if (w->scene == SCENE_DAY02) {
        glClearColor(0.24f, 0.42f, 0.58f, 1.0f); /* Sunlit Peruvian sky */
        sr = 0.88f; sg = 0.78f; sb = 0.58f; ar = 0.28f; ag = 0.24f; ab = 0.20f;
        fr = 0.18f; fg = 0.15f; fb = 0.12f; fd = 0.014f;
    } else if (w->scene == SCENE_DAY03) {
        glClearColor(0.46f, 0.68f, 0.90f, 1.0f); /* Bright sunlit de_yule_dust noon! */
        sr = 0.96f; sg = 0.92f; sb = 0.78f; ar = 0.42f; ag = 0.38f; ab = 0.32f;
        fr = 0.52f; fg = 0.62f; fb = 0.74f; fd = 0.008f;
    } else if (w->scene == SCENE_DAY04) {
        glClearColor(0.10f, 0.07f, 0.06f, 1.0f); /* Gothic Quake rust gloom */
        sr = 0.62f; sg = 0.48f; sb = 0.36f; ar = 0.24f; ag = 0.20f; ab = 0.18f;
        fr = 0.10f; fg = 0.07f; fb = 0.06f; fd = 0.016f;
    } else if (w->scene == SCENE_DAY05) {
        glClearColor(0.08f, 0.04f, 0.10f, 1.0f); /* Foggy Blood midnight crimson */
        sr = 0.55f; sg = 0.32f; sb = 0.40f; ar = 0.22f; ag = 0.16f; ab = 0.22f;
        fr = 0.10f; fg = 0.05f; fb = 0.11f; fd = 0.019f;
    } else if (w->scene == SCENE_DAY06) {
        glClearColor(0.14f, 0.04f, 0.04f, 1.0f); /* Doom Brimstone sky */
        sr = 0.72f; sg = 0.56f; sb = 0.52f; ar = 0.28f; ag = 0.24f; ab = 0.24f;
        fr = 0.12f; fg = 0.05f; fb = 0.05f; fd = 0.014f;
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
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunColor"), sr, sg, sb);
    glUniform3f(glGetUniformLocation(r->prog_world, "uAmbient"),  ar, ag, ab);
    glUniform3f(glGetUniformLocation(r->prog_world, "uFogColor"), fr, fg, fb);
    glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), fd);

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

    /* 1. Draw Chunked Textured Voxel World */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->voxel_atlas_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r->shadow_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uShadowMap"), 1);

    Mat4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, ident.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 2);
    draw_all_chunks(r);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->white_tex);

    /* 2. Portals, Spinning Tree Star, Barrels, Boulders, Flares/Smoke, Tripmines */
    if (w->scene == SCENE_HUB) {
        Mat4 star_m = mat4_mul(mat4_translate(vec3_make(64.5f, 16.0f, 64.5f)),
                      mat4_mul(mat4_rotate_y(w->time_sec * 1.4f), mat4_scale(vec3_make(1.2f, 1.2f, 1.2f))));
        draw_colored_box(r, r->prog_world, star_m, 1.0f, 0.9f, 0.25f, 1.0f, 1.0f);

        for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
            const PortalMeta *meta = swieta_portal_meta(w->portals[i].day);
            bool unlocked = swieta_portal_is_unlocked(w->portals[i].day, w->now, w->unlock_all);
            bool done = (w->shard_mask & (1u << (unsigned)i)) != 0u;
            float pr = unlocked ? (meta ? meta->color_r : 0.4f) : 0.35f;
            float pg = unlocked ? (meta ? meta->color_g : 0.7f) : 0.48f;
            float pb = unlocked ? (meta ? meta->color_b : 1.0f) : 0.62f;
            if (done) { pr = 1.0f; pg = 0.86f; pb = 0.25f; }
            Vec3 p = vec3_add(w->portals[i].pos, vec3_make(0.0f, 2.0f, 0.5f));
            bool x_wall = (w->portals[i].pos.x <= 30.0f || w->portals[i].pos.x >= 98.0f);
            Vec3 sc = x_wall ? vec3_make(0.22f, 3.6f, 2.0f) : vec3_make(2.0f, 3.6f, 0.22f);
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(p), mat4_scale(sc)),
                             pr, pg, pb, unlocked ? 0.88f : 0.52f, unlocked ? 0.90f : 0.15f);
        }
    } else {
        Vec3 rp = vec3_add(w->return_portal_pos, vec3_make(0, 1.4f, 0));
        draw_billboard_sprite(r, rp, 1.9f, 2.8f, 7, cam_yaw, 0.3f, 0.9f, 1.0f, 0.95f);
    }

    /* Explosive Red Nukage / Powder Barrels */
    for (int i = 0; i < w->barrel_count; ++i) {
        if (!w->barrels[i].active) continue;
        Vec3 bp = w->barrels[i].pos;
        draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 0.65f, 0))),
                         mat4_scale(vec3_make(0.75f, 1.25f, 0.75f))), 0.86f, 0.16f, 0.16f, 1.0f, 0.18f);
        draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 1.28f, 0))),
                         mat4_scale(vec3_make(0.65f, 0.08f, 0.65f))), 0.35f, 1.0f, 0.30f, 1.0f, 0.85f);
    }

    /* Rolling Tomb Raider Boulders */
    for (int i = 0; i < w->boulder_count; ++i) {
        if (!w->boulders[i].active) continue;
        float rot = w->boulders[i].rolling ? w->time_sec * 5.0f : 0.0f;
        Mat4 bm = mat4_mul(mat4_translate(w->boulders[i].pos),
                  mat4_mul(mat4_rotate_x(rot), mat4_scale(vec3_make(2.4f, 2.4f, 2.4f))));
        draw_colored_box(r, r->prog_world, bm, 0.62f, 0.56f, 0.46f, 1.0f, 0.05f);
    }

    /* Glowing Green Magnesium Flares & Billowing CS Smoke Clouds */
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        const FlareLight *fl = &w->flares[i];
        if (!fl->active) continue;
        if (fl->is_smoke) {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(fl->pos, vec3_make(0, 1.6f, 0))),
                             mat4_scale(vec3_make(4.6f, 3.6f, 4.6f))), 0.78f, 0.82f, 0.86f, 0.78f, 0.12f);
        } else {
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(fl->pos),
                             mat4_scale(vec3_make(0.18f, 0.18f, 0.42f))), fl->r, fl->g, fl->b, 1.0f, 1.2f);
        }
    }

    /* Quake Thunderbolt live crackling lightning arc! */
    for (int i = 0; i < w->player_count; ++i) {
        if (!w->players[i].active || w->players[i].lightning_timer <= 0.0f) continue;
        Vec3 a = vec3_add(player_eye_pos(&w->players[i]), vec3_scale(player_forward(&w->players[i]), 0.8f));
        Vec3 b = w->players[i].lightning_end;
        for (int s = 1; s <= 8; ++s) {
            Vec3 pt = vec3_lerp(a, b, (float)s / 8.0f);
            float jit = sinf(w->time_sec * 45.0f + (float)s * 2.1f) * 0.14f;
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(vec3_add(pt, vec3_make(jit, jit * 0.6f, 0))),
                             mat4_scale(vec3_make(0.18f, 0.18f, 0.18f))), 0.45f, 0.94f, 1.0f, 0.95f, 1.3f);
        }
    }

    /* Armed Laser Tripmines + Glowing Red Laser Beams */
    for (int i = 0; i < WORLD_MAX_TRIPMINES; ++i) {
        const Tripmine *tm = &w->tripmines[i];
        if (!tm->active) continue;
        draw_colored_box(r, r->prog_world,
                         mat4_mul(mat4_translate(tm->pos), mat4_scale(vec3_make(0.28f, 0.22f, 0.28f))),
                         0.28f, 0.30f, 0.36f, 1.0f, 0.2f);
        if (tm->arm_timer <= 0.0f) {
            Vec3 mid = vec3_lerp(tm->pos, tm->beam_end, 0.5f);
            float blen = vec3_dist(tm->pos, tm->beam_end);
            Vec3 sc = (fabsf(tm->normal.x) > 0.5f) ? vec3_make(blen, 0.05f, 0.05f) :
                      ((fabsf(tm->normal.z) > 0.5f) ? vec3_make(0.05f, 0.05f, blen) : vec3_make(0.05f, blen, 0.05f));
            draw_colored_box(r, r->prog_world, mat4_mul(mat4_translate(mid), mat4_scale(sc)),
                             1.0f, 0.12f, 0.15f, 0.85f, 1.2f);
        }
    }

    /* Co-op Player Avatars */
    for (int i = 0; i < w->player_count; ++i) {
        if (i == viewer_slot || !w->players[i].active || w->players[i].hp <= 0) continue;
        Vec3 pp = w->players[i].pos;
        float sc = (w->players[i].self_shrunk_timer > 0.0f) ? 0.36f : 1.0f;
        float colors[4][3] = {{0.2f,0.7f,1.0f},{1.0f,0.35f,0.35f},{0.35f,0.95f,0.45f},{1.0f,0.85f,0.25f}};
        draw_colored_box(r, r->prog_world,
                         mat4_mul(mat4_translate(vec3_add(pp, vec3_make(0, 0.75f * sc, 0))),
                         mat4_mul(mat4_rotate_y(-w->players[i].yaw), mat4_scale(vec3_make(0.65f * sc, 1.15f * sc, 0.45f * sc)))),
                         colors[i][0], colors[i][1], colors[i][2], 1.0f, 0.1f);
        draw_colored_box(r, r->prog_world,
                         mat4_mul(mat4_translate(vec3_add(pp, vec3_make(0, 1.55f * sc, 0))),
                                  mat4_scale(vec3_make(0.45f * sc, 0.42f * sc, 0.45f * sc))),
                         0.92f, 0.14f, 0.18f, 1.0f, 0.15f);
    }

    /* 3. Enemies & Distinct 3D Act I Bosses */
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float fl = (e->hit_flash > 0.0f) ? 1.0f : 0.0f;
        float burn = (e->burn_timer > 0.0f) ? 0.65f : 0.0f;
        float blind = (e->blind_timer > 0.0f) ? 0.8f : 0.0f;

        if (world_enemy_is_boss(e->kind)) {
            float bob = 0.09f * sinf(e->anim_time * 5.0f);
            float p3 = (w->boss_phase == 3) ? 0.35f : 0.0f;
            float br = 0.78f, bg = 0.16f, bb = 0.20f;
            if (e->kind == ENEMY_BOSS_TREX)       { br = 0.32f; bg = 0.56f; bb = 0.24f; }
            else if (e->kind == ENEMY_BOSS_SHAMBLER)  { br = 0.86f; bg = 0.84f; bb = 0.90f; }
            else if (e->kind == ENEMY_BOSS_MATRIARCH) { br = 0.46f; bg = 0.28f; bb = 0.52f; }
            else if (e->kind == ENEMY_BOSS_CYBERTITAN){ br = 0.72f; bg = 0.36f; bb = 0.26f; }

            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, 1.85f + bob, 0))),
                             mat4_mul(mat4_rotate_y(-e->yaw), mat4_scale(vec3_make(2.0f, 1.65f, 1.55f)))),
                             sw_clampf(br + fl * 0.25f + burn * 0.4f, 0, 1),
                             sw_clampf(bg + fl * 0.4f + burn * 0.2f, 0, 1),
                             sw_clampf(bb + fl * 0.4f, 0, 1), 1.0f, 0.22f + p3 + fl * 0.5f + burn * 0.5f);
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(-0.65f, 0.70f, 0))),
                                      mat4_scale(vec3_make(0.52f, 1.4f, 0.65f))), 0.36f, 0.38f, 0.44f, 1.0f, 0.08f);
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0.65f, 0.70f, 0))),
                                      mat4_scale(vec3_make(0.52f, 1.4f, 0.65f))), 0.36f, 0.38f, 0.44f, 1.0f, 0.08f);
            /* Head / Visor / Horns / Wings accent */
            float er = (e->kind == ENEMY_BOSS_SHAMBLER) ? 0.35f : 1.0f;
            float eg = (e->kind == ENEMY_BOSS_TREX) ? 0.92f : (e->kind == ENEMY_BOSS_SHAMBLER ? 0.90f : 0.22f);
            float eb = (e->kind == ENEMY_BOSS_SHAMBLER) ? 1.0f : 0.18f;
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, 2.55f + bob, 0))),
                                      mat4_scale(vec3_make(e->kind == ENEMY_BOSS_MATRIARCH ? 3.2f : 1.3f, 0.45f, 1.6f))),
                             er, eg, eb, 1.0f, 0.95f);
        } else {
            float sc = (e->shrunk_timer > 0.0f) ? 0.35f : 1.0f;
            int tile = 0;
            float cr = 1.0f, cg = 1.0f, cb = 1.0f;
            switch (e->kind) {
                case ENEMY_PIGGUARD:         tile = 1; break;
                case ENEMY_OCTABRAIN:        tile = 8; break;
                case ENEMY_TURRET:           tile = 9; break;
                case ENEMY_WOLF:             tile = 1; cr = 0.78f; cg = 0.68f; cb = 0.56f; sc *= 0.80f; break;
                case ENEMY_BAT:              tile = 8; cr = 0.62f; cg = 0.42f; cb = 0.78f; sc *= 0.72f; break;
                case ENEMY_STONE_GUARD:      tile = 1; cr = 0.96f; cg = 0.82f; cb = 0.44f; break;
                case ENEMY_SYNDICATE_RIFLE:  tile = 0; cr = 0.78f; cg = 0.76f; cb = 0.64f; break;
                case ENEMY_SYNDICATE_SNIPER: tile = 1; cr = 0.58f; cg = 0.74f; cb = 0.92f; break;
                case ENEMY_OGRE:             tile = 1; cr = 0.92f; cg = 0.72f; cb = 0.46f; break;
                case ENEMY_FIEND:            tile = 0; cr = 0.94f; cg = 0.45f; cb = 0.34f; break;
                case ENEMY_DEATH_KNIGHT:     tile = 1; cr = 0.65f; cg = 0.62f; cb = 0.76f; break;
                case ENEMY_CULTIST:          tile = 0; cr = 0.98f; cg = 0.34f; cb = 0.34f; break;
                case ENEMY_GHOUL:            tile = 1; cr = 0.68f; cg = 0.85f; cb = 0.68f; break;
                case ENEMY_GARGOYLE:         tile = 8; cr = 0.72f; cg = 0.68f; cb = 0.76f; break;
                case ENEMY_IMP:              tile = 0; cr = 0.96f; cg = 0.52f; cb = 0.34f; break;
                case ENEMY_PINKY:            tile = 1; cr = 1.00f; cg = 0.46f; cb = 0.66f; break;
                case ENEMY_CACODEMON:        tile = 8; cr = 1.00f; cg = 0.28f; cb = 0.28f; break;
                default: break;
            }
            if (burn > 0.0f) { cr = 1.0f; cg = 0.55f; cb = 0.18f; }
            if (blind > 0.0f){ cr = 1.0f; cg = 1.0f;  cb = 1.0f;  }
            draw_billboard_sprite(r, e->pos, 1.25f * sc, 1.80f * sc, tile, cam_yaw,
                                  cr, cg, cb, sw_clampf(fl * 0.7f + burn + blind, 0.0f, 1.2f));
        }
    }

    if (w->decoy.active) {
        draw_billboard_sprite(r, w->decoy.pos, 1.1f, 1.7f, 2, cam_yaw, 0.4f, 0.95f, 1.0f, 0.95f);
    }

    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        const Pickup *pk = &w->pickups[i];
        if (!pk->active) continue;
        Vec3 pos = vec3_add(pk->pos, vec3_make(0, 0.16f * sinf(pk->bob_phase), 0));
        int tile = 4;
        float cr = 1.0f, cg = 1.0f, cb = 1.0f;
        switch (pk->kind) {
            case PICKUP_MEDKIT:      tile = 3; break;
            case PICKUP_SHOTGUN:
            case PICKUP_WEAPON_GRANT:tile = 11; cr = 1.0f; cg = 0.92f; cb = 0.58f; break;
            case PICKUP_KEY_BLUE:    tile = 10; cr = 0.25f; cg = 0.65f; cb = 1.0f; break;
            case PICKUP_KEY_YELLOW:  tile = 10; cr = 1.0f;  cg = 0.92f; cb = 0.20f; break;
            case PICKUP_KEY_RED:     tile = 10; cr = 1.0f;  cg = 0.25f; cb = 0.28f; break;
            case PICKUP_SHARD:       tile = 6; break;
            case PICKUP_QUAD:        tile = 6; cr = 0.25f; cg = 0.65f; cb = 1.0f; break;
            case PICKUP_BERSERK:     tile = 3; cr = 1.0f;  cg = 0.20f; cb = 0.22f; break;
            case PICKUP_RADSUIT:     tile = 4; cr = 0.30f; cg = 1.0f;  cb = 0.35f; break;
            case PICKUP_CASH:        tile = 6; cr = 0.35f; cg = 1.0f;  cb = 0.45f; break;
            case PICKUP_PIPEBOMB:    cr = 1.0f; cg = 0.40f; cb = 0.22f; break;
            case PICKUP_TRIPMINE:    cr = 1.0f; cg = 0.20f; cb = 0.30f; break;
            case PICKUP_ARMOR:       cr = 0.35f; cg = 0.85f; cb = 1.0f; break;
            case PICKUP_SHRINK_AMMO: cr = 0.30f; cg = 1.0f; cb = 0.40f; break;
            default: break;
        }
        draw_billboard_sprite(r, pos, 0.85f, 0.85f, tile, cam_yaw, cr, cg, cb, 0.78f);
    }

    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        const Projectile *pr = &w->projectiles[i];
        if (!pr->active) continue;
        if (pr->kind == PROJ_PIPEBOMB || pr->kind == PROJ_TNT_BUNDLE || pr->kind == PROJ_TNT_PROX ||
            pr->kind == PROJ_MOLOTOV || pr->kind == PROJ_FLASHBANG || pr->kind == PROJ_ENEMY_GRENADE) {
            float cr = (pr->kind == PROJ_FLASHBANG) ? 0.85f : 0.95f;
            float cg = (pr->kind == PROJ_FLASHBANG) ? 0.92f : 0.28f;
            float cb = (pr->kind == PROJ_FLASHBANG) ? 1.0f  : 0.18f;
            draw_colored_box(r, r->prog_world,
                             mat4_mul(mat4_translate(pr->pos), mat4_scale(vec3_make(0.28f, 0.20f, 0.42f))),
                             cr, cg, cb, 1.0f, 0.80f);
        } else if (pr->kind == PROJ_SHRINK_ORB || pr->kind == PROJ_BFG_ORB) {
            float sz = (pr->kind == PROJ_BFG_ORB) ? 1.35f : 0.62f;
            draw_billboard_sprite(r, pr->pos, sz, sz, 4, cam_yaw, 0.25f, 1.0f, 0.38f, 1.2f);
        } else if (pr->kind == PROJ_OCTA_WAVE) {
            draw_billboard_sprite(r, pr->pos, 0.70f, 0.70f, 4, cam_yaw, 0.85f, 0.30f, 1.0f, 1.05f);
        } else if (pr->kind == PROJ_NAIL) {
            draw_billboard_sprite(r, pr->pos, 0.32f, 0.32f, 4, cam_yaw, 0.95f, 0.82f, 0.45f, 0.9f);
        } else {
            draw_billboard_sprite(r, pr->pos, 0.60f, 0.60f, 5, cam_yaw, 1.0f, 0.42f, 0.18f, 1.1f);
        }
    }

    for (int i = 0; i < 8; ++i) {
        if (!w->explosions[i].active) continue;
        float t = w->explosions[i].timer / 0.45f;
        float sz = w->explosions[i].radius * (1.35f - t * 0.5f);
        draw_billboard_sprite(r, vec3_sub(w->explosions[i].pos, vec3_make(0, sz * 0.4f, 0)),
                              sz, sz, 5, cam_yaw, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b, 1.1f);
    }

    if (viewer_slot >= 0 && viewer_slot < w->player_count &&
        (w->scene == SCENE_DAY01 || (w->scene >= SCENE_DAY02 && w->scene <= SCENE_DAY06))) {
        const Player *p = &w->players[viewer_slot];
        if (p->active && p->hp > 0 && !p->zoomed) {
            draw_weapon_viewmodel(r, p);
        }
    }
}

/* Minimal 2D HUD batcher + 3x5 retro pixel font */
typedef struct { float x, y, r, g, b, a; } HudVert;
static HudVert g_hud_buf[24576];
static int g_hud_count = 0;

static void hud_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    if (g_hud_count + 6 > 24576) return;
    HudVert v[6] = {
        {x, y, r, g, b, a}, {x + w, y, r, g, b, a}, {x + w, y + h, r, g, b, a},
        {x, y, r, g, b, a}, {x + w, y + h, r, g, b, a}, {x, y + h, r, g, b, a}
    };
    memcpy(&g_hud_buf[g_hud_count], v, sizeof(v));
    g_hud_count += 6;
}

static const uint8_t *glyph_rows(char c) {
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
    static const uint8_t kDollar[5]={7,6,7,3,7};
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
        case '!': return kExcl;  case '.': return kDot;   case '$': return kDollar;
        default: return kSpace;
    }
}

static void hud_text(float x, float y, float px_w, float px_h, const char *s, float r, float g, float b, float a) {
    float cx = x;
    for (; *s; ++s) {
        if (cx > 0.96f) break;
        const uint8_t *rows = glyph_rows(*s);
        for (int ry = 0; ry < 5; ++ry) {
            for (int rx = 0; rx < 3; ++rx) {
                if (rows[ry] & (1u << (2 - rx))) {
                    hud_rect(cx + (float)rx * px_w, y + (float)ry * px_h, px_w, px_h, r, g, b, a);
                }
            }
        }
        cx += px_w * 4.0f;
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

static void draw_minimap(const GameWorld *w, const Player *p, float aspect) {
    float mw = (aspect < 1.0f) ? 0.22f : 0.13f;
    float mh = mw * aspect;
    float mx = 0.025f;
    float my = (aspect < 1.0f) ? 0.145f : 0.19f;

    hud_rect(mx, my, mw, mh, 0.03f, 0.05f, 0.10f, 0.72f);
    hud_rect(mx, my, mw, 0.003f * aspect, 0.2f, 0.85f, 1.0f, 0.7f);
    hud_rect(mx, my + mh - 0.003f * aspect, mw, 0.003f * aspect, 0.2f, 0.85f, 1.0f, 0.7f);

    float cx = mx + mw * 0.5f;
    float cy = my + mh * 0.5f;
    float range = 44.0f;
    float cyaw = cosf(-p->yaw);
    float syaw = sinf(-p->yaw);

    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        if (!w->enemies[i].active) continue;
        float dx = w->enemies[i].pos.x - p->pos.x;
        float dz = w->enemies[i].pos.z - p->pos.z;
        float rx = (dx * cyaw - dz * syaw) / range;
        float rz = (dx * syaw + dz * cyaw) / range;
        if (fabsf(rx) < 0.92f && fabsf(rz) < 0.92f) {
            float sz = world_enemy_is_boss(w->enemies[i].kind) ? 0.011f : 0.006f;
            hud_rect(cx + rx * mw * 0.45f - sz * 0.5f, cy + rz * mh * 0.45f - sz * aspect * 0.5f,
                     sz, sz * aspect, 1.0f, 0.22f, 0.28f, 0.95f);
        }
    }

    float odx = w->objective_pos.x - p->pos.x;
    float odz = w->objective_pos.z - p->pos.z;
    float orx = sw_clampf((odx * cyaw - odz * syaw) / range, -0.90f, 0.90f);
    float orz = sw_clampf((odx * syaw + odz * cyaw) / range, -0.90f, 0.90f);
    hud_rect(cx + orx * mw * 0.45f - 0.005f, cy + orz * mh * 0.45f - 0.005f * aspect,
             0.010f, 0.010f * aspect, 1.0f, 0.88f, 0.22f, 1.0f);

    hud_rect(cx - 0.004f, cy - 0.004f * aspect, 0.008f, 0.008f * aspect, 0.25f, 0.95f, 1.0f, 1.0f);
}

static void draw_player_hud(GlRenderer *r, const GameWorld *w, int pi, int vw, int vh) {
    const Player *p = &w->players[pi];
    g_hud_count = 0;

    float aspect = (float)vw / (float)(vh > 0 ? vh : 1);
    float px_w = (aspect < 1.0f) ? 0.0040f : 0.0042f;
    float px_h = px_w * aspect;
    float ch_w = 0.016f / (aspect < 1.0f ? 1.0f : aspect);
    float ch_h = ch_w * aspect;

    /* Status & Tactical Overlays */
    if (p->in_water)             hud_rect(0, 0, 1, 1, 0.08f, 0.42f, 0.68f, 0.22f);
    if (p->quad_timer > 0.0f)    hud_rect(0, 0, 1, 1, 0.16f, 0.42f, 1.00f, 0.18f);
    if (p->berserk_timer > 0.0f) hud_rect(0, 0, 1, 1, 0.95f, 0.12f, 0.12f, 0.22f);
    if (p->rad_suit_timer > 0.0f)hud_rect(0, 0, 1, 1, 0.15f, 0.88f, 0.25f, 0.14f);
    if (p->damage_flash > 0.0f)  hud_rect(0, 0, 1, 1, 0.9f, 0.1f, 0.1f, p->damage_flash * 0.6f);
    if (p->stomp_flash > 0.0f)   hud_rect(0, 0, 1, 1, 0.2f, 1.0f, 0.35f, p->stomp_flash * 0.45f);
    if (p->flash_blind_timer > 0.0f) hud_rect(0, 0, 1, 1, 1.0f, 1.0f, 1.0f, sw_clampf(p->flash_blind_timer, 0.0f, 0.85f));

    /* CS Sniper Scope Overlay OR Crosshair + Hitmarker */
    if (p->zoomed) {
        hud_rect(0.0f, 0.0f, 0.22f, 1.0f, 0.02f, 0.02f, 0.03f, 0.92f);
        hud_rect(0.78f, 0.0f, 0.22f, 1.0f, 0.02f, 0.02f, 0.03f, 0.92f);
        hud_rect(0.22f, 0.0f, 0.56f, 0.14f, 0.02f, 0.02f, 0.03f, 0.92f);
        hud_rect(0.22f, 0.86f, 0.56f, 0.14f, 0.02f, 0.02f, 0.03f, 0.92f);
        hud_rect(0.499f, 0.14f, 0.002f, 0.72f, 0.15f, 0.95f, 0.35f, 0.85f);
        hud_rect(0.22f, 0.499f, 0.56f, 0.002f, 0.15f, 0.95f, 0.35f, 0.85f);
    } else {
        float cr = 1.0f;
        float cg = p->hitmarker_timer > 0.0f ? 0.25f : 0.95f;
        float cb = p->hitmarker_timer > 0.0f ? 0.25f : 0.30f;
        hud_rect(0.5f - ch_w * 0.15f, 0.5f - ch_h * 0.5f, ch_w * 0.3f, ch_h, cr, cg, cb, 0.92f);
        hud_rect(0.5f - ch_w * 0.5f, 0.5f - ch_h * 0.15f, ch_w, ch_h * 0.3f, cr, cg, cb, 0.92f);
    }

    /* Top objective / banner strip */
    float banner_y = (aspect < 1.0f) ? 0.068f : 0.105f;
    const char *top_msg = (w->banner_timer > 0.0f && w->banner[0] != '\0') ? w->banner : w->objective_text;
    if (top_msg && top_msg[0] != '\0') {
        hud_rect(0.025f, banner_y, 0.95f, px_h * 8.0f, 0.04f, 0.04f, 0.10f, 0.80f);
        hud_text(0.04f, banner_y + px_h * 1.6f, px_w * 0.84f, px_h * 0.84f, top_msg,
                 (w->banner_timer > 0.0f) ? 1.0f : 0.35f,
                 (w->banner_timer > 0.0f) ? 0.92f : 0.95f,
                 (w->banner_timer > 0.0f) ? 0.42f : 1.0f, 1.0f);
    }

    draw_minimap(w, p, aspect);

    /* Nearby Portal prompt in Grand Festive Hall */
    if (w->scene == SCENE_HUB) {
        const PortalGate *near = world_find_nearby_portal(w, p->pos, 5.0f);
        if (near) {
            const PortalMeta *meta = swieta_portal_meta(near->day);
            bool unlocked = swieta_portal_is_unlocked(near->day, w->now, w->unlock_all);
            char tip[128];
            snprintf(tip, sizeof(tip), "DAY %02d: %s [%s]",
                     near->day, meta ? meta->portal_title : "?", unlocked ? "ENTER" : "LOCKED");
            float ty = banner_y + px_h * 9.5f;
            hud_rect(0.28f, ty, 0.68f, px_h * 8.0f, 0.05f, 0.07f, 0.14f, 0.84f);
            hud_text(0.30f, ty + px_h * 1.5f, px_w, px_h, tip,
                     unlocked ? 0.35f : 0.95f, unlocked ? 1.0f : 0.45f, unlocked ? 0.55f : 0.45f, 1.0f);
        }
    }

    /* Bottom status bar with Weapons, Ammo, Keys/Cash & Secrets */
    float bar_h = px_h * 9.2f;
    float bar_y = ((aspect < 1.0f) ? 0.988f : 0.942f) - bar_h;
    hud_rect(0.02f, bar_y, 0.96f, bar_h, 0.05f, 0.05f, 0.11f, 0.84f);

    const char *wname = world_weapon_short_name(p->weapon);
    int cur_ammo = world_player_weapon_ammo(p, p->weapon);
    char keys_str[8] = "---";
    if (p->keys & KEY_BLUE)   keys_str[0] = 'B';
    if (p->keys & KEY_YELLOW) keys_str[1] = 'Y';
    if (p->keys & KEY_RED)    keys_str[2] = 'R';

    char stats[128];
    if (w->scene == SCENE_DAY03) {
        snprintf(stats, sizeof(stats), "HP:%d AR:%d $%d %s:%d C4:%d/2%s",
                 p->hp, p->armor, p->cash, wname, cur_ammo, w->bombs_defused,
                 p->has_defuse_kit ? " KIT" : "");
    } else if (w->scene == SCENE_DAY01) {
        snprintf(stats, sizeof(stats), "HP:%d AR:%d J:%d %s:%d K:%s SEC:%d/%d",
                 p->hp, p->armor, (int)p->jetpack_fuel, wname, cur_ammo,
                 keys_str, w->secrets_found, w->secret_count);
    } else {
        snprintf(stats, sizeof(stats), "HP:%d AR:%d %s:%d K:%s SEC:%d/%d%s",
                 p->hp, p->armor, wname, cur_ammo, keys_str,
                 w->secrets_found, w->secret_count,
                 p->quad_timer > 0.0f ? " QUAD!" : (p->berserk_timer > 0.0f ? " BERSK" : ""));
    }
    hud_text(0.035f, bar_y + px_h * 2.0f, px_w * 0.95f, px_h * 0.95f, stats, 0.96f, 0.97f, 1.0f, 1.0f);

    flush_hud(r);
}

static void draw_touch_overlay(GlRenderer *r, const TouchController *tc, int win_w, int win_h) {
    if (!tc || !tc->enabled) return;
    g_hud_count = 0;

    float aspect = (float)win_w / (float)(win_h > 0 ? win_h : 1);
    float px_w = 0.0040f;
    float px_h = px_w * aspect;

    float sx = tc->stick_origin_x;
    float sy = tc->stick_origin_y;
    float r_w = 0.09f;
    float r_h = r_w * aspect;
    hud_rect(sx - r_w, sy - r_h, r_w * 2.0f, r_h * 2.0f, 0.2f, 0.8f, 1.0f, 0.24f);
    float kx = sx + tc->move_x * r_w * 0.7f;
    float ky = sy - tc->move_y * r_h * 0.7f;
    hud_rect(kx - r_w * 0.35f, ky - r_h * 0.35f, r_w * 0.7f, r_h * 0.7f, 0.4f, 0.95f, 1.0f, 0.70f);

    int count = 0;
    const TouchButtonRect *btns = touch_get_layout(&count);
    for (int i = 0; i < count; ++i) {
        bool pressed = (tc->buttons & btns[i].bit) != 0u;
        float bw = btns[i].radius * 1.45f;
        float bh = bw * aspect;
        hud_rect(btns[i].cx - bw * 0.5f, btns[i].cy - bh * 0.5f, bw, bh,
                 pressed ? 1.0f : 0.95f, pressed ? 0.3f : 0.85f, pressed ? 0.7f : 0.35f,
                 pressed ? 0.70f : 0.32f);
        hud_text(btns[i].cx - bw * 0.38f, btns[i].cy - px_h * 2.5f, px_w, px_h,
                 btns[i].label, 1.0f, 1.0f, 1.0f, 0.95f);
    }
    flush_hud(r);
}

void gl_renderer_render_frame(GlRenderer *r, GameWorld *w, const TouchController *touch, int win_w, int win_h) {
    if (!r || !r->initialized || !w || win_w <= 0 || win_h <= 0) return;
    sync_dirty_chunks(r, w);

    bool portrait = win_h > win_w;
    if ((portrait && r->retro_w > r->retro_h) || (!portrait && r->retro_h > r->retro_w)) {
        gl_renderer_set_retro_res(r, r->retro_h, r->retro_w);
    }

    Vec3 sun_dir;
    Mat4 light_vp = compute_light_vp(w, w->players[0].pos, &sun_dir);
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
            if (portrait) { vh = win_h / 2; vy = (1 - pi) * vh; }
            else { vw = win_w / 2; vx = pi * vw; }
        } else if (pcount > 2) {
            vw = win_w / 2; vh = win_h / 2;
            vx = (pi % 2) * vw; vy = (1 - (pi / 2)) * vh;
        }

        unbind_all_textures();
        glBindFramebuffer(GL_FRAMEBUFFER, r->retro_fbo);
        glViewport(0, 0, r->retro_w, r->retro_h);
        Vec3 eye = player_eye_pos(p);
        Vec3 fwd = player_forward(p);
        float aspect = (float)vw / (float)(vh > 0 ? vh : 1);
        float fovy = p->zoomed ? 28.0f : ((aspect < 1.0f) ? 85.0f : 70.0f);
        Mat4 proj = mat4_perspective(fovy * SW_PI / 180.0f, aspect, 0.1f, 110.0f);
        Mat4 view = mat4_look_at(eye, vec3_add(eye, fwd), vec3_make(0, 1, 0));
        render_scene_view(r, w, proj, view, eye, p->yaw, pi, light_vp, sun_dir);

        unbind_all_textures();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vx, vy, vw, vh);
        glEnable(GL_SCISSOR_TEST);
        glScissor(vx, vy, vw, vh);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUseProgram(r->prog_post);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->retro_color_tex);
        glUniform1i(glGetUniformLocation(r->prog_post, "uScene"), 0);
        glUniform2f(glGetUniformLocation(r->prog_post, "uRetroSize"), (float)r->retro_w, (float)r->retro_h);
        glUniform1i(glGetUniformLocation(r->prog_post, "uDither"), r->dither_enabled ? 1 : 0);
        glBindVertexArray(r->quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        unbind_all_textures();

        draw_player_hud(r, w, pi, vw, vh);
        glDisable(GL_SCISSOR_TEST);
    }

    if (touch && touch->enabled) {
        glViewport(0, 0, win_w, win_h);
        draw_touch_overlay(r, touch, win_w, win_h);
    }
}

void gl_renderer_render_xr_eye(GlRenderer *r, GameWorld *w, const Mat4 *proj, const Mat4 *view,
                               GLuint target_fbo, int vx, int vy, int vw, int vh) {
    if (!r || !r->initialized || !w || !proj || !view) return;
    sync_dirty_chunks(r, w);

    Vec3 sun_dir;
    Mat4 light_vp = compute_light_vp(w, w->players[0].pos, &sun_dir);
    render_shadow_pass(r, w, light_vp);

    unbind_all_textures();
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);

    Vec3 eye = player_eye_pos(&w->players[0]);
    render_scene_view(r, w, *proj, *view, eye, w->players[0].yaw, -1, light_vp, sun_dir);
    unbind_all_textures();
    glDisable(GL_SCISSOR_TEST);
}
