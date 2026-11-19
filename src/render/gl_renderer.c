#include "render/gl_renderer.h"
#include "render/retro_art.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#define GLSL_HEADER "#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n"
#else
#define GLSL_HEADER "#version 330 core\n"
#endif

#define SHADOW_SIZE 1024

/* ============================================================================
 * SHADERS: Procedural Biome Skybox, Shadow Depth, Normal-Mapped Specular World,
 * Filmic Post-Process, and 2D HUD
 * ============================================================================ */

static const char *kSkyVS =
    GLSL_HEADER
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "  vUV = aUV;\n"
    "  gl_Position = vec4(aPos, 0.9999, 1.0);\n"
    "}\n";

static const char *kSkyFS =
    GLSL_HEADER
    "in vec2 vUV;\n"
    "uniform vec3 uCamRight;\n"
    "uniform vec3 uCamUp;\n"
    "uniform vec3 uCamFwd;\n"
    "uniform vec2 uInvProj;\n"
    "uniform int uSceneId;\n"
    "uniform float uTime;\n"
    "out vec4 FragColor;\n"
    "float hash21(vec2 p) {\n"
    "  p = fract(p * vec2(123.34, 456.21));\n"
    "  p += dot(p, p + 45.32);\n"
    "  return fract(p.x * p.y);\n"
    "}\n"
    "float vnoise(vec2 p) {\n"
    "  vec2 i = floor(p);\n"
    "  vec2 f = fract(p);\n"
    "  f = f * f * (3.0 - 2.0 * f);\n"
    "  float a = hash21(i);\n"
    "  float b = hash21(i + vec2(1.0, 0.0));\n"
    "  float c = hash21(i + vec2(0.0, 1.0));\n"
    "  float d = hash21(i + vec2(1.0, 1.0));\n"
    "  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "float fbm(vec2 p) {\n"
    "  float s = 0.0;\n"
    "  float a = 0.5;\n"
    "  for (int i = 0; i < 4; ++i) {\n"
    "    s += vnoise(p) * a;\n"
    "    p = p * 2.03 + vec2(17.1, 31.7);\n"
    "    a *= 0.5;\n"
    "  }\n"
    "  return s;\n"
    "}\n"
    "void main() {\n"
    "  vec2 ndc = vUV * 2.0 - 1.0;\n"
    "  vec3 dir = normalize(uCamFwd + uCamRight * (ndc.x * uInvProj.x) + uCamUp * (ndc.y * uInvProj.y));\n"
    "  float y = dir.y;\n"
    "  float az = atan(dir.z, dir.x);\n"
    "  vec2 dome = dir.xz / (abs(y) + 0.34);\n"
    "  vec3 col = vec3(0.05, 0.06, 0.12);\n"
    "\n"
    "  if (uSceneId == 1) { /* Day 1: Duke Nukem 3D Neon LA Night Skyline */\n"
    "    vec3 horiz = vec3(0.24, 0.09, 0.30);\n"
    "    vec3 mid   = vec3(0.06, 0.07, 0.22);\n"
    "    vec3 zen   = vec3(0.02, 0.03, 0.09);\n"
    "    col = mix(horiz, mid, smoothstep(-0.04, 0.26, y));\n"
    "    col = mix(col, zen, smoothstep(0.24, 0.78, y));\n"
    "    float clouds = fbm(dome * 1.1 + vec2(uTime * 0.015, 0.0));\n"
    "    col += vec3(0.18, 0.06, 0.22) * smoothstep(0.45, 0.85, clouds) * smoothstep(0.0, 0.3, y);\n"
    "    if (y > 0.04) {\n"
    "      vec2 sp = vec2(az * 92.0, y * 118.0);\n"
    "      vec2 sg = floor(sp);\n"
    "      vec2 sf = fract(sp) - 0.5;\n"
    "      float st = hash21(sg);\n"
    "      if (st > 0.965) {\n"
    "        float dotS = smoothstep(0.18, 0.02, length(sf));\n"
    "        float tw = 0.65 + 0.35 * sin(uTime * 2.5 + st * 90.0);\n"
    "        col += vec3(0.88, 0.94, 1.0) * dotS * tw * smoothstep(0.04, 0.25, y);\n"
    "      }\n"
    "    }\n"
    "    /* Sweeping LA Searchlight Beams */\n"
    "    float beam1 = abs(sin(az * 2.0 + sin(uTime * 0.65) * 0.85));\n"
    "    col += vec3(0.16, 0.26, 0.38) * smoothstep(0.08, 0.0, beam1) * smoothstep(0.0, 0.45, y) * (1.0 - smoothstep(0.45, 0.9, y));\n"
    "    /* Distant Moon */\n"
    "    vec3 moonDir = normalize(vec3(-0.45, 0.42, -0.78));\n"
    "    float md = dot(dir, moonDir);\n"
    "    col += vec3(0.95, 0.94, 0.82) * smoothstep(0.9965, 0.9985, md);\n"
    "    col += vec3(0.24, 0.28, 0.46) * pow(max(md, 0.0), 48.0) * 0.55;\n"
    "    /* Silhouetted City Skyline with Lit Neon/Gold Windows */\n"
    "    float bcol = floor((az + 3.14159) * 14.0);\n"
    "    float bh = 0.09 + hash21(vec2(bcol, 3.0)) * 0.21;\n"
    "    if (y < bh) {\n"
    "      vec3 bld = vec3(0.03, 0.03, 0.07);\n"
    "      float wx = fract((az + 3.14159) * 14.0);\n"
    "      float wy = fract(y * 78.0);\n"
    "      if (y > 0.005 && wx > 0.16 && wx < 0.84 && wy > 0.28 && wy < 0.72) {\n"
    "        float wseed = hash21(vec2(bcol * 7.0 + floor(wx * 4.0), floor(y * 78.0)));\n"
    "        if (wseed > 0.62) {\n"
    "          vec3 wc = (wseed > 0.88) ? vec3(1.0, 0.28, 0.78) : ((wseed > 0.76) ? vec3(0.25, 0.92, 1.0) : vec3(1.0, 0.84, 0.42));\n"
    "          bld = mix(bld, wc, 0.82);\n"
    "        }\n"
    "      }\n"
    "      col = bld;\n"
    "    }\n"
    "  } else if (uSceneId == 3) { /* Day 2: Tomb Raider Sunlit Peruvian Andean Peaks */\n"
    "    vec3 horiz = vec3(0.76, 0.72, 0.58);\n"
    "    vec3 mid   = vec3(0.28, 0.56, 0.82);\n"
    "    vec3 zen   = vec3(0.11, 0.28, 0.58);\n"
    "    col = mix(horiz, mid, smoothstep(-0.02, 0.25, y));\n"
    "    col = mix(col, zen, smoothstep(0.22, 0.82, y));\n"
    "    vec3 sunDir = normalize(vec3(0.42, 0.48, -0.76));\n"
    "    float sd = max(dot(dir, sunDir), 0.0);\n"
    "    col += vec3(1.0, 0.88, 0.58) * pow(sd, 12.0) * 0.42;\n"
    "    col += vec3(1.0, 0.98, 0.88) * smoothstep(0.994, 0.998, sd);\n"
    "    float cl = fbm(dome * 0.95 + vec2(uTime * 0.012, -uTime * 0.008));\n"
    "    col = mix(col, vec3(0.96, 0.94, 0.88), smoothstep(0.52, 0.82, cl) * smoothstep(0.05, 0.30, y) * 0.65);\n"
    "    float mtn = 0.04 + 0.14 * fbm(vec2(az * 3.2 + 2.0, 1.5));\n"
    "    if (y < mtn) {\n"
    "      float snowCap = smoothstep(mtn - 0.032, mtn - 0.006, y);\n"
    "      vec3 rock = mix(vec3(0.18, 0.16, 0.20), vec3(0.32, 0.28, 0.26), smoothstep(0.0, mtn, y));\n"
    "      col = mix(rock, vec3(0.88, 0.90, 0.94), snowCap * 0.85);\n"
    "    }\n"
    "  } else if (uSceneId == 4) { /* Day 3: Counter-Strike de_yule_dust Desert Noon */\n"
    "    vec3 horiz = vec3(0.86, 0.80, 0.66);\n"
    "    vec3 mid   = vec3(0.42, 0.68, 0.92);\n"
    "    vec3 zen   = vec3(0.18, 0.46, 0.82);\n"
    "    col = mix(horiz, mid, smoothstep(-0.02, 0.26, y));\n"
    "    col = mix(col, zen, smoothstep(0.24, 0.80, y));\n"
    "    vec3 sunDir = normalize(vec3(0.35, 0.65, 0.66));\n"
    "    float sd = max(dot(dir, sunDir), 0.0);\n"
    "    col += vec3(1.0, 0.94, 0.78) * pow(sd, 16.0) * 0.35;\n"
    "    col += vec3(1.0, 0.99, 0.92) * smoothstep(0.995, 0.9985, sd);\n"
    "    float cl = fbm(dome * 0.75 + vec2(uTime * 0.010, 0.0));\n"
    "    col = mix(col, vec3(0.98, 0.96, 0.90), smoothstep(0.55, 0.86, cl) * smoothstep(0.08, 0.32, y) * 0.55);\n"
    "    float dune = 0.015 + 0.055 * fbm(vec2(az * 2.5, 4.1));\n"
    "    if (y < dune) col = mix(vec3(0.66, 0.54, 0.38), vec3(0.78, 0.66, 0.48), smoothstep(-0.05, dune, y));\n"
    "  } else if (uSceneId == 5) { /* Day 4: Quake Dual-Layer Scrolling Storm Sky */\n"
    "    vec2 p1 = dome * 1.15 + vec2(uTime * 0.045, uTime * 0.018);\n"
    "    vec2 p2 = dome * 2.10 + vec2(-uTime * 0.070, uTime * 0.032);\n"
    "    float c1 = fbm(p1);\n"
    "    float c2 = fbm(p2);\n"
    "    vec3 baseSky = mix(vec3(0.12, 0.07, 0.14), vec3(0.28, 0.16, 0.24), c1);\n"
    "    vec3 upper   = mix(vec3(0.06, 0.04, 0.09), vec3(0.44, 0.28, 0.18), smoothstep(0.38, 0.82, c2));\n"
    "    col = mix(baseSky, upper, 0.55);\n"
    "    float flash = pow(max(sin(uTime * 1.7) * sin(uTime * 4.3), 0.0), 18.0);\n"
    "    col += vec3(0.45, 0.35, 0.68) * flash * smoothstep(0.45, 0.85, c2);\n"
    "    col = mix(vec3(0.18, 0.10, 0.08), col, smoothstep(-0.04, 0.22, y));\n"
    "  } else if (uSceneId == 6) { /* Day 5: Blood Harvest Crimson Moon & Gothic Fog */\n"
    "    vec3 horiz = vec3(0.24, 0.06, 0.16);\n"
    "    vec3 mid   = vec3(0.10, 0.04, 0.15);\n"
    "    vec3 zen   = vec3(0.03, 0.02, 0.07);\n"
    "    col = mix(horiz, mid, smoothstep(-0.03, 0.24, y));\n"
    "    col = mix(col, zen, smoothstep(0.22, 0.78, y));\n"
    "    vec3 moonDir = normalize(vec3(0.25, 0.32, -0.91));\n"
    "    float md = max(dot(dir, moonDir), 0.0);\n"
    "    float crater = 0.82 + 0.18 * fbm(dir.xy * 18.0);\n"
    "    col += vec3(0.88, 0.12, 0.14) * pow(md, 14.0) * 0.62;\n"
    "    col = mix(col, vec3(1.0, 0.26, 0.18) * crater, smoothstep(0.986, 0.991, md));\n"
    "    float mist = fbm(dome * 1.3 + vec2(uTime * 0.03, 0.0));\n"
    "    col = mix(col, vec3(0.18, 0.06, 0.18), smoothstep(0.48, 0.84, mist) * 0.55);\n"
    "    float spire = 0.03 + 0.08 * pow(fbm(vec2(az * 6.0, 2.0)), 1.6);\n"
    "    if (y < spire) col = vec3(0.03, 0.02, 0.04);\n"
    "  } else if (uSceneId == 7) { /* Day 6: Doom Phobos Hellfire Brimstone Sky */\n"
    "    vec2 p1 = dome * 1.2 + vec2(uTime * 0.035, -uTime * 0.02);\n"
    "    float cl = fbm(p1);\n"
    "    vec3 horiz = vec3(0.82, 0.26, 0.08);\n"
    "    vec3 mid   = vec3(0.36, 0.08, 0.06);\n"
    "    vec3 zen   = vec3(0.12, 0.03, 0.04);\n"
    "    col = mix(horiz, mid, smoothstep(-0.02, 0.28, y));\n"
    "    col = mix(col, zen, smoothstep(0.26, 0.80, y));\n"
    "    col += vec3(0.52, 0.18, 0.04) * smoothstep(0.42, 0.84, cl) * (1.0 - smoothstep(0.25, 0.85, y));\n"
    "    float peak = 0.03 + 0.16 * pow(fbm(vec2(az * 4.5 + 1.0, 7.2)), 1.3);\n"
    "    if (y < peak) {\n"
    "      float rim = smoothstep(peak - 0.018, peak, y);\n"
    "      col = mix(vec3(0.08, 0.03, 0.03), vec3(0.68, 0.22, 0.06), rim * 0.65);\n"
    "    }\n"
    "  } else { /* Grand Festive Hall & Preview: Starry Winter Night + Aurora */\n"
    "    vec3 horiz = vec3(0.10, 0.08, 0.18);\n"
    "    vec3 zen   = vec3(0.02, 0.03, 0.09);\n"
    "    col = mix(horiz, zen, smoothstep(0.0, 0.72, y));\n"
    "    float aur = sin(az * 3.0 + uTime * 0.35 + fbm(dome * 1.5) * 4.0) * 0.5 + 0.5;\n"
    "    col += mix(vec3(0.08, 0.46, 0.38), vec3(0.46, 0.34, 0.12), aur) * pow(aur, 3.0) * smoothstep(0.08, 0.45, y) * (1.0 - smoothstep(0.45, 0.85, y)) * 0.55;\n"
    "    if (y > 0.02) {\n"
    "      vec2 sp = vec2(az * 96.0, y * 120.0);\n"
    "      vec2 sg = floor(sp);\n"
    "      vec2 sf = fract(sp) - 0.5;\n"
    "      float st = hash21(sg);\n"
    "      if (st > 0.960) {\n"
    "        float dotS = smoothstep(0.18, 0.02, length(sf));\n"
    "        col += vec3(1.0, 0.96, 0.84) * dotS * (0.6 + 0.4 * sin(uTime * 3.0 + st * 80.0));\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "  FragColor = vec4(col, 1.0);\n"
    "}\n";

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
    "  mat3 nMat = transpose(inverse(mat3(uModel)));\n"
    "  vNormal = normalize(nMat * aNormal);\n"
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
    "uniform sampler2D uNormalMap;\n"
    "uniform vec3 uCamPos;\n"
    "uniform vec3 uSunDir;\n"
    "uniform vec3 uSunColor;\n"
    "uniform vec3 uSkyAmbient;\n"
    "uniform vec3 uGroundAmbient;\n"
    "uniform vec3 uFogColor;\n"
    "uniform float uFogDensity;\n"
    "uniform int uLightCount;\n"
    "uniform vec4 uLightPosRad[12];\n"
    "uniform vec4 uLightColInt[12];\n"
    "uniform int uUseTex;\n"
    "uniform vec4 uTint;\n"
    "uniform float uMeshEmissive;\n"
    "uniform float uMeshGloss;\n"
    "out vec4 FragColor;\n"
    "float sampleShadow3x3(vec4 sp, float ndotl) {\n"
    "  vec3 proj = sp.xyz / max(sp.w, 0.0001);\n"
    "  proj = proj * 0.5 + 0.5;\n"
    "  if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;\n"
    "  float bias = max(0.0032 * (1.0 - ndotl), 0.0014);\n"
    "  float texel = 1.0 / 1024.0;\n"
    "  float lit = 0.0;\n"
    "  for (int x = -1; x <= 1; ++x) {\n"
    "    for (int y = -1; y <= 1; ++y) {\n"
    "      float d = texture(uShadowMap, proj.xy + vec2(float(x), float(y)) * texel).r;\n"
    "      lit += (proj.z - bias <= d) ? 1.0 : 0.24;\n"
    "    }\n"
    "  }\n"
    "  return lit / 9.0;\n"
    "}\n"
    "vec3 acesTone(vec3 x) {\n"
    "  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);\n"
    "}\n"
    "void main() {\n"
    "  vec4 baseCol = vColor;\n"
    "  vec3 gN = normalize(vNormal);\n"
    "  vec3 N = gN;\n"
    "  float gloss = uMeshGloss;\n"
    "  float em = vEmissive;\n"
    "  if (uUseTex == 0) {\n"
    "    baseCol *= uTint;\n"
    "    em += uMeshEmissive;\n"
    "  } else if (uUseTex == 1) {\n"
    "    vec4 texCol = texture(uTex, vUV);\n"
    "    if (texCol.a < 0.35) discard;\n"
    "    baseCol *= texCol;\n"
    "    gloss = 0.18;\n"
    "  } else if (uUseTex == 2) {\n"
    "    vec4 texCol = texture(uTex, vUV);\n"
    "    baseCol.rgb *= texCol.rgb;\n"
    "    vec4 nm = texture(uNormalMap, vUV);\n"
    "    vec3 tn = nm.xyz * 2.0 - 1.0;\n"
    "    gloss = nm.a;\n"
    "    vec3 T, B;\n"
    "    if (abs(gN.y) < 0.5) {\n"
    "      B = vec3(0.0, -1.0, 0.0);\n"
    "      T = normalize(cross(gN, B));\n"
    "    } else {\n"
    "      T = vec3(1.0, 0.0, 0.0);\n"
    "      B = vec3(0.0, 0.0, sign(gN.y));\n"
    "    }\n"
    "    N = normalize(T * tn.x + B * tn.y + gN * tn.z);\n"
    "  }\n"
    "  vec3 V = normalize(uCamPos - vWorldPos);\n"
    "  vec3 Lsun = normalize(uSunDir);\n"
    "  float gNdotL = max(dot(gN, Lsun), 0.0);\n"
    "  float ndotl  = max(dot(N, Lsun), 0.0);\n"
    "  float sh = sampleShadow3x3(vShadowPos, gNdotL);\n"
    "  float hemi = N.y * 0.5 + 0.5;\n"
    "  vec3 diffuseLight = mix(uGroundAmbient, uSkyAmbient, hemi) + uSunColor * ndotl * sh;\n"
    "  float specPow = mix(10.0, 64.0, gloss);\n"
    "  vec3 Hsun = normalize(Lsun + V);\n"
    "  vec3 specLight = uSunColor * (pow(max(dot(N, Hsun), 0.0), specPow) * gloss * 0.85 * sh * step(0.01, gNdotL));\n"
    "  for (int i = 0; i < 12; ++i) {\n"
    "    if (i >= uLightCount) break;\n"
    "    vec3 toL = uLightPosRad[i].xyz - vWorldPos;\n"
    "    float dist = length(toL);\n"
    "    float rad = uLightPosRad[i].w;\n"
    "    if (dist < rad) {\n"
    "      float atten = 1.0 - (dist / rad);\n"
    "      atten = atten * atten;\n"
    "      vec3 L = toL / max(dist, 0.001);\n"
    "      float diff = max(dot(N, L), 0.0) * 0.82 + 0.18;\n"
    "      vec3 lcol = uLightColInt[i].rgb * uLightColInt[i].w * atten;\n"
    "      diffuseLight += lcol * diff;\n"
    "      vec3 H = normalize(L + V);\n"
    "      specLight += lcol * (pow(max(dot(N, H), 0.0), specPow) * gloss * 0.90);\n"
    "    }\n"
    "  }\n"
    "  /* Subtle Fresnel rim sheen on glossy metals, glass & water */\n"
    "  float fres = pow(1.0 - max(dot(N, V), 0.0), 3.0) * gloss * 0.22;\n"
    "  vec3 litRgb = baseCol.rgb * diffuseLight + specLight + uSkyAmbient * fres + baseCol.rgb * em * 1.18;\n"
    "  float camDist = length(uCamPos - vWorldPos);\n"
    "  float fog = 1.0 - exp(-camDist * uFogDensity);\n"
    "  vec3 fogged = mix(litRgb, uFogColor, clamp(fog, 0.0, 0.88));\n"
    "  FragColor = vec4(acesTone(fogged * 1.22), baseCol.a);\n"
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
    "  /* Gentle cinematic edge vignette */\n"
    "  vec2 c = vUV * 2.0 - 1.0;\n"
    "  col *= 1.0 - 0.10 * dot(c, c);\n"
    "  /* Classic DOS 5-bit Bayer dithering only when playing in low-res <= 480w mode */\n"
    "  if (uDither == 1 && max(uRetroSize.x, uRetroSize.y) <= 480.5) {\n"
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

static Mat4 mat4_rotate_z_local(float rad) {
    Mat4 r = mat4_identity();
    float c = cosf(rad);
    float s = sinf(rad);
    r.m[0] = c;  r.m[4] = -s;
    r.m[1] = s;  r.m[5] = c;
    return r;
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
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
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static void bind_voxel_vertex_layout(void) {
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(VoxelVertex), (void *)(12 * sizeof(float)));
}

static VoxelVertex make_mesh_vert(float px, float py, float pz, float nx, float ny, float nz, float u, float v) {
    VoxelVertex vv;
    vv.pos[0] = px; vv.pos[1] = py; vv.pos[2] = pz;
    vv.normal[0] = nx; vv.normal[1] = ny; vv.normal[2] = nz;
    vv.uv[0] = u; vv.uv[1] = v;
    vv.color[0] = 1.0f; vv.color[1] = 1.0f; vv.color[2] = 1.0f; vv.color[3] = 1.0f;
    vv.emissive = 0.0f;
    return vv;
}

static void upload_static_mesh(MeshGpu *dst, const VoxelVertex *verts, int count) {
    glGenVertexArrays(1, &dst->vao);
    glGenBuffers(1, &dst->vbo);
    glBindVertexArray(dst->vao);
    glBindBuffer(GL_ARRAY_BUFFER, dst->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)count * sizeof(VoxelVertex)), verts, GL_STATIC_DRAW);
    bind_voxel_vertex_layout();
    glBindVertexArray(0);
    dst->vert_count = count;
}

static void init_mesh_library(GlRenderer *r) {
    /* 1. Unit Box [-0.5..0.5] */
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
    VoxelVertex box_v[36];
    for (int i = 0; i < 36; ++i) {
        box_v[i] = make_mesh_vert(c[i][0], c[i][1], c[i][2], c[i][3], c[i][4], c[i][5], c[i][6], c[i][7]);
    }
    upload_static_mesh(&r->mesh_box, box_v, 36);

    /* 2. Smooth 16-sided Capped Cylinder (radius 0.5, height [-0.5..0.5] along Y) */
    const int CSEG = 16;
    VoxelVertex *cyl_v = (VoxelVertex *)calloc((size_t)(CSEG * 12), sizeof(VoxelVertex));
    if (cyl_v) {
        int ci = 0;
        for (int i = 0; i < CSEG; ++i) {
            float a0 = ((float)i / (float)CSEG) * 2.0f * SW_PI;
            float a1 = ((float)(i + 1) / (float)CSEG) * 2.0f * SW_PI;
            float c0 = cosf(a0), s0 = sinf(a0);
            float c1 = cosf(a1), s1 = sinf(a1);
            float x0 = c0 * 0.5f, z0 = s0 * 0.5f;
            float x1 = c1 * 0.5f, z1 = s1 * 0.5f;
            /* Side quad */
            cyl_v[ci++] = make_mesh_vert(x0, -0.5f, z0, c0, 0, s0, 0, 0);
            cyl_v[ci++] = make_mesh_vert(x1, -0.5f, z1, c1, 0, s1, 1, 0);
            cyl_v[ci++] = make_mesh_vert(x1,  0.5f, z1, c1, 0, s1, 1, 1);
            cyl_v[ci++] = make_mesh_vert(x0, -0.5f, z0, c0, 0, s0, 0, 0);
            cyl_v[ci++] = make_mesh_vert(x1,  0.5f, z1, c1, 0, s1, 1, 1);
            cyl_v[ci++] = make_mesh_vert(x0,  0.5f, z0, c0, 0, s0, 0, 1);
            /* Top cap */
            cyl_v[ci++] = make_mesh_vert(0,  0.5f, 0, 0, 1, 0, 0.5f, 0.5f);
            cyl_v[ci++] = make_mesh_vert(x1, 0.5f, z1, 0, 1, 0, 1, 1);
            cyl_v[ci++] = make_mesh_vert(x0, 0.5f, z0, 0, 1, 0, 0, 1);
            /* Bottom cap */
            cyl_v[ci++] = make_mesh_vert(0, -0.5f, 0, 0, -1, 0, 0.5f, 0.5f);
            cyl_v[ci++] = make_mesh_vert(x0, -0.5f, z0, 0, -1, 0, 0, 0);
            cyl_v[ci++] = make_mesh_vert(x1, -0.5f, z1, 0, -1, 0, 1, 0);
        }
        upload_static_mesh(&r->mesh_cyl, cyl_v, ci);
        free(cyl_v);
    }

    /* 3. Smooth 14x10 UV Sphere (radius 0.5) */
    const int SLICE = 14, STACK = 10;
    VoxelVertex *sph_v = (VoxelVertex *)calloc((size_t)(SLICE * STACK * 6), sizeof(VoxelVertex));
    if (sph_v) {
        int si = 0;
        for (int st = 0; st < STACK; ++st) {
            float t0 = -SW_PI * 0.5f + ((float)st / (float)STACK) * SW_PI;
            float t1 = -SW_PI * 0.5f + ((float)(st + 1) / (float)STACK) * SW_PI;
            float ct0 = cosf(t0), st0 = sinf(t0);
            float ct1 = cosf(t1), st1 = sinf(t1);
            for (int sl = 0; sl < SLICE; ++sl) {
                float p0 = ((float)sl / (float)SLICE) * 2.0f * SW_PI;
                float p1 = ((float)(sl + 1) / (float)SLICE) * 2.0f * SW_PI;
                float cp0 = cosf(p0), sp0 = sinf(p0);
                float cp1 = cosf(p1), sp1 = sinf(p1);
                float n00[3] = {ct0 * cp0, st0, ct0 * sp0};
                float n10[3] = {ct0 * cp1, st0, ct0 * sp1};
                float n11[3] = {ct1 * cp1, st1, ct1 * sp1};
                float n01[3] = {ct1 * cp0, st1, ct1 * sp0};
                sph_v[si++] = make_mesh_vert(n00[0]*0.5f, n00[1]*0.5f, n00[2]*0.5f, n00[0], n00[1], n00[2], 0, 0);
                sph_v[si++] = make_mesh_vert(n10[0]*0.5f, n10[1]*0.5f, n10[2]*0.5f, n10[0], n10[1], n10[2], 1, 0);
                sph_v[si++] = make_mesh_vert(n11[0]*0.5f, n11[1]*0.5f, n11[2]*0.5f, n11[0], n11[1], n11[2], 1, 1);
                sph_v[si++] = make_mesh_vert(n00[0]*0.5f, n00[1]*0.5f, n00[2]*0.5f, n00[0], n00[1], n00[2], 0, 0);
                sph_v[si++] = make_mesh_vert(n11[0]*0.5f, n11[1]*0.5f, n11[2]*0.5f, n11[0], n11[1], n11[2], 1, 1);
                sph_v[si++] = make_mesh_vert(n01[0]*0.5f, n01[1]*0.5f, n01[2]*0.5f, n01[0], n01[1], n01[2], 0, 1);
            }
        }
        upload_static_mesh(&r->mesh_sphere, sph_v, si);
        free(sph_v);
    }

    /* 4. Smooth 14-sided Cone (base radius 0.5 at y=-0.5, apex at y=+0.5) */
    const int KSEG = 14;
    VoxelVertex *cone_v = (VoxelVertex *)calloc((size_t)(KSEG * 6), sizeof(VoxelVertex));
    if (cone_v) {
        int ki = 0;
        for (int i = 0; i < KSEG; ++i) {
            float a0 = ((float)i / (float)KSEG) * 2.0f * SW_PI;
            float a1 = ((float)(i + 1) / (float)KSEG) * 2.0f * SW_PI;
            float am = 0.5f * (a0 + a1);
            float c0 = cosf(a0), s0 = sinf(a0);
            float c1 = cosf(a1), s1 = sinf(a1);
            float cm = cosf(am), sm = sinf(am);
            cone_v[ki++] = make_mesh_vert(c0 * 0.5f, -0.5f, s0 * 0.5f, c0 * 0.894f, 0.447f, s0 * 0.894f, 0, 0);
            cone_v[ki++] = make_mesh_vert(c1 * 0.5f, -0.5f, s1 * 0.5f, c1 * 0.894f, 0.447f, s1 * 0.894f, 1, 0);
            cone_v[ki++] = make_mesh_vert(0.0f,       0.5f, 0.0f,      cm * 0.894f, 0.447f, sm * 0.894f, 0.5f, 1);
            cone_v[ki++] = make_mesh_vert(0.0f,      -0.5f, 0.0f,      0, -1, 0, 0.5f, 0.5f);
            cone_v[ki++] = make_mesh_vert(c1 * 0.5f, -0.5f, s1 * 0.5f, 0, -1, 0, 1, 0);
            cone_v[ki++] = make_mesh_vert(c0 * 0.5f, -0.5f, s0 * 0.5f, 0, -1, 0, 0, 0);
        }
        upload_static_mesh(&r->mesh_cone, cone_v, ki);
        free(cone_v);
    }

    /* 5. Dynamic 6-vertex Billboard Quad */
    glGenVertexArrays(1, &r->bill_vao);
    glGenBuffers(1, &r->bill_vbo);
    glBindVertexArray(r->bill_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->bill_vbo);
    VoxelVertex dummy[6];
    memset(dummy, 0, sizeof(dummy));
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_DYNAMIC_DRAW);
    bind_voxel_vertex_layout();
    glBindVertexArray(0);
}

static void create_atlases(GlRenderer *r) {
    /* 1. High-Detail 1024x192 (64x64 tiles) Diffuse + Tangent-Space Normal/Gloss Atlases */
    uint8_t *vox_diff = NULL;
    uint8_t *vox_norm = NULL;
    int vw = 0, vh = 0;
    retro_art_generate_voxel_atlases(&vox_diff, &vox_norm, &vw, &vh);
    if (vox_diff && vox_norm) {
        glGenTextures(1, &r->voxel_atlas_tex);
        glBindTexture(GL_TEXTURE_2D, r->voxel_atlas_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, vox_diff);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &r->voxel_normal_tex);
        glBindTexture(GL_TEXTURE_2D, r->voxel_normal_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, vox_norm);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    free(vox_diff);
    free(vox_norm);

    /* 2. High-Detail 1280x128 (20x2 tiles of 64x64) Shaded & Outlined Animated Sprite Atlas */
    uint8_t *spr = NULL;
    int sw = 0, sh = 0;
    retro_art_generate_sprite_atlas(&spr, &sw, &sh);
    if (spr) {
        glGenTextures(1, &r->sprite_atlas_tex);
        glBindTexture(GL_TEXTURE_2D, r->sprite_atlas_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sw, sh, 0, GL_RGBA, GL_UNSIGNED_BYTE, spr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    free(spr);

    /* 3. 1x1 Utility Textures (white diffuse + flat normal) */
    uint8_t white[4] = {255, 255, 255, 255};
    glGenTextures(1, &r->white_tex);
    glBindTexture(GL_TEXTURE_2D, r->white_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    uint8_t flat_n[4] = {128, 128, 255, 64};
    glGenTextures(1, &r->flat_normal_tex);
    glBindTexture(GL_TEXTURE_2D, r->flat_normal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flat_n);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
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

    GLenum filter = (w > 480 || h > 480) ? GL_LINEAR : GL_NEAREST;
    glGenTextures(1, &r->retro_color_tex);
    glBindTexture(GL_TEXTURE_2D, r->retro_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
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

bool gl_renderer_init(GlRenderer *r, int retro_w, int retro_h) {
    if (!r) return false;
    memset(r, 0, sizeof(*r));
    if (!sw_gl_load_functions()) {
        fprintf(stderr, "Failed to load OpenGL 3.3 Core functions\n");
        return false;
    }

    r->dither_enabled = true;
    r->prog_sky    = link_program(kSkyVS,    kSkyFS);
    r->prog_shadow = link_program(kShadowVS, kShadowFS);
    r->prog_world  = link_program(kWorldVS,  kWorldFS);
    r->prog_post   = link_program(kPostVS,   kPostFS);
    r->prog_hud    = link_program(kHudVS,    kHudFS);
    if (!r->prog_sky || !r->prog_shadow || !r->prog_world || !r->prog_post || !r->prog_hud) {
        return false;
    }

    voxel_mesh_init(&r->scratch_mesh);
    init_mesh_library(r);

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

    create_atlases(r);
    setup_shadow_fbo(r);
    setup_retro_fbo(r, retro_w > 0 ? retro_w : 640, retro_h > 0 ? retro_h : 360);

    r->initialized = true;
    return true;
}

void gl_renderer_set_retro_res(GlRenderer *r, int retro_w, int retro_h) {
    if (!r || !r->initialized) return;
    if (retro_w != r->retro_w || retro_h != r->retro_h) {
        setup_retro_fbo(r, retro_w, retro_h);
    }
}

static void delete_mesh_gpu(MeshGpu *m) {
    if (m->vao) glDeleteVertexArrays(1, &m->vao);
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    m->vao = 0; m->vbo = 0; m->vert_count = 0;
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
    glDeleteProgram(r->prog_sky);
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
    glDeleteTextures(1, &r->voxel_normal_tex);
    glDeleteTextures(1, &r->sprite_atlas_tex);
    glDeleteTextures(1, &r->white_tex);
    glDeleteTextures(1, &r->flat_normal_tex);
    delete_mesh_gpu(&r->mesh_box);
    delete_mesh_gpu(&r->mesh_cyl);
    delete_mesh_gpu(&r->mesh_sphere);
    delete_mesh_gpu(&r->mesh_cone);
    glDeleteVertexArrays(1, &r->bill_vao);
    glDeleteBuffers(1, &r->bill_vbo);
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

/* Zero-upload sculpted 3D primitive draw using static MeshGpu VAOs */
static void draw_mesh(GlRenderer *r, const MeshGpu *mesh, Mat4 model,
                      float cr, float cg, float cb, float ca, float emissive, float gloss) {
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, model.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 0);
    glUniform4f(glGetUniformLocation(r->prog_world, "uTint"), cr, cg, cb, ca);
    glUniform1f(glGetUniformLocation(r->prog_world, "uMeshEmissive"), emissive);
    glUniform1f(glGetUniformLocation(r->prog_world, "uMeshGloss"), gloss);
    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vert_count);
}

/* 64x64 2-frame shaded & outlined billboard sprite draw */
static void draw_billboard_sprite(GlRenderer *r, Vec3 pos, float w, float h, int col, int row, float cam_yaw,
                                  float cr, float cg, float cb, float emissive) {
    float du = 1.0f / (float)RETRO_SPRITE_COLS;
    float dv = 1.0f / (float)RETRO_SPRITE_ROWS;
    float pu = 0.5f / (float)(RETRO_SPRITE_COLS * RETRO_SPRITE_PX);
    float pv = 0.5f / (float)(RETRO_SPRITE_ROWS * RETRO_SPRITE_PX);
    float u0 = (float)col * du + pu;
    float u1 = (float)(col + 1) * du - pu;
    float v0 = (float)row * dv + pv;
    float v1 = (float)(row + 1) * dv - pv;

    VoxelVertex v[6];
    const float q[6][4] = {
        {-0.5f, 0.0f, u0, v1}, { 0.5f, 0.0f, u1, v1}, { 0.5f, 1.0f, u1, v0},
        {-0.5f, 0.0f, u0, v1}, { 0.5f, 1.0f, u1, v0}, {-0.5f, 1.0f, u0, v0}
    };
    for (int i = 0; i < 6; ++i) {
        v[i].pos[0] = q[i][0] * w; v[i].pos[1] = q[i][1] * h; v[i].pos[2] = 0.0f;
        v[i].normal[0] = 0.0f; v[i].normal[1] = 0.35f; v[i].normal[2] = 0.94f;
        v[i].uv[0] = q[i][2]; v[i].uv[1] = q[i][3];
        v[i].color[0] = cr; v[i].color[1] = cg; v[i].color[2] = cb; v[i].color[3] = 1.0f;
        v[i].emissive = emissive;
    }
    Mat4 model = mat4_mul(mat4_translate(pos), mat4_rotate_y(-cam_yaw));
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, model.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas_tex);
    glBindVertexArray(r->bill_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->bill_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void unbind_all_textures(void) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static Mat4 compute_light_vp(const GameWorld *w, Vec3 focus, Vec3 *out_sun_dir) {
    Vec3 sun_dir;
    switch (w->scene) {
        case SCENE_DAY01: sun_dir = vec3_norm(vec3_make(-0.42f, 0.84f, 0.36f)); break;
        case SCENE_DAY02: sun_dir = vec3_norm(vec3_make( 0.44f, 0.78f, -0.44f)); break;
        case SCENE_DAY03: sun_dir = vec3_norm(vec3_make( 0.36f, 0.88f,  0.34f)); break;
        case SCENE_DAY04: sun_dir = vec3_norm(vec3_make(-0.35f, 0.82f, -0.45f)); break;
        case SCENE_DAY05: sun_dir = vec3_norm(vec3_make( 0.28f, 0.75f, -0.60f)); break;
        case SCENE_DAY06: sun_dir = vec3_norm(vec3_make(-0.40f, 0.80f, -0.44f)); break;
        default:          sun_dir = vec3_norm(vec3_make( 0.35f, 0.90f,  0.40f)); break;
    }
    if (out_sun_dir) *out_sun_dir = sun_dir;
    Vec3 eye = vec3_add(focus, vec3_scale(sun_dir, 36.0f));
    Mat4 view = mat4_look_at(eye, focus, vec3_make(0, 1, 0));
    Mat4 proj = mat4_ortho(-36.0f, 36.0f, -36.0f, 36.0f, 1.0f, 96.0f);
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
        float sc = world_enemy_is_boss(e->kind) ? 2.0f : (e->shrunk_timer > 0.0f ? 0.35f : 0.95f);
        Mat4 m = mat4_mul(mat4_translate(vec3_add(e->pos, vec3_make(0, sc * 0.75f, 0))),
                          mat4_scale(vec3_make(sc * 0.85f, sc * 1.5f, sc * 0.85f)));
        glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, m.m);
        glBindVertexArray(r->mesh_cyl.vao);
        glDrawArrays(GL_TRIANGLES, 0, r->mesh_cyl.vert_count);
    }

    for (int i = 0; i < w->barrel_count; ++i) {
        if (!w->barrels[i].active) continue;
        Mat4 m = mat4_mul(mat4_translate(vec3_add(w->barrels[i].pos, vec3_make(0, 0.65f, 0))),
                          mat4_scale(vec3_make(0.75f, 1.3f, 0.75f)));
        glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, m.m);
        glBindVertexArray(r->mesh_cyl.vao);
        glDrawArrays(GL_TRIANGLES, 0, r->mesh_cyl.vert_count);
    }

    for (int i = 0; i < w->boulder_count; ++i) {
        if (!w->boulders[i].active) continue;
        Mat4 m = mat4_mul(mat4_translate(w->boulders[i].pos), mat4_scale(vec3_make(2.4f, 2.4f, 2.4f)));
        glUniformMatrix4fv(glGetUniformLocation(r->prog_shadow, "uModel"), 1, GL_FALSE, m.m);
        glBindVertexArray(r->mesh_sphere.vao);
        glDrawArrays(GL_TRIANGLES, 0, r->mesh_sphere.vert_count);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ============================================================================
 * SCULPTED ARTICULATED 3D BOSSES (ALL 5 ACT I CLIMAX ENCOUNTERS)
 * ============================================================================ */

static void draw_boss_3d(GlRenderer *r, const GameWorld *w, const Enemy *e, float fl, float burn) {
    float t = e->anim_time;
    float bob = 0.08f * sinf(t * 5.0f);
    float stride = sinf(t * 6.0f) * 0.25f;
    float p3 = (w->boss_phase == 3) ? 0.30f : 0.0f;
    float em = 0.14f + p3 + fl * 0.55f + burn * 0.55f;
    Mat4 root = mat4_mul(mat4_translate(e->pos), mat4_rotate_y(-e->yaw));
    Mat4 rx90 = mat4_rotate_x(SW_PI * 0.5f);

    if (e->kind == ENEMY_BOSS_TREX) {
        /* Day 2 Tomb Raider T-Rex: Horizontal tapered scaly body, snapping jaw, white teeth, wagging tail */
        float gr = sw_clampf(0.26f + fl * 0.5f + burn * 0.5f, 0, 1);
        float gg = sw_clampf(0.52f + fl * 0.3f, 0, 1);
        float gb = sw_clampf(0.22f + fl * 0.3f, 0, 1);
        /* Legs */
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-0.62f, 0.78f, stride)), mat4_scale(vec3_make(0.55f, 1.55f, 0.65f)))),
                  gr * 0.85f, gg * 0.85f, gb * 0.85f, 1.0f, em * 0.6f, 0.35f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make( 0.62f, 0.78f, -stride)), mat4_scale(vec3_make(0.55f, 1.55f, 0.65f)))),
                  gr * 0.85f, gg * 0.85f, gb * 0.85f, 1.0f, em * 0.6f, 0.35f);
        /* Torso */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.85f + bob, 0.0f)), mat4_scale(vec3_make(1.75f, 1.55f, 2.75f)))),
                  gr, gg, gb, 1.0f, em, 0.40f);
        /* Pale scaly underbelly */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.48f + bob, 0.05f)), mat4_scale(vec3_make(1.30f, 0.85f, 2.15f)))),
                  0.66f, 0.64f, 0.42f, 1.0f, em * 0.4f, 0.25f);
        /* Sweeping wagging conical tail */
        float wag = sinf(t * 4.2f) * 0.28f;
        Mat4 tail_m = mat4_mul(root, mat4_mul(mat4_translate(vec3_make(wag * 0.6f, 1.75f + bob, -1.95f)),
                      mat4_mul(mat4_rotate_y(wag), mat4_mul(mat4_rotate_x(-SW_PI * 0.46f), mat4_scale(vec3_make(1.05f, 2.65f, 1.05f))))));
        draw_mesh(r, &r->mesh_cone, tail_m, gr * 0.9f, gg * 0.9f, gb * 0.9f, 1.0f, em * 0.6f, 0.35f);
        /* Neck & Snapping Snout */
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.38f + bob, 1.15f)),
                  mat4_mul(mat4_rotate_x(0.45f), mat4_scale(vec3_make(0.88f, 1.15f, 0.95f))))), gr, gg, gb, 1.0f, em, 0.40f);
        draw_mesh(r, &r->mesh_box, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.85f + bob, 1.78f)), mat4_scale(vec3_make(1.05f, 0.55f, 1.38f)))),
                  gr * 1.08f, gg * 1.05f, gb, 1.0f, em, 0.45f);
        float jaw_open = 0.14f + 0.14f * fabsf(sinf(t * 4.5f));
        draw_mesh(r, &r->mesh_box, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.42f + bob - jaw_open * 0.35f, 1.75f)),
                  mat4_mul(mat4_rotate_x(jaw_open), mat4_scale(vec3_make(0.92f, 0.30f, 1.25f))))), 0.55f, 0.18f, 0.18f, 1.0f, em * 0.5f, 0.3f);
        /* White conical predator teeth */
        for (int k = -1; k <= 1; k += 2) {
            draw_mesh(r, &r->mesh_cone, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)k * 0.38f, 2.52f + bob, 2.22f)),
                      mat4_mul(mat4_rotate_x(SW_PI), mat4_scale(vec3_make(0.14f, 0.28f, 0.14f))))), 0.96f, 0.95f, 0.88f, 1.0f, 0.15f, 0.7f);
        }
        /* Blazing golden eyes */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-0.46f, 3.00f + bob, 1.65f)), mat4_scale(vec3_make(0.24f, 0.24f, 0.24f)))),
                  1.0f, 0.90f, 0.18f, 1.0f, 1.15f, 0.9f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make( 0.46f, 3.00f + bob, 1.65f)), mat4_scale(vec3_make(0.24f, 0.24f, 0.24f)))),
                  1.0f, 0.90f, 0.18f, 1.0f, 1.15f, 0.9f);
    } else if (e->kind == ENEMY_BOSS_SHAMBLER) {
        /* Day 4 Quake Shambler: Towering hunched white furred yeti with crackling cyan lightning hands */
        float wr = sw_clampf(0.84f + fl * 0.16f + burn * 0.35f, 0, 1);
        float wg = sw_clampf(0.82f + fl * 0.16f - burn * 0.25f, 0, 1);
        float wb = sw_clampf(0.88f + fl * 0.12f - burn * 0.35f, 0, 1);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-0.58f, 0.72f, stride)), mat4_scale(vec3_make(0.62f, 1.45f, 0.65f)))),
                  wr * 0.85f, wg * 0.85f, wb * 0.88f, 1.0f, em * 0.5f, 0.15f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make( 0.58f, 0.72f, -stride)), mat4_scale(vec3_make(0.62f, 1.45f, 0.65f)))),
                  wr * 0.85f, wg * 0.85f, wb * 0.88f, 1.0f, em * 0.5f, 0.15f);
        /* Hunched muscular body & broad shoulders */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.95f + bob, 0.0f)), mat4_scale(vec3_make(2.25f, 1.95f, 1.75f)))),
                  wr, wg, wb, 1.0f, em, 0.18f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.75f + bob, 0.45f)), mat4_scale(vec3_make(1.22f, 1.08f, 1.15f)))),
                  wr * 0.94f, wg * 0.92f, wb * 0.94f, 1.0f, em, 0.2f);
        /* Terrifying bloody toothed maw (faceless!) */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.58f + bob, 0.95f)), mat4_scale(vec3_make(0.75f, 0.44f, 0.42f)))),
                  0.68f, 0.08f, 0.12f, 1.0f, 0.35f, 0.4f);
        /* Swinging clawed arms + blazing blue-white lightning orbs */
        for (int s = -1; s <= 1; s += 2) {
            float arm_swing = sinf(t * 5.0f + (float)s) * 0.35f;
            draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 1.25f, 2.05f + bob, 0.45f)),
                      mat4_mul(mat4_rotate_x(0.85f + arm_swing), mat4_scale(vec3_make(0.50f, 1.45f, 0.50f))))),
                      wr * 0.92f, wg * 0.90f, wb * 0.94f, 1.0f, em * 0.6f, 0.18f);
            float pulse = 0.45f + 0.16f * sinf(t * 14.0f + (float)s * 2.0f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 1.32f, 1.95f + bob, 1.18f)),
                      mat4_scale(vec3_make(pulse, pulse, pulse)))), 0.38f, 0.92f, 1.0f, 0.95f, 1.35f, 0.95f);
        }
    } else if (e->kind == ENEMY_BOSS_MATRIARCH) {
        /* Day 5 Blood Gargoyle Matriarch: Swept flapping demonic wings, curved horns, glowing crimson eyes */
        float vr = sw_clampf(0.42f + fl * 0.5f + burn * 0.5f, 0, 1);
        float vg = sw_clampf(0.24f + fl * 0.3f, 0, 1);
        float vb = sw_clampf(0.48f + fl * 0.3f, 0, 1);
        float flap = sinf(t * 6.5f) * 0.42f;
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.95f + bob * 1.8f, 0)), mat4_scale(vec3_make(1.25f, 1.65f, 0.95f)))),
                  vr, vg, vb, 1.0f, em, 0.45f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.95f + bob * 1.8f, 0.15f)), mat4_scale(vec3_make(0.92f, 0.95f, 0.92f)))),
                  vr * 1.1f, vg * 1.1f, vb * 1.1f, 1.0f, em, 0.50f);
        for (int s = -1; s <= 1; s += 2) {
            /* Curved horns */
            draw_mesh(r, &r->mesh_cone, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.42f, 3.55f + bob * 1.8f, 0.05f)),
                      mat4_mul(mat4_rotate_z_local(-(float)s * 0.48f), mat4_scale(vec3_make(0.26f, 0.85f, 0.26f))))),
                      0.18f, 0.14f, 0.22f, 1.0f, 0.15f, 0.65f);
            /* Swept Bat/Gargoyle Wings */
            draw_mesh(r, &r->mesh_box, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 1.65f, 2.45f + bob * 1.8f + flap * 0.55f, -0.35f)),
                      mat4_mul(mat4_rotate_z_local((float)s * (0.28f + flap)), mat4_scale(vec3_make(2.25f, 1.25f, 0.14f))))),
                      0.58f, 0.12f, 0.24f, 1.0f, em * 0.9f, 0.35f);
            /* Glowing blood eyes */
            draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.24f, 3.02f + bob * 1.8f, 0.56f)),
                      mat4_scale(vec3_make(0.18f, 0.18f, 0.18f)))), 1.0f, 0.14f, 0.16f, 1.0f, 1.35f, 0.95f);
        }
    } else if (e->kind == ENEMY_BOSS_CYBERTITAN) {
        /* Day 6 Doom Cyber-Titan: Muscular demon + chrome hydraulic piston leg + cylindrical Rocket Launcher arm + bull horns */
        float fr = sw_clampf(0.76f + fl * 0.24f + burn * 0.2f, 0, 1);
        float fg = sw_clampf(0.36f + fl * 0.4f, 0, 1);
        float fb = sw_clampf(0.28f + fl * 0.4f, 0, 1);
        /* Left organic leg & Right gleaming chrome cybernetic piston leg */
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-0.68f, 0.80f, stride)), mat4_scale(vec3_make(0.62f, 1.60f, 0.62f)))),
                  fr * 0.85f, fg * 0.82f, fb * 0.80f, 1.0f, em * 0.5f, 0.30f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make( 0.68f, 0.80f, -stride)), mat4_scale(vec3_make(0.52f, 1.60f, 0.52f)))),
                  0.78f, 0.82f, 0.88f, 1.0f, 0.15f, 0.92f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make( 0.68f, 0.85f, -stride)), mat4_scale(vec3_make(0.68f, 0.42f, 0.68f)))),
                  0.85f, 0.65f, 0.26f, 1.0f, 0.20f, 0.85f);
        /* Muscular torso & ribbed abdomen cables */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.08f + bob, 0)), mat4_scale(vec3_make(2.15f, 1.75f, 1.50f)))),
                  fr, fg, fb, 1.0f, em, 0.35f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.55f + bob, 0.42f)), mat4_scale(vec3_make(0.95f, 0.55f, 0.55f)))),
                  0.32f, 0.34f, 0.38f, 1.0f, 0.15f, 0.85f);
        /* Horned Bull Head */
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 3.10f + bob, 0.22f)), mat4_scale(vec3_make(1.05f, 1.02f, 1.05f)))),
                  fr * 0.95f, fg * 0.90f, fb * 0.90f, 1.0f, em, 0.35f);
        for (int s = -1; s <= 1; s += 2) {
            draw_mesh(r, &r->mesh_cone, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.68f, 3.45f + bob, 0.10f)),
                      mat4_mul(mat4_rotate_z_local(-(float)s * 0.85f), mat4_scale(vec3_make(0.32f, 1.05f, 0.32f))))),
                      0.26f, 0.22f, 0.20f, 1.0f, 0.08f, 0.60f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.24f, 3.15f + bob, 0.68f)),
                      mat4_scale(vec3_make(0.18f, 0.16f, 0.18f)))), 1.0f, 0.15f, 0.10f, 1.0f, 1.4f, 0.95f);
        }
        /* Massive Cybernetic Rocket Launcher Arm */
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-1.32f, 2.05f + bob, 0.72f)),
                  mat4_mul(rx90, mat4_scale(vec3_make(0.60f, 1.85f, 0.60f))))), 0.46f, 0.48f, 0.54f, 1.0f, 0.18f, 0.90f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(-1.32f, 2.05f + bob, 1.62f)),
                  mat4_scale(vec3_make(0.46f, 0.46f, 0.28f)))), 1.0f, 0.42f, 0.12f, 1.0f, 1.2f, 0.8f);
    } else {
        /* Day 1 KRAMPUS-MECH: Armored festive crimson mech with shoulder missile pods, Krampus horns & glowing visor */
        float mr = sw_clampf(0.80f + fl * 0.2f, 0, 1);
        float mg = sw_clampf(0.15f + fl * 0.5f, 0, 1);
        float mb = sw_clampf(0.20f + fl * 0.5f, 0, 1);
        /* Armored piston legs */
        for (int s = -1; s <= 1; s += 2) {
            float st = (s < 0) ? stride : -stride;
            draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.68f, 0.75f, st)), mat4_scale(vec3_make(0.56f, 1.50f, 0.56f)))),
                      0.38f, 0.42f, 0.48f, 1.0f, 0.10f, 0.85f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.68f, 0.78f, st + 0.12f)), mat4_scale(vec3_make(0.58f, 0.45f, 0.58f)))),
                      0.85f, 0.68f, 0.22f, 1.0f, 0.18f, 0.85f);
        }
        /* Heavy cylindrical crimson chassis + gold trim */
        draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 1.98f + bob, 0)), mat4_scale(vec3_make(1.88f, 1.55f, 1.52f)))),
                  mr, mg, mb, 1.0f, em, 0.75f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.86f + bob, 0.12f)), mat4_scale(vec3_make(1.12f, 0.92f, 1.12f)))),
                  0.26f, 0.28f, 0.34f, 1.0f, 0.14f, 0.88f);
        /* Glowing crimson visor */
        draw_mesh(r, &r->mesh_box, mat4_mul(root, mat4_mul(mat4_translate(vec3_make(0, 2.88f + bob, 0.60f)), mat4_scale(vec3_make(0.86f, 0.24f, 0.24f)))),
                  1.0f, 0.14f, 0.24f, 1.0f, 1.35f, 0.95f);
        /* Dual Krampus horns & shoulder missile pods */
        for (int s = -1; s <= 1; s += 2) {
            draw_mesh(r, &r->mesh_cone, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 0.52f, 3.45f + bob, 0.05f)),
                      mat4_mul(mat4_rotate_z_local(-(float)s * 0.55f), mat4_scale(vec3_make(0.28f, 0.92f, 0.28f))))),
                      0.92f, 0.76f, 0.24f, 1.0f, 0.28f, 0.88f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 1.24f, 2.52f + bob, 0.25f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.56f, 1.35f, 0.56f))))), 0.34f, 0.38f, 0.45f, 1.0f, 0.16f, 0.88f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(root, mat4_mul(mat4_translate(vec3_make((float)s * 1.24f, 2.52f + bob, 0.94f)),
                      mat4_scale(vec3_make(0.42f, 0.42f, 0.22f)))), 1.0f, 0.28f, 0.14f, 1.0f, 1.10f, 0.85f);
        }
    }
}

/* ============================================================================
 * SCULPTED MULTI-PART FIRST-PERSON WEAPON VIEWMODELS + GLOVED HANDS + FLASH
 * ============================================================================ */

static void draw_muzzle_star(GlRenderer *r, Vec3 tip, float scale, float cr, float cg, float cb) {
    draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(tip), mat4_scale(vec3_make(scale, scale, scale))),
              cr, cg, cb, 0.95f, 1.6f, 0.1f);
    for (int k = 0; k < 4; ++k) {
        float ang = (float)k * (SW_PI * 0.25f);
        Mat4 m = mat4_mul(mat4_translate(tip), mat4_mul(mat4_rotate_z_local(ang), mat4_scale(vec3_make(scale * 0.32f, scale * 2.1f, scale * 0.32f))));
        draw_mesh(r, &r->mesh_cone, m, 1.0f, 0.95f, 0.72f, 0.88f, 1.5f, 0.1f);
    }
}

static void draw_gloved_hand(GlRenderer *r, Vec3 pos) {
    /* Tactical dark leather glove + wrist cuff */
    draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(pos), mat4_scale(vec3_make(0.095f, 0.088f, 0.115f))),
              0.16f, 0.15f, 0.18f, 1.0f, 0.02f, 0.35f);
    draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(pos, vec3_make(0.02f, -0.04f, 0.11f))),
              mat4_mul(mat4_rotate_x(1.30f), mat4_scale(vec3_make(0.088f, 0.18f, 0.088f)))),
              0.22f, 0.24f, 0.28f, 1.0f, 0.02f, 0.25f);
}

static void draw_weapon_viewmodel(GlRenderer *r, const Player *p) {
    glClear(GL_DEPTH_BUFFER_BIT);
    Mat4 id_view = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uView"), 1, GL_FALSE, id_view.m);
    glUniform3f(glGetUniformLocation(r->prog_world, "uCamPos"), 0.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), 0.0f);
    glUniform1i(glGetUniformLocation(r->prog_world, "uLightCount"), 0);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunDir"), 0.42f, 0.72f, 0.55f);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunColor"), 0.98f, 0.95f, 0.90f);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSkyAmbient"), 0.48f, 0.52f, 0.62f);
    glUniform3f(glGetUniformLocation(r->prog_world, "uGroundAmbient"), 0.24f, 0.22f, 0.25f);

    float bob_x = sinf(p->walk_bob) * 0.015f;
    float bob_y = fabsf(cosf(p->walk_bob)) * 0.013f;
    float kick  = (p->fire_cooldown > 0.0f) ? sw_clampf(p->fire_cooldown * 0.35f, 0.0f, 0.12f) : 0.0f;
    float pitch_kick = kick * 1.15f;
    bool flaring = (p->muzzle_flash > 0.0f);
    Mat4 rx90 = mat4_rotate_x(SW_PI * 0.5f);

    /* Mighty Boot Stomp kick animation on left side */
    if (p->stomp_flash > 0.0f) {
        float kt = p->stomp_flash / 0.32f;
        Vec3 bpos = vec3_make(-0.18f, -0.34f + kt * 0.26f, -0.54f - kt * 0.14f);
        draw_mesh(r, &r->mesh_box, mat4_mul(mat4_translate(bpos), mat4_scale(vec3_make(0.17f, 0.13f, 0.34f))),
                  0.26f, 0.17f, 0.12f, 1.0f, 0.05f, 0.45f);
        draw_mesh(r, &r->mesh_box, mat4_mul(mat4_translate(vec3_add(bpos, vec3_make(0, -0.07f, 0))),
                  mat4_scale(vec3_make(0.18f, 0.04f, 0.36f))), 0.12f, 0.12f, 0.14f, 1.0f, 0.02f, 0.25f);
    }

    Vec3 base = vec3_make(0.22f + bob_x, -0.195f - bob_y - kick * 0.18f, -0.46f + kick * 0.40f);
    Mat4 weap_root = mat4_mul(mat4_translate(base), mat4_rotate_x(pitch_kick));

    switch (p->weapon) {
        case WEAP_PISTOL:
        case WEAP_USP:
        case WEAP_DEAGLE:
        case WEAP_MAGNUM: {
            bool chrome = (p->weapon == WEAP_MAGNUM || p->weapon == WEAP_DEAGLE);
            bool silenced = (p->weapon == WEAP_USP);
            float mr = chrome ? 0.82f : 0.48f;
            float mg = chrome ? 0.85f : 0.52f;
            float mb = chrome ? 0.90f : 0.58f;
            /* Dark polymer/steel lower frame + contrasting machined upper slide */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.022f, 0.01f)),
                      mat4_scale(vec3_make(0.062f, 0.036f, 0.27f)))), 0.18f, 0.20f, 0.24f, 1.0f, 0.02f, 0.55f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.016f, 0.0f)),
                      mat4_scale(vec3_make(0.066f, 0.048f, 0.29f)))), mr, mg, mb, 1.0f, 0.04f, 0.92f);
            /* Dark ejection port & glowing green tritium iron sights */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0.022f, 0.038f, -0.03f)),
                      mat4_scale(vec3_make(0.028f, 0.012f, 0.065f)))), 0.10f, 0.11f, 0.13f, 1.0f, 0.0f, 0.30f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(-0.018f, 0.046f, 0.12f)),
                      mat4_scale(vec3_make(0.012f, 0.014f, 0.016f)))), 0.22f, 1.0f, 0.35f, 1.0f, 0.95f, 0.8f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make( 0.018f, 0.046f, 0.12f)),
                      mat4_scale(vec3_make(0.012f, 0.014f, 0.016f)))), 0.22f, 1.0f, 0.35f, 1.0f, 0.95f, 0.8f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make( 0.0f,   0.048f, -0.13f)),
                      mat4_scale(vec3_make(0.010f, 0.016f, 0.018f)))), 1.0f, 0.42f, 0.16f, 1.0f, 0.95f, 0.8f);
            /* Cylindrical Barrel + Optional Ribbed USP Tactical Suppressor */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.016f, -0.16f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.034f, 0.13f, 0.034f))))), 0.22f, 0.24f, 0.28f, 1.0f, 0.03f, 0.92f);
            if (silenced) {
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.016f, -0.27f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.046f, 0.21f, 0.046f))))), 0.18f, 0.20f, 0.24f, 1.0f, 0.02f, 0.78f);
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.016f, -0.19f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.052f, 0.024f, 0.052f))))), 0.42f, 0.44f, 0.48f, 1.0f, 0.03f, 0.92f);
            }
            /* Checkered Grip + Visible Tactical Gloved Hand */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.082f, 0.088f)),
                      mat4_mul(mat4_rotate_x(-0.26f), mat4_scale(vec3_make(0.056f, 0.13f, 0.076f))))), 0.15f, 0.14f, 0.16f, 1.0f, 0.01f, 0.30f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0.012f, -0.078f, 0.095f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.02f, silenced ? -0.39f : -0.24f)), 0.085f, 1.0f, 0.85f, 0.32f);
            break;
        }
        case WEAP_DUAL_PISTOLS: {
            /* Iconic Lara Croft twin brushed-chrome pistols with dark grips, sights & twin gloves! */
            for (int side = -1; side <= 1; side += 2) {
                Vec3 sp = vec3_make((float)side * (0.23f + bob_x), base.y, base.z);
                Mat4 sr = mat4_mul(mat4_translate(sp), mat4_rotate_x(pitch_kick));
                draw_mesh(r, &r->mesh_box, mat4_mul(sr, mat4_mul(mat4_translate(vec3_make(0, -0.020f, 0.01f)),
                          mat4_scale(vec3_make(0.054f, 0.032f, 0.25f)))), 0.20f, 0.22f, 0.26f, 1.0f, 0.02f, 0.60f);
                draw_mesh(r, &r->mesh_box, mat4_mul(sr, mat4_mul(mat4_translate(vec3_make(0, 0.014f, 0.0f)),
                          mat4_scale(vec3_make(0.058f, 0.044f, 0.27f)))), 0.82f, 0.85f, 0.90f, 1.0f, 0.04f, 0.95f);
                draw_mesh(r, &r->mesh_box, mat4_mul(sr, mat4_mul(mat4_translate(vec3_make(0, 0.042f, -0.11f)),
                          mat4_scale(vec3_make(0.010f, 0.014f, 0.016f)))), 1.0f, 0.55f, 0.18f, 1.0f, 0.85f, 0.8f);
                draw_mesh(r, &r->mesh_cyl, mat4_mul(sr, mat4_mul(mat4_translate(vec3_make(0, 0.014f, -0.15f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.030f, 0.11f, 0.030f))))), 0.24f, 0.26f, 0.30f, 1.0f, 0.04f, 0.92f);
                draw_gloved_hand(r, vec3_add(sp, vec3_make(0, -0.072f, 0.085f)));
                if (flaring) draw_muzzle_star(r, vec3_add(sp, vec3_make(0, 0.02f, -0.22f)), 0.075f, 1.0f, 0.88f, 0.40f);
            }
            break;
        }
        case WEAP_FLARE: {
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(rx90, mat4_scale(vec3_make(0.045f, 0.26f, 0.045f)))),
                      0.20f, 0.68f, 0.32f, 1.0f, 0.18f, 0.55f);
            draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.01f, -0.15f)), 0.075f, 0.35f, 1.0f, 0.45f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.03f, 0.06f)));
            break;
        }
        case WEAP_SHOTGUN:
        case WEAP_SUPER_SHOTGUN: {
            bool dbl = (p->weapon == WEAP_SUPER_SHOTGUN);
            if (dbl) {
                /* Iconic Doom II Double-Barreled Super Shotgun: twin blued steel barrels + brass bead + walnut forend + hammers */
                for (int s = -1; s <= 1; s += 2) {
                    draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make((float)s * 0.028f, 0.024f, -0.12f)),
                              mat4_mul(rx90, mat4_scale(vec3_make(0.048f, 0.46f, 0.048f))))), 0.38f, 0.42f, 0.48f, 1.0f, 0.04f, 0.94f);
                }
                /* Center rib & brass bead front sight */
                draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.045f, -0.12f)),
                          mat4_scale(vec3_make(0.018f, 0.010f, 0.44f)))), 0.22f, 0.24f, 0.28f, 1.0f, 0.02f, 0.85f);
                draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.056f, -0.33f)),
                          mat4_scale(vec3_make(0.016f, 0.016f, 0.016f)))), 1.0f, 0.82f, 0.24f, 1.0f, 0.55f, 0.95f);
                /* Walnut forend wood grip under twin barrels */
                draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.014f, -0.04f)),
                          mat4_scale(vec3_make(0.095f, 0.048f, 0.19f)))), 0.48f, 0.24f, 0.11f, 1.0f, 0.02f, 0.38f);
                /* Twin brass hammers */
                for (int s = -1; s <= 1; s += 2) {
                    draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make((float)s * 0.032f, 0.055f, 0.13f)),
                              mat4_mul(mat4_rotate_x(-0.45f), mat4_scale(vec3_make(0.016f, 0.036f, 0.022f))))), 0.78f, 0.62f, 0.26f, 1.0f, 0.06f, 0.90f);
                }
            } else {
                /* Pump Shotgun: top cylindrical barrel + lower tubular magazine + wooden pump grip */
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.032f, -0.13f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.044f, 0.52f, 0.044f))))), 0.46f, 0.50f, 0.56f, 1.0f, 0.04f, 0.94f);
                draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.058f, -0.37f)),
                          mat4_scale(vec3_make(0.015f, 0.015f, 0.015f)))), 1.0f, 0.80f, 0.22f, 1.0f, 0.55f, 0.95f);
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.012f, -0.10f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.038f, 0.42f, 0.038f))))), 0.28f, 0.31f, 0.36f, 1.0f, 0.02f, 0.85f);
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.018f, -0.14f + kick * 0.5f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.062f, 0.16f, 0.062f))))), 0.48f, 0.25f, 0.12f, 1.0f, 0.02f, 0.40f);
            }
            /* Engraved Breech Receiver & Hardwood Stock */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.008f, 0.11f)),
                      mat4_scale(vec3_make(0.095f, 0.082f, 0.16f)))), 0.28f, 0.30f, 0.36f, 1.0f, 0.03f, 0.88f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.045f, 0.22f)),
                      mat4_mul(mat4_rotate_x(-0.18f), mat4_scale(vec3_make(0.075f, 0.095f, 0.20f))))), 0.48f, 0.24f, 0.11f, 1.0f, 0.02f, 0.38f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0.01f, -0.055f, 0.14f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.03f, -0.39f)), dbl ? 0.14f : 0.11f, 1.0f, 0.78f, 0.24f);
            break;
        }
        case WEAP_CARBINE:
        case WEAP_SNIPER: {
            bool awp = (p->weapon == WEAP_SNIPER);
            float blen = awp ? 0.62f : 0.46f;
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_scale(vec3_make(0.072f, 0.085f, 0.34f))),
                      awp ? 0.24f : 0.22f, awp ? 0.34f : 0.24f, awp ? 0.22f : 0.28f, 1.0f, 0.02f, 0.62f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.015f, -0.22f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.034f, blen, 0.034f))))), 0.32f, 0.34f, 0.38f, 1.0f, 0.03f, 0.90f);
            /* Cylindrical Optical Scope on top! */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.088f, -0.02f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.048f, awp ? 0.28f : 0.16f, 0.048f))))), 0.18f, 0.20f, 0.24f, 1.0f, 0.04f, 0.88f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.088f, awp ? 0.12f : 0.06f)),
                      mat4_scale(vec3_make(0.044f, 0.044f, 0.015f)))), 0.22f, 0.78f, 1.0f, 1.0f, 0.45f, 0.98f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.075f, 0.10f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.02f, -0.22f - blen * 0.52f)), 0.10f, 1.0f, 0.86f, 0.34f);
            break;
        }
        case WEAP_FLASH_SMOKE: {
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_scale(vec3_make(0.085f, 0.16f, 0.085f))),
                      0.62f, 0.70f, 0.66f, 1.0f, 0.05f, 0.75f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.095f, 0)),
                      mat4_scale(vec3_make(0.042f, 0.04f, 0.042f)))), 0.25f, 0.85f, 1.0f, 1.0f, 0.65f, 0.85f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.04f, 0.04f)));
            break;
        }
        case WEAP_SHRINKER:
        case WEAP_BFG: {
            bool bfg = (p->weapon == WEAP_BFG);
            float sc = bfg ? 1.35f : 1.0f;
            /* Ribbed Alien/UAC Chassis + Glowing Emerald Plasma Core Sphere */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(rx90, mat4_scale(vec3_make(0.13f * sc, 0.44f, 0.13f * sc)))),
                      bfg ? 0.44f : 0.56f, bfg ? 0.46f : 0.44f, bfg ? 0.52f : 0.32f, 1.0f, 0.05f, 0.82f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.055f * sc, -0.10f)),
                      mat4_scale(vec3_make(0.11f * sc, 0.11f * sc, 0.14f * sc)))), 0.22f, 1.0f, 0.42f, 1.0f, flaring ? 1.55f : 0.95f, 0.95f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.08f, 0.12f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.02f, -0.28f)), 0.13f * sc, 0.32f, 1.0f, 0.48f);
            break;
        }
        case WEAP_NAILGUN:
        case WEAP_CHAINGUN: {
            bool cg = (p->weapon == WEAP_CHAINGUN);
            float cr = cg ? 0.46f : 0.68f, cgr = cg ? 0.48f : 0.50f, cb = cg ? 0.54f : 0.28f;
            int barrels = cg ? 4 : 2;
            float spin = cg ? (p->walk_bob * 3.0f + (flaring ? 1.2f : 0.0f)) : 0.0f;
            /* Heavy Cast-Iron Receiver Housing + Amber Riveted Magazine */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.01f, 0.08f)),
                      mat4_scale(vec3_make(0.135f, 0.105f, 0.22f)))), 0.24f, 0.22f, 0.22f, 1.0f, 0.03f, 0.82f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.052f, 0.08f)),
                      mat4_scale(vec3_make(0.052f, 0.024f, 0.14f)))), 0.88f, 0.62f, 0.20f, 1.0f, 0.15f, 0.85f);
            for (int b = 0; b < barrels; ++b) {
                float ang = spin + ((float)b / (float)barrels) * 2.0f * SW_PI;
                float ox = cosf(ang) * (cg ? 0.042f : 0.038f);
                float oy = sinf(ang) * (cg ? 0.042f : 0.0f);
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(ox, oy, -0.13f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.032f, 0.44f, 0.032f))))), cr, cgr, cb, 1.0f, 0.04f, 0.92f);
            }
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0, -0.25f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.115f, 0.042f, 0.115f))))), 0.26f, 0.28f, 0.34f, 1.0f, 0.04f, 0.88f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.08f, 0.12f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.01f, -0.37f)), 0.11f, 1.0f, 0.82f, 0.28f);
            break;
        }
        case WEAP_ROCKET: {
            /* Quake/Doom cylindrical heavy tube rocket launcher with glowing warhead tip */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(rx90, mat4_scale(vec3_make(0.145f, 0.56f, 0.145f)))),
                      0.38f, 0.36f, 0.34f, 1.0f, 0.04f, 0.84f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0, -0.26f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.165f, 0.06f, 0.165f))))), 0.62f, 0.46f, 0.26f, 1.0f, 0.08f, 0.88f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0, -0.29f)),
                      mat4_mul(mat4_rotate_x(-SW_PI * 0.5f), mat4_scale(vec3_make(0.10f, 0.10f, 0.10f))))), 0.92f, 0.28f, 0.14f, 1.0f, 0.45f, 0.7f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.09f, 0.08f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0, -0.34f)), 0.15f, 1.0f, 0.62f, 0.18f);
            break;
        }
        case WEAP_THUNDERBOLT: {
            /* Brass Tesla Coil Lightning Gun with copper rings + crackling cyan sphere */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(rx90, mat4_scale(vec3_make(0.10f, 0.46f, 0.10f)))),
                      0.68f, 0.48f, 0.28f, 1.0f, 0.06f, 0.90f);
            for (int k = 0; k < 3; ++k) {
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0, -0.05f - (float)k * 0.08f)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.135f, 0.025f, 0.135f))))), 0.85f, 0.68f, 0.38f, 1.0f, 0.12f, 0.94f);
            }
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0, -0.26f)),
                      mat4_scale(vec3_make(0.085f, 0.085f, 0.085f)))), 0.35f, 0.92f, 1.0f, 1.0f, 1.35f, 0.95f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.08f, 0.12f)));
            break;
        }
        case WEAP_PITCHFORK: {
            /* Iconic Blood 4-prong crimson-tipped pitchfork! */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.03f, 0.10f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.042f, 0.42f, 0.042f))))), 0.48f, 0.28f, 0.15f, 1.0f, 0.03f, 0.35f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.0f, -0.10f)),
                      mat4_scale(vec3_make(0.18f, 0.028f, 0.032f)))), 0.42f, 0.44f, 0.48f, 1.0f, 0.04f, 0.85f);
            for (int k = 0; k < 4; ++k) {
                float ox = -0.075f + (float)k * 0.050f;
                draw_mesh(r, &r->mesh_cone, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(ox, 0.01f, -0.24f)),
                          mat4_mul(mat4_rotate_x(-SW_PI * 0.5f), mat4_scale(vec3_make(0.020f, 0.26f, 0.020f))))),
                          0.82f, 0.20f, 0.22f, 1.0f, 0.18f, 0.88f);
            }
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.04f, 0.12f)));
            break;
        }
        case WEAP_FLAREGUN: {
            /* Iconic Blood Brass Break-Action Flare Pistol + Iron Breech + Glowing Orange Bore + Walnut Grip */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.022f, -0.08f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.068f, 0.28f, 0.068f))))), 0.84f, 0.56f, 0.22f, 1.0f, 0.06f, 0.92f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.022f, -0.22f)),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.076f, 0.024f, 0.076f))))), 0.24f, 0.24f, 0.28f, 1.0f, 0.04f, 0.88f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.022f, -0.225f)),
                      mat4_scale(vec3_make(0.044f, 0.044f, 0.015f)))), 1.0f, 0.35f, 0.08f, 1.0f, 1.15f, 0.9f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.010f, 0.065f)),
                      mat4_scale(vec3_make(0.074f, 0.076f, 0.095f)))), 0.24f, 0.24f, 0.28f, 1.0f, 0.04f, 0.88f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.056f, 0.092f)),
                      mat4_mul(mat4_rotate_x(-0.5f), mat4_scale(vec3_make(0.018f, 0.036f, 0.022f))))), 0.72f, 0.52f, 0.24f, 1.0f, 0.04f, 0.88f);
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, -0.072f, 0.075f)),
                      mat4_mul(mat4_rotate_x(-0.25f), mat4_scale(vec3_make(0.056f, 0.13f, 0.078f))))), 0.44f, 0.22f, 0.12f, 1.0f, 0.02f, 0.38f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0.01f, -0.072f, 0.085f)));
            if (flaring) draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.02f, -0.25f)), 0.11f, 1.0f, 0.45f, 0.12f);
            break;
        }
        case WEAP_AEROSOL: {
            /* Iconic Blood Red Aerosol Can + Zippo Lighter Flame! */
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_scale(vec3_make(0.095f, 0.19f, 0.095f))),
                      0.86f, 0.16f, 0.14f, 1.0f, 0.06f, 0.82f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.095f, 0)),
                      mat4_scale(vec3_make(0.092f, 0.05f, 0.092f)))), 0.82f, 0.84f, 0.88f, 1.0f, 0.06f, 0.92f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(-0.07f, 0.08f, -0.09f)),
                      mat4_scale(vec3_make(0.045f, 0.095f, 0.045f)))), 1.0f, 0.68f, 0.16f, 0.95f, 1.4f, 0.2f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.06f, 0.04f)));
            break;
        }
        case WEAP_PIPEBOMB:
        case WEAP_TNT: {
            /* Bundled Crimson TNT / Pipebomb + Left-hand Steel Detonator */
            for (int s = -1; s <= 1; ++s) {
                draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make((float)s * 0.032f, 0, 0)),
                          mat4_mul(rx90, mat4_scale(vec3_make(0.038f, 0.22f, 0.038f))))), 0.88f, 0.18f, 0.15f, 1.0f, 0.08f, 0.55f);
            }
            draw_muzzle_star(r, vec3_add(base, vec3_make(0, 0.02f, -0.13f)), 0.045f, 1.0f, 0.82f, 0.25f);
            draw_mesh(r, &r->mesh_box, mat4_mul(mat4_translate(vec3_make(-0.22f + bob_x, -0.28f - bob_y, -0.46f)),
                      mat4_scale(vec3_make(0.085f, 0.13f, 0.075f))), 0.34f, 0.38f, 0.44f, 1.0f, 0.06f, 0.80f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_make(-0.22f + bob_x, -0.20f - bob_y, -0.46f)),
                      mat4_scale(vec3_make(0.035f, 0.035f, 0.035f))), 1.0f, 0.18f, 0.18f, 1.0f, 1.1f, 0.9f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.05f, 0.05f)));
            break;
        }
        case WEAP_VOODOO: {
            /* Sculpted Burlap Voodoo Doll + Glowing Cursed Pin */
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_scale(vec3_make(0.11f, 0.16f, 0.08f))),
                      0.68f, 0.54f, 0.38f, 1.0f, 0.05f, 0.18f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.11f, 0)),
                      mat4_scale(vec3_make(0.088f, 0.088f, 0.088f)))), 0.72f, 0.58f, 0.42f, 1.0f, 0.05f, 0.18f);
            draw_mesh(r, &r->mesh_cyl, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(-0.07f + kick * 0.4f, 0.02f, -0.04f)),
                      mat4_mul(mat4_rotate_z_local(1.35f), mat4_scale(vec3_make(0.015f, 0.16f, 0.015f))))),
                      0.88f, 0.28f, 1.0f, 1.0f, 1.15f, 0.90f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.08f, 0.05f)));
            break;
        }
        case WEAP_CHAINSAW: {
            float vib = sinf(p->walk_bob * 6.0f + kick * 20.0f) * 0.008f;
            Mat4 cr = mat4_mul(mat4_translate(vec3_add(base, vec3_make(vib, 0, 0.02f))), mat4_rotate_x(pitch_kick));
            draw_mesh(r, &r->mesh_box, mat4_mul(cr, mat4_scale(vec3_make(0.135f, 0.115f, 0.18f))),
                      0.92f, 0.72f, 0.14f, 1.0f, 0.08f, 0.75f);
            draw_mesh(r, &r->mesh_box, mat4_mul(cr, mat4_mul(mat4_translate(vec3_make(0, 0.02f, -0.24f)),
                      mat4_scale(vec3_make(0.036f, 0.088f, 0.38f)))), 0.74f, 0.78f, 0.84f, 1.0f, 0.08f, 0.94f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.07f, 0.10f)));
            break;
        }
        default: {
            /* Laser Tripmine emitter */
            draw_mesh(r, &r->mesh_box, mat4_mul(weap_root, mat4_scale(vec3_make(0.13f, 0.085f, 0.16f))),
                      0.28f, 0.32f, 0.38f, 1.0f, 0.05f, 0.80f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(weap_root, mat4_mul(mat4_translate(vec3_make(0, 0.03f, -0.085f)),
                      mat4_scale(vec3_make(0.048f, 0.048f, 0.048f)))), 1.0f, 0.14f, 0.16f, 1.0f, 1.25f, 0.95f);
            draw_gloved_hand(r, vec3_add(base, vec3_make(0, -0.06f, 0.06f)));
            break;
        }
    }
}

/* ============================================================================
 * ATMOSPHERIC AMBIENT PARTICLES (SNOW, EMBERS, DUST MOTES, HAUNTED SPARKS)
 * ============================================================================ */

static void draw_ambient_particles(GlRenderer *r, const GameWorld *w, Vec3 cam_pos) {
    float pr = 1.0f, pg = 0.92f, pb = 0.65f, speed = 0.65f, dir_y = 1.0f, sz = 0.07f;
    switch (w->scene) {
        case SCENE_DAY01: pr = 0.92f; pg = 0.96f; pb = 1.00f; speed = 1.4f; dir_y = -1.0f; sz = 0.08f; break; /* Falling winter snow! */
        case SCENE_DAY02: pr = 1.00f; pg = 0.90f; pb = 0.58f; speed = 0.4f; dir_y =  0.4f; sz = 0.06f; break; /* Golden sunbeams dust */
        case SCENE_DAY03: pr = 0.92f; pg = 0.82f; pb = 0.62f; speed = 0.9f; dir_y =  0.2f; sz = 0.06f; break; /* Desert breeze sand */
        case SCENE_DAY04: pr = 1.00f; pg = 0.55f; pb = 0.22f; speed = 1.1f; dir_y =  1.0f; sz = 0.07f; break; /* Forge embers */
        case SCENE_DAY05: pr = 0.92f; pg = 0.28f; pb = 0.62f; speed = 0.6f; dir_y =  0.5f; sz = 0.07f; break; /* Crypt mist sparks */
        case SCENE_DAY06: pr = 1.00f; pg = 0.42f; pb = 0.14f; speed = 1.6f; dir_y =  1.0f; sz = 0.08f; break; /* Hellfire ash */
        default:          pr = 1.00f; pg = 0.88f; pb = 0.46f; speed = 0.5f; dir_y =  0.6f; sz = 0.07f; break; /* Festive hall sparkles */
    }
    for (int i = 0; i < 32; ++i) {
        float h1 = fmodf(sinf((float)i * 12.9898f) * 43758.5453f, 1.0f);
        float h2 = fmodf(sinf((float)i * 78.2330f) * 43758.5453f, 1.0f);
        float h3 = fmodf(sinf((float)i * 39.4250f) * 43758.5453f, 1.0f);
        if (h1 < 0) h1 += 1.0f;
        if (h2 < 0) h2 += 1.0f;
        if (h3 < 0) h3 += 1.0f;
        float drift = sinf(w->time_sec * 1.2f + (float)i) * 0.8f;
        float ox = (h1 - 0.5f) * 26.0f + drift;
        float oz = (h2 - 0.5f) * 26.0f + cosf(w->time_sec * 0.9f + (float)i) * 0.8f;
        float oy = fmodf(h3 * 12.0f + w->time_sec * speed * dir_y + 120.0f, 12.0f) - 2.0f;
        Vec3 pos = vec3_make(cam_pos.x + ox, sw_clampf(cam_pos.y + oy, 0.6f, 18.0f), cam_pos.z + oz);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(pos), mat4_scale(vec3_make(sz, sz, sz))),
                  pr, pg, pb, 0.78f, 1.1f, 0.2f);
    }
}

/* ============================================================================
 * MAIN 3D SCENE PASS
 * ============================================================================ */

static void render_scene_view(GlRenderer *r, const GameWorld *w, Mat4 proj, Mat4 view,
                              Vec3 cam_pos, float cam_yaw, int viewer_slot, Mat4 light_vp, Vec3 sun_dir) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* 0. Procedural Biome Skybox Pass (extracts camera basis directly from view & proj) */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(r->prog_sky);
    glUniform3f(glGetUniformLocation(r->prog_sky, "uCamRight"), view.m[0], view.m[4], view.m[8]);
    glUniform3f(glGetUniformLocation(r->prog_sky, "uCamUp"),    view.m[1], view.m[5], view.m[9]);
    glUniform3f(glGetUniformLocation(r->prog_sky, "uCamFwd"),  -view.m[2], -view.m[6], -view.m[10]);
    float inv_px = (fabsf(proj.m[0]) > 1e-5f) ? (1.0f / fabsf(proj.m[0])) : 1.0f;
    float inv_py = (fabsf(proj.m[5]) > 1e-5f) ? (1.0f / fabsf(proj.m[5])) : 1.0f;
    glUniform2f(glGetUniformLocation(r->prog_sky, "uInvProj"), inv_px, inv_py);
    glUniform1i(glGetUniformLocation(r->prog_sky, "uSceneId"), (int)w->scene);
    glUniform1f(glGetUniformLocation(r->prog_sky, "uTime"), w->time_sec);
    glBindVertexArray(r->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    /* Per-realm realistic hemisphere + directional sun + atmospheric fog palette */
    float sr = 0.78f, sg = 0.66f, sb = 0.50f;
    float sky_r = 0.34f, sky_g = 0.30f, sky_b = 0.36f;
    float gnd_r = 0.24f, gnd_g = 0.17f, gnd_b = 0.14f;
    float fr = 0.10f, fg = 0.08f, fb = 0.12f, fd = 0.010f;
    if (w->scene == SCENE_DAY01) {
        sr = 0.66f; sg = 0.70f; sb = 0.96f;
        sky_r = 0.26f; sky_g = 0.26f; sky_b = 0.44f;
        gnd_r = 0.22f; gnd_g = 0.14f; gnd_b = 0.26f;
        fr = 0.10f; fg = 0.07f; fb = 0.18f; fd = 0.012f;
    } else if (w->scene == SCENE_DAY02) {
        sr = 1.00f; sg = 0.88f; sb = 0.66f;
        sky_r = 0.34f; sky_g = 0.46f; sky_b = 0.58f;
        gnd_r = 0.28f; gnd_g = 0.22f; gnd_b = 0.16f;
        fr = 0.30f; fg = 0.34f; fb = 0.36f; fd = 0.010f;
    } else if (w->scene == SCENE_DAY03) {
        sr = 1.05f; sg = 0.98f; sb = 0.82f;
        sky_r = 0.44f; sky_g = 0.56f; sky_b = 0.72f;
        gnd_r = 0.44f; gnd_g = 0.36f; gnd_b = 0.24f;
        fr = 0.68f; fg = 0.66f; fb = 0.58f; fd = 0.006f;
    } else if (w->scene == SCENE_DAY04) {
        sr = 0.76f; sg = 0.58f; sb = 0.42f;
        sky_r = 0.28f; sky_g = 0.22f; sky_b = 0.30f;
        gnd_r = 0.24f; gnd_g = 0.16f; gnd_b = 0.12f;
        fr = 0.14f; fg = 0.09f; fb = 0.10f; fd = 0.013f;
    } else if (w->scene == SCENE_DAY05) {
        sr = 0.72f; sg = 0.36f; sb = 0.44f;
        sky_r = 0.24f; sky_g = 0.16f; sky_b = 0.32f;
        gnd_r = 0.20f; gnd_g = 0.12f; gnd_b = 0.16f;
        fr = 0.14f; fg = 0.06f; fb = 0.15f; fd = 0.015f;
    } else if (w->scene == SCENE_DAY06) {
        sr = 0.88f; sg = 0.64f; sb = 0.54f;
        sky_r = 0.36f; sky_g = 0.24f; sky_b = 0.24f;
        gnd_r = 0.32f; gnd_g = 0.16f; gnd_b = 0.12f;
        fr = 0.20f; fg = 0.08f; fb = 0.06f; fd = 0.012f;
    }

    glUseProgram(r->prog_world);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uProj"), 1, GL_FALSE, proj.m);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uLightVP"), 1, GL_FALSE, light_vp.m);
    glUniform3f(glGetUniformLocation(r->prog_world, "uCamPos"), cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunDir"), sun_dir.x, sun_dir.y, sun_dir.z);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSunColor"), sr, sg, sb);
    glUniform3f(glGetUniformLocation(r->prog_world, "uSkyAmbient"), sky_r, sky_g, sky_b);
    glUniform3f(glGetUniformLocation(r->prog_world, "uGroundAmbient"), gnd_r, gnd_g, gnd_b);
    glUniform3f(glGetUniformLocation(r->prog_world, "uFogColor"), fr, fg, fb);
    glUniform1f(glGetUniformLocation(r->prog_world, "uFogDensity"), fd);
    glUniform4f(glGetUniformLocation(r->prog_world, "uTint"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(r->prog_world, "uMeshEmissive"), 0.0f);
    glUniform1f(glGetUniformLocation(r->prog_world, "uMeshGloss"), 0.25f);

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

    /* 1. Draw Chunked Normal-Mapped 64x64 Textured Voxel World */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->voxel_atlas_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r->shadow_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uShadowMap"), 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, r->voxel_normal_tex);
    glUniform1i(glGetUniformLocation(r->prog_world, "uNormalMap"), 2);

    Mat4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(r->prog_world, "uModel"), 1, GL_FALSE, ident.m);
    glUniform1i(glGetUniformLocation(r->prog_world, "uUseTex"), 2);
    draw_all_chunks(r);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, r->flat_normal_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->white_tex);

    Mat4 rx90 = mat4_rotate_x(SW_PI * 0.5f);

    /* 2. Grand Festive Hall Sculpted Props OR Return Portal */
    if (w->scene == SCENE_HUB) {
        /* Lush 4-Tier Sculpted Conical Pine Boughs forming a classic tapered Christmas Tree! */
        static const float tiers[4][4] = {
            {5.8f,  13.2f, 4.8f, 0.08f},
            {8.2f,  10.8f, 4.4f, 0.10f},
            {10.5f,  8.2f, 4.0f, 0.12f},
            {12.6f,  5.4f, 3.6f, 0.15f}
        };
        for (int t = 0; t < 4; ++t) {
            draw_mesh(r, &r->mesh_cone, mat4_mul(mat4_translate(vec3_make(64.5f, tiers[t][0], 64.5f)),
                      mat4_scale(vec3_make(tiers[t][1], tiers[t][2], tiers[t][1]))),
                      0.07f, 0.32f + (float)t * 0.04f, 0.12f, 1.0f, 0.02f, 0.24f);
        }

        /* Multi-Point Sculpted 3D Golden Star Topper */
        Mat4 star_base = mat4_mul(mat4_translate(vec3_make(64.5f, 14.8f, 64.5f)), mat4_rotate_y(w->time_sec * 1.1f));
        draw_mesh(r, &r->mesh_sphere, mat4_mul(star_base, mat4_scale(vec3_make(0.85f, 0.85f, 0.85f))),
                  1.0f, 0.92f, 0.35f, 1.0f, 1.20f, 0.95f);
        for (int s = 0; s < 6; ++s) {
            float ang = (float)s * (SW_PI / 3.0f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(star_base, mat4_mul(mat4_rotate_z_local(ang), mat4_mul(mat4_translate(vec3_make(0, 0.65f, 0)),
                      mat4_scale(vec3_make(0.40f, 1.25f, 0.40f))))), 1.0f, 0.86f, 0.22f, 1.0f, 1.10f, 0.92f);
        }

        /* 32 Glossy Spherical Christmas Baubles + 44 Golden Fairy Lights draped cleanly along the pine tiers! */
        static const float orn_cols[4][3] = {
            {0.96f, 0.14f, 0.20f}, {1.00f, 0.82f, 0.18f}, {0.18f, 0.86f, 1.00f}, {0.98f, 0.28f, 0.84f}
        };
        for (int i = 0; i < 32; ++i) {
            float frac = (float)i / 32.0f;
            float y = 3.7f + frac * 9.8f;
            float rad = 6.35f - frac * 4.55f;
            float ang = (float)i * 0.95f + 0.4f;
            float ox = 64.5f + cosf(ang) * rad;
            float oz = 64.5f + sinf(ang) * rad;
            bool earned = (i < 24) && ((w->shard_mask & (1u << (unsigned)i)) != 0u);
            const float *c = orn_cols[i & 3];
            float tw = earned ? (0.95f + 0.30f * sinf(w->time_sec * 3.5f + (float)i)) : (0.50f + 0.22f * sinf(w->time_sec * 2.2f + (float)i));
            float sz = earned ? 0.58f : 0.46f;
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_make(ox, y, oz)), mat4_scale(vec3_make(sz, sz, sz))),
                      c[0], c[1], c[2], 1.0f, tw, 0.96f);
        }
        for (int i = 0; i < 44; ++i) {
            float frac = (float)i / 44.0f;
            float y = 3.6f + frac * 10.0f;
            float rad = 6.50f - frac * 4.65f;
            float ang = (float)i * 0.65f + 1.1f;
            float ox = 64.5f + cosf(ang) * rad;
            float oz = 64.5f + sinf(ang) * rad;
            float tw = 0.90f + 0.45f * sinf(w->time_sec * 4.5f + (float)i * 1.4f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_make(ox, y, oz)), mat4_scale(vec3_make(0.22f, 0.22f, 0.22f))),
                      1.0f, 0.92f, 0.48f, 1.0f, tw, 0.95f);
        }

        /* Animated 3D Conical Fireplace Flames & Mantle Candles */
        for (int f = 0; f < 6; ++f) {
            float fx = 61.5f + (float)f * 1.15f;
            float flk = 0.78f + 0.32f * sinf(w->time_sec * 8.5f + (float)f * 1.7f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(mat4_translate(vec3_make(fx, 1.3f + flk * 0.7f, 28.9f)),
                      mat4_scale(vec3_make(0.65f, 1.6f * flk, 0.65f))), 1.0f, 0.42f, 0.08f, 0.92f, 1.35f, 0.2f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(mat4_translate(vec3_make(fx, 1.2f + flk * 0.45f, 29.0f)),
                      mat4_scale(vec3_make(0.36f, 1.0f * flk, 0.36f))), 1.0f, 0.90f, 0.32f, 0.95f, 1.55f, 0.2f);
        }
        for (int c = 0; c < 2; ++c) {
            float cx = (c == 0) ? 58.5f : 70.5f;
            draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_make(cx, 7.25f, 30.5f)), mat4_scale(vec3_make(0.22f, 0.65f, 0.22f))),
                      0.96f, 0.92f, 0.82f, 1.0f, 0.12f, 0.45f);
            draw_mesh(r, &r->mesh_cone, mat4_mul(mat4_translate(vec3_make(cx, 7.75f, 30.5f)), mat4_scale(vec3_make(0.18f, 0.38f, 0.18f))),
                      1.0f, 0.82f, 0.26f, 0.95f, 1.45f, 0.2f);
        }

        /* 24 Portal Energy Fields + Glowing Keystone Orbs */
        for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
            const PortalMeta *meta = swieta_portal_meta(w->portals[i].day);
            bool unlocked = swieta_portal_is_unlocked(w->portals[i].day, w->now, w->unlock_all);
            bool done = (w->shard_mask & (1u << (unsigned)i)) != 0u;
            float pr = unlocked ? (meta ? meta->color_r : 0.4f) : 0.28f;
            float pg = unlocked ? (meta ? meta->color_g : 0.7f) : 0.38f;
            float pb = unlocked ? (meta ? meta->color_b : 1.0f) : 0.52f;
            if (done) { pr = 1.0f; pg = 0.86f; pb = 0.25f; }
            Vec3 p = vec3_add(w->portals[i].pos, vec3_make(0.0f, 2.0f, 0.5f));
            bool x_wall = (w->portals[i].pos.x <= 30.0f || w->portals[i].pos.x >= 98.0f);
            Vec3 sc = x_wall ? vec3_make(0.16f, 3.65f, 2.05f) : vec3_make(2.05f, 3.65f, 0.16f);
            float pulse = unlocked ? (0.78f + 0.22f * sinf(w->time_sec * 3.0f + (float)i)) : 0.12f;
            draw_mesh(r, &r->mesh_box, mat4_mul(mat4_translate(p), mat4_scale(sc)),
                      pr, pg, pb, unlocked ? 0.78f : 0.42f, pulse, 0.90f);
            /* Sculpted Keystone Crystal Sphere above each portal arch */
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_add(p, vec3_make(0, 2.55f, 0))),
                      mat4_scale(vec3_make(0.55f, 0.55f, 0.55f))), pr, pg, pb, 1.0f, unlocked ? 1.1f : 0.18f, 0.96f);
        }
    } else {
        Vec3 rp = vec3_add(w->return_portal_pos, vec3_make(0, 1.35f, 0));
        draw_billboard_sprite(r, rp, 2.1f, 3.0f, SPR_FX_ORB, 1, cam_yaw, 0.35f, 0.92f, 1.0f, 1.05f);
    }

    /* Ribbed Cylindrical Explosive Barrels (Crimson drum + steel rims + glowing green top) */
    for (int i = 0; i < w->barrel_count; ++i) {
        if (!w->barrels[i].active) continue;
        Vec3 bp = w->barrels[i].pos;
        draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 0.64f, 0))),
                  mat4_scale(vec3_make(0.76f, 1.24f, 0.76f))), 0.84f, 0.15f, 0.14f, 1.0f, 0.08f, 0.65f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 0.18f, 0))),
                  mat4_scale(vec3_make(0.82f, 0.08f, 0.82f))), 0.32f, 0.34f, 0.38f, 1.0f, 0.04f, 0.90f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 0.64f, 0))),
                  mat4_scale(vec3_make(0.81f, 0.14f, 0.81f))), 0.92f, 0.78f, 0.16f, 1.0f, 0.12f, 0.55f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 1.12f, 0))),
                  mat4_scale(vec3_make(0.82f, 0.08f, 0.82f))), 0.32f, 0.34f, 0.38f, 1.0f, 0.04f, 0.90f);
        draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(vec3_add(bp, vec3_make(0, 1.27f, 0))),
                  mat4_scale(vec3_make(0.64f, 0.05f, 0.64f))), 0.32f, 1.0f, 0.28f, 1.0f, 0.95f, 0.92f);
    }

    /* Craggy Multi-Sphere Rolling Tomb Raider Boulders */
    for (int i = 0; i < w->boulder_count; ++i) {
        if (!w->boulders[i].active) continue;
        float rot = w->boulders[i].rolling ? w->time_sec * 4.8f : 0.0f;
        Mat4 broot = mat4_mul(mat4_translate(w->boulders[i].pos), mat4_rotate_x(rot));
        draw_mesh(r, &r->mesh_sphere, mat4_mul(broot, mat4_scale(vec3_make(2.45f, 2.38f, 2.45f))),
                  0.58f, 0.50f, 0.40f, 1.0f, 0.03f, 0.22f);
        static const float lobes[4][3] = {{0.55f, 0.35f, 0.25f}, {-0.52f, -0.32f, 0.38f}, {0.20f, -0.45f, -0.48f}, {-0.38f, 0.48f, -0.32f}};
        for (int k = 0; k < 4; ++k) {
            draw_mesh(r, &r->mesh_sphere, mat4_mul(broot, mat4_mul(mat4_translate(vec3_make(lobes[k][0], lobes[k][1], lobes[k][2])),
                      mat4_scale(vec3_make(1.45f, 1.35f, 1.45f)))), 0.50f, 0.43f, 0.35f, 1.0f, 0.02f, 0.18f);
        }
    }

    /* Glowing Magnesium Flares & Billowing Multi-Sphere CS Smoke Clouds */
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        const FlareLight *fl = &w->flares[i];
        if (!fl->active) continue;
        if (fl->is_smoke) {
            static const float puffs[5][4] = {
                {0.0f, 1.55f, 0.0f, 3.6f}, {1.25f, 1.25f, 0.75f, 2.8f}, {-1.20f, 1.30f, -0.65f, 2.7f},
                {0.65f, 2.15f, -0.95f, 2.6f}, {-0.75f, 1.95f, 0.90f, 2.5f}
            };
            for (int k = 0; k < 5; ++k) {
                float p = puffs[k][3] * (0.94f + 0.08f * sinf(w->time_sec * 1.8f + (float)k * 1.3f));
                draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_add(fl->pos, vec3_make(puffs[k][0], puffs[k][1], puffs[k][2]))),
                          mat4_scale(vec3_make(p, p * 0.85f, p))), 0.80f, 0.83f, 0.88f, 0.52f, 0.08f, 0.08f);
            }
        } else {
            draw_mesh(r, &r->mesh_cyl, mat4_mul(mat4_translate(fl->pos), mat4_mul(rx90, mat4_scale(vec3_make(0.14f, 0.42f, 0.14f)))),
                      0.22f, 0.65f, 0.30f, 1.0f, 0.25f, 0.55f);
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_add(fl->pos, vec3_make(0, 0.08f, 0.20f))),
                      mat4_scale(vec3_make(0.28f, 0.28f, 0.28f))), fl->r, fl->g, fl->b, 1.0f, 1.45f, 0.9f);
        }
    }

    /* Quake Thunderbolt live crackling lightning arc */
    for (int i = 0; i < w->player_count; ++i) {
        if (!w->players[i].active || w->players[i].lightning_timer <= 0.0f) continue;
        Vec3 a = vec3_add(player_eye_pos(&w->players[i]), vec3_scale(player_forward(&w->players[i]), 0.8f));
        Vec3 b = w->players[i].lightning_end;
        for (int s = 1; s <= 10; ++s) {
            Vec3 pt = vec3_lerp(a, b, (float)s / 10.0f);
            float jit = sinf(w->time_sec * 52.0f + (float)s * 2.3f) * 0.16f;
            float j2  = cosf(w->time_sec * 44.0f + (float)s * 1.9f) * 0.12f;
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_add(pt, vec3_make(jit, j2, 0))),
                      mat4_scale(vec3_make(0.22f, 0.22f, 0.22f))), 0.48f, 0.94f, 1.0f, 0.95f, 1.50f, 0.9f);
        }
    }

    /* Armed Laser Tripmines + Glowing Cylindrical Red Laser Beams */
    for (int i = 0; i < WORLD_MAX_TRIPMINES; ++i) {
        const Tripmine *tm = &w->tripmines[i];
        if (!tm->active) continue;
        draw_mesh(r, &r->mesh_box, mat4_mul(mat4_translate(tm->pos), mat4_scale(vec3_make(0.28f, 0.22f, 0.28f))),
                  0.30f, 0.34f, 0.40f, 1.0f, 0.10f, 0.85f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(vec3_add(tm->pos, vec3_scale(tm->normal, 0.12f))),
                  mat4_scale(vec3_make(0.11f, 0.11f, 0.11f))), 1.0f, 0.12f, 0.16f, 1.0f, 1.35f, 0.95f);
        if (tm->arm_timer <= 0.0f) {
            Vec3 mid = vec3_lerp(tm->pos, tm->beam_end, 0.5f);
            float blen = vec3_dist(tm->pos, tm->beam_end);
            Mat4 bm = mat4_translate(mid);
            if (fabsf(tm->normal.x) > 0.5f) bm = mat4_mul(bm, mat4_mul(mat4_rotate_z_local(SW_PI * 0.5f), mat4_scale(vec3_make(0.045f, blen, 0.045f))));
            else if (fabsf(tm->normal.z) > 0.5f) bm = mat4_mul(bm, mat4_mul(rx90, mat4_scale(vec3_make(0.045f, blen, 0.045f))));
            else bm = mat4_mul(bm, mat4_scale(vec3_make(0.045f, blen, 0.045f)));
            draw_mesh(r, &r->mesh_cyl, bm, 1.0f, 0.12f, 0.16f, 0.88f, 1.45f, 0.9f);
        }
    }

    /* Sculpted Co-op Player Avatars with Festive Santa Hats */
    for (int i = 0; i < w->player_count; ++i) {
        if (i == viewer_slot || !w->players[i].active || w->players[i].hp <= 0) continue;
        Vec3 pp = w->players[i].pos;
        float sc = (w->players[i].self_shrunk_timer > 0.0f) ? 0.36f : 1.0f;
        static const float colors[4][3] = {{0.22f,0.72f,1.0f},{1.0f,0.34f,0.34f},{0.34f,0.92f,0.44f},{1.0f,0.84f,0.24f}};
        Mat4 pr = mat4_mul(mat4_translate(pp), mat4_rotate_y(-w->players[i].yaw));
        draw_mesh(r, &r->mesh_cyl, mat4_mul(pr, mat4_mul(mat4_translate(vec3_make(0, 0.78f * sc, 0)),
                  mat4_scale(vec3_make(0.62f * sc, 1.12f * sc, 0.48f * sc)))), colors[i][0], colors[i][1], colors[i][2], 1.0f, 0.08f, 0.55f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(pr, mat4_mul(mat4_translate(vec3_make(0, 1.48f * sc, 0)),
                  mat4_scale(vec3_make(0.42f * sc, 0.42f * sc, 0.42f * sc)))), 0.92f, 0.78f, 0.66f, 1.0f, 0.05f, 0.30f);
        /* Festive Santa Hat */
        draw_mesh(r, &r->mesh_cone, mat4_mul(pr, mat4_mul(mat4_translate(vec3_make(0, 1.82f * sc, 0)),
                  mat4_scale(vec3_make(0.46f * sc, 0.44f * sc, 0.46f * sc)))), 0.90f, 0.14f, 0.18f, 1.0f, 0.12f, 0.30f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(pr, mat4_mul(mat4_translate(vec3_make(0, 2.04f * sc, 0)),
                  mat4_scale(vec3_make(0.14f * sc, 0.14f * sc, 0.14f * sc)))), 0.98f, 0.98f, 1.0f, 1.0f, 0.15f, 0.20f);
    }

    /* 3. Sculpted 3D Bosses & 64x64 2-Frame Animated Enemy Sprites */
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float fl = (e->hit_flash > 0.0f) ? 1.0f : 0.0f;
        float burn = (e->burn_timer > 0.0f) ? 0.65f : 0.0f;
        float blind = (e->blind_timer > 0.0f) ? 0.8f : 0.0f;

        if (world_enemy_is_boss(e->kind)) {
            draw_boss_3d(r, w, e, fl, burn);
        } else {
            float sc = (e->shrunk_timer > 0.0f) ? 0.36f : 1.0f;
            int col = SPR_ALIEN_TROOPER;
            int frame = ((int)(e->anim_time * 4.5f)) & 1;
            float cr = 1.0f, cg = 1.0f, cb = 1.0f;
            float w_sz = 1.38f, h_sz = 1.92f;
            switch (e->kind) {
                case ENEMY_TROOPER:          col = SPR_ALIEN_TROOPER; break;
                case ENEMY_PIGGUARD:         col = SPR_PIG_GUARD; w_sz = 1.48f; break;
                case ENEMY_OCTABRAIN:        col = SPR_OCTABRAIN; w_sz = 1.50f; h_sz = 1.72f; break;
                case ENEMY_TURRET:           col = SPR_TURRET; w_sz = 1.28f; h_sz = 1.55f; break;
                case ENEMY_WOLF:             col = SPR_WOLF; w_sz = 1.35f; h_sz = 1.15f; break;
                case ENEMY_BAT:              col = SPR_BAT; w_sz = 1.28f; h_sz = 1.10f; break;
                case ENEMY_STONE_GUARD:      col = SPR_STONE_GUARD; w_sz = 1.52f; h_sz = 2.08f; break;
                case ENEMY_SYNDICATE_RIFLE:  col = SPR_SYNDICATE; break;
                case ENEMY_SYNDICATE_SNIPER: col = SPR_SYNDICATE; cr = 0.78f; cg = 0.90f; cb = 1.00f; break;
                case ENEMY_OGRE:             col = SPR_OGRE_KNIGHT; w_sz = 1.58f; h_sz = 2.05f; break;
                case ENEMY_FIEND:            col = SPR_FIEND_GHOUL; cr = 1.00f; cg = 0.80f; cb = 0.72f; w_sz = 1.50f; h_sz = 1.75f; break;
                case ENEMY_DEATH_KNIGHT:     col = SPR_OGRE_KNIGHT; cr = 0.72f; cg = 0.74f; cb = 0.94f; w_sz = 1.52f; h_sz = 2.05f; break;
                case ENEMY_CULTIST:          col = SPR_CULTIST; break;
                case ENEMY_GHOUL:            col = SPR_FIEND_GHOUL; cr = 0.76f; cg = 0.96f; cb = 0.78f; break;
                case ENEMY_GARGOYLE:         col = SPR_GARGOYLE; w_sz = 1.55f; h_sz = 1.85f; break;
                case ENEMY_IMP:              col = SPR_IMP; break;
                case ENEMY_PINKY:            col = SPR_PINKY; w_sz = 1.60f; h_sz = 1.82f; break;
                case ENEMY_CACODEMON:        col = SPR_CACODEMON; w_sz = 1.75f; h_sz = 1.75f; break;
                default: break;
            }
            if (burn > 0.0f) { cr = 1.0f; cg = 0.56f; cb = 0.18f; }
            if (blind > 0.0f){ cr = 1.0f; cg = 1.0f;  cb = 1.0f;  }
            draw_billboard_sprite(r, e->pos, w_sz * sc, h_sz * sc, col, frame, cam_yaw,
                                  cr, cg, cb, sw_clampf(fl * 0.75f + burn + blind, 0.0f, 1.25f));
        }
    }

    if (w->decoy.active) {
        int df = ((int)(w->time_sec * 8.0f)) & 1;
        draw_billboard_sprite(r, w->decoy.pos, 1.25f, 1.85f, SPR_HOLO_DECOY, df, cam_yaw, 0.45f, 0.96f, 1.0f, 1.05f);
    }

    /* 4. Crisp 64x64 Pickups */
    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        const Pickup *pk = &w->pickups[i];
        if (!pk->active) continue;
        Vec3 pos = vec3_add(pk->pos, vec3_make(0, 0.16f * sinf(pk->bob_phase), 0));
        int col = SPR_FX_ORB, row = 0;
        float cr = 1.0f, cg = 1.0f, cb = 1.0f;
        float sz = 0.92f;
        switch (pk->kind) {
            case PICKUP_MEDKIT:      col = SPR_MEDKIT; row = 0; break;
            case PICKUP_SHOTGUN:
            case PICKUP_WEAPON_GRANT:col = SPR_SHARD_WEAPON; row = 1; sz = 1.10f; cr = 1.0f; cg = 0.96f; cb = 0.85f; break;
            case PICKUP_KEY_BLUE:    col = SPR_KEYCARD; row = 0; cr = 0.25f; cg = 0.68f; cb = 1.0f; break;
            case PICKUP_KEY_YELLOW:  col = SPR_KEYCARD; row = 0; cr = 1.0f;  cg = 0.92f; cb = 0.22f; break;
            case PICKUP_KEY_RED:     col = SPR_KEYCARD; row = 0; cr = 1.0f;  cg = 0.25f; cb = 0.28f; break;
            case PICKUP_SHARD:       col = SPR_SHARD_WEAPON; row = 0; sz = 1.15f; break;
            case PICKUP_QUAD:        col = SPR_SHARD_WEAPON; row = 0; sz = 1.05f; cr = 0.25f; cg = 0.68f; cb = 1.0f; break;
            case PICKUP_BERSERK:     col = SPR_MEDKIT; row = 0; cr = 1.0f;  cg = 0.22f; cb = 0.22f; break;
            case PICKUP_RADSUIT:     col = SPR_FX_ORB; row = 0; cr = 0.30f; cg = 1.0f;  cb = 0.35f; break;
            case PICKUP_CASH:        col = SPR_SHARD_WEAPON; row = 0; cr = 0.35f; cg = 1.0f; cb = 0.45f; break;
            case PICKUP_PIPEBOMB:    cr = 1.0f; cg = 0.40f; cb = 0.22f; break;
            case PICKUP_TRIPMINE:    cr = 1.0f; cg = 0.20f; cb = 0.30f; break;
            case PICKUP_ARMOR:       cr = 0.35f; cg = 0.88f; cb = 1.0f; break;
            case PICKUP_SHRINK_AMMO: cr = 0.30f; cg = 1.0f;  cb = 0.42f; break;
            default: break;
        }
        draw_billboard_sprite(r, pos, sz, sz, col, row, cam_yaw, cr, cg, cb, 0.72f);
    }

    /* 5. Sculpted 3D Projectiles & Glowing Orbs */
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        const Projectile *pr = &w->projectiles[i];
        if (!pr->active) continue;
        if (pr->kind == PROJ_PIPEBOMB || pr->kind == PROJ_TNT_BUNDLE || pr->kind == PROJ_TNT_PROX ||
            pr->kind == PROJ_MOLOTOV || pr->kind == PROJ_FLASHBANG || pr->kind == PROJ_ENEMY_GRENADE) {
            float cr = (pr->kind == PROJ_FLASHBANG) ? 0.85f : 0.94f;
            float cg = (pr->kind == PROJ_FLASHBANG) ? 0.92f : 0.22f;
            float cb = (pr->kind == PROJ_FLASHBANG) ? 1.00f : 0.16f;
            Mat4 pm = mat4_mul(mat4_translate(pr->pos), mat4_mul(mat4_rotate_y(w->time_sec * 6.0f),
                      mat4_mul(rx90, mat4_scale(vec3_make(0.22f, 0.42f, 0.22f)))));
            draw_mesh(r, &r->mesh_cyl, pm, cr, cg, cb, 1.0f, 0.55f, 0.75f);
        } else if (pr->kind == PROJ_PLAYER_ROCKET || pr->kind == PROJ_BOSS_ROCKET) {
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(pr->pos), mat4_scale(vec3_make(0.36f, 0.36f, 0.36f))),
                      1.0f, 0.45f, 0.12f, 1.0f, 1.35f, 0.8f);
            draw_billboard_sprite(r, vec3_sub(pr->pos, vec3_make(0, 0.35f, 0)), 0.75f, 0.75f, SPR_FX_ORB, 0, cam_yaw, 1.0f, 0.65f, 0.20f, 1.25f);
        } else if (pr->kind == PROJ_SHRINK_ORB || pr->kind == PROJ_BFG_ORB) {
            float sz = (pr->kind == PROJ_BFG_ORB) ? 1.40f : 0.65f;
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(pr->pos), mat4_scale(vec3_make(sz * 0.45f, sz * 0.45f, sz * 0.45f))),
                      0.30f, 1.0f, 0.45f, 1.0f, 1.45f, 0.95f);
            draw_billboard_sprite(r, vec3_sub(pr->pos, vec3_make(0, sz * 0.5f, 0)), sz, sz, SPR_FX_ORB, 0, cam_yaw, 0.25f, 1.0f, 0.38f, 1.25f);
        } else if (pr->kind == PROJ_OCTA_WAVE) {
            draw_billboard_sprite(r, vec3_sub(pr->pos, vec3_make(0, 0.38f, 0)), 0.76f, 0.76f, SPR_FX_ORB, 0, cam_yaw, 0.85f, 0.30f, 1.0f, 1.15f);
        } else if (pr->kind == PROJ_NAIL) {
            draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(pr->pos), mat4_scale(vec3_make(0.16f, 0.16f, 0.16f))),
                      0.96f, 0.82f, 0.42f, 1.0f, 0.85f, 0.92f);
        } else {
            draw_billboard_sprite(r, vec3_sub(pr->pos, vec3_make(0, 0.34f, 0)), 0.68f, 0.68f, SPR_FX_ORB, 0, cam_yaw, 1.0f, 0.46f, 0.18f, 1.20f);
        }
    }

    /* 6. Explosions & Atmospheric Ambient Particles */
    for (int i = 0; i < 8; ++i) {
        if (!w->explosions[i].active) continue;
        float t = w->explosions[i].timer / 0.45f;
        float sz = w->explosions[i].radius * (1.42f - t * 0.5f);
        draw_mesh(r, &r->mesh_sphere, mat4_mul(mat4_translate(w->explosions[i].pos),
                  mat4_scale(vec3_make(sz * 0.55f, sz * 0.55f, sz * 0.55f))),
                  w->explosions[i].r, w->explosions[i].g, w->explosions[i].b, t * 0.85f, 1.55f, 0.2f);
        draw_billboard_sprite(r, vec3_sub(w->explosions[i].pos, vec3_make(0, sz * 0.45f, 0)),
                              sz, sz, SPR_FX_ORB, 0, cam_yaw, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b, 1.35f);
    }

    draw_ambient_particles(r, w, cam_pos);

    /* 7. Sculpted First-Person Weapon Viewmodel */
    if (viewer_slot >= 0 && viewer_slot < w->player_count &&
        (w->scene == SCENE_DAY01 || (w->scene >= SCENE_DAY02 && w->scene <= SCENE_DAY06))) {
        const Player *p = &w->players[viewer_slot];
        if (p->active && p->hp > 0 && !p->zoomed) {
            draw_weapon_viewmodel(r, p);
        }
    }
}

/* ============================================================================
 * 2D HUD BATCHER, MINIMAP & ADAPTIVE MOBILE TOUCH OVERLAY
 * ============================================================================ */

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
    static const uint8_t kDollar[5]={2,7,6,3,7}, kLParen[5]={1,2,2,2,1}, kRParen[5]={4,2,2,2,4}, kAmp[5]={2,5,2,5,3}, kPlus[5]={0,2,7,2,0}, kApos[5]={2,2,0,0,0};
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
        case '(': return kLParen; case ')': return kRParen; case '&': return kAmp;
        case '+': return kPlus;  case '\'': return kApos;
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
    hud_rect(mx, my, mw, 0.003f * aspect, 0.2f, 0.85f, 1.0f, 0.75f);
    hud_rect(mx, my + mh - 0.003f * aspect, mw, 0.003f * aspect, 0.2f, 0.85f, 1.0f, 0.75f);

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
    if (p->in_water)              hud_rect(0, 0, 1, 1, 0.08f, 0.42f, 0.68f, 0.22f);
    if (p->quad_timer > 0.0f)     hud_rect(0, 0, 1, 1, 0.16f, 0.42f, 1.00f, 0.18f);
    if (p->berserk_timer > 0.0f)  hud_rect(0, 0, 1, 1, 0.95f, 0.12f, 0.12f, 0.22f);
    if (p->rad_suit_timer > 0.0f) hud_rect(0, 0, 1, 1, 0.15f, 0.88f, 0.25f, 0.14f);
    if (p->damage_flash > 0.0f)   hud_rect(0, 0, 1, 1, 0.9f, 0.1f, 0.1f, p->damage_flash * 0.6f);
    if (p->stomp_flash > 0.0f)    hud_rect(0, 0, 1, 1, 0.2f, 1.0f, 0.35f, p->stomp_flash * 0.45f);
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
        hud_rect(0.025f, banner_y, 0.95f, 0.0025f * aspect, 1.0f, 0.84f, 0.32f, 0.65f);
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
    hud_rect(0.02f, bar_y, 0.96f, 0.0025f * aspect, 0.24f, 0.88f, 1.0f, 0.65f);

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
        Mat4 proj = mat4_perspective(fovy * SW_PI / 180.0f, aspect, 0.1f, 120.0f);
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
