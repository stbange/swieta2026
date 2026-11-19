#include "render/retro_art.h"
#include "render/voxel_mesher.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint8_t u8_clamp(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return (uint8_t)(v + 0.5f);
}

static float hash2_wrap(int x, int y, int period, int seed) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    uint32_t n = (uint32_t)(x * 374761393 + y * 668265263 + seed * 1442695041);
    n = (n ^ (n >> 13)) * 1274126177u;
    return (float)((n ^ (n >> 16)) & 0xFFFFu) / 65535.0f;
}

static float vnoise_wrap(float fx, float fy, int period, int seed) {
    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    float tx = fx - (float)x0;
    float ty = fy - (float)y0;
    float ux = tx * tx * (3.0f - 2.0f * tx);
    float uy = ty * ty * (3.0f - 2.0f * ty);
    float a = hash2_wrap(x0,     y0,     period, seed);
    float b = hash2_wrap(x0 + 1, y0,     period, seed);
    float c = hash2_wrap(x0,     y0 + 1, period, seed);
    float d = hash2_wrap(x0 + 1, y0 + 1, period, seed);
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uy;
}

static float fbm_tile(int x, int y, int base_freq, int seed) {
    float sum = 0.0f;
    float amp = 0.55f;
    float norm = 0.0f;
    int freq = base_freq;
    for (int oct = 0; oct < 3; ++oct) {
        float fx = ((float)x / (float)RETRO_VOX_TILE_PX) * (float)freq;
        float fy = ((float)y / (float)RETRO_VOX_TILE_PX) * (float)freq;
        sum += vnoise_wrap(fx, fy, freq, seed + oct * 97) * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2;
    }
    return sum / norm;
}

/* ============================================================================
 * 1024x192 (16x3 tiles of 64x64) MULTI-OCTAVE DIFFUSE + NORMAL/GLOSS ATLAS
 * ============================================================================ */
void retro_art_generate_voxel_atlases(uint8_t **out_diffuse, uint8_t **out_normal, int *out_w, int *out_h) {
    const int T = RETRO_VOX_TILE_PX;
    const int W = VOX_ATLAS_COLS * T; /* 1024 */
    const int H = VOX_ATLAS_ROWS * T; /* 192 */
    uint8_t *diff = (uint8_t *)calloc((size_t)(W * H * 4), 1);
    uint8_t *norm = (uint8_t *)calloc((size_t)(W * H * 4), 1);
    if (!diff || !norm) {
        free(diff);
        free(norm);
        return;
    }

    float (*hmap)[RETRO_VOX_TILE_PX] = (float (*)[RETRO_VOX_TILE_PX])calloc(RETRO_VOX_TILE_PX, sizeof(*hmap));
    float (*gmap)[RETRO_VOX_TILE_PX] = (float (*)[RETRO_VOX_TILE_PX])calloc(RETRO_VOX_TILE_PX, sizeof(*gmap));
    if (!hmap || !gmap) {
        free(diff); free(norm); free(hmap); free(gmap);
        return;
    }

    for (int tile = 0; tile < VOX_TYPE_COUNT; ++tile) {
        int tx = (tile % VOX_ATLAS_COLS) * T;
        int ty = (tile / VOX_ATLAS_COLS) * T;
        float bump_strength = 2.6f;

        for (int y = 0; y < T; ++y) {
            for (int x = 0; x < T; ++x) {
                float n1 = fbm_tile(x, y, 4, tile * 17 + 3);
                float n2 = fbm_tile(x, y, 8, tile * 31 + 11);
                float grain = (n2 - 0.5f) * 28.0f;
                float cr = 160.0f, cg = 160.0f, cb = 160.0f;
                float ht = n1 * 0.5f + n2 * 0.3f;
                float gloss = 0.12f;

                switch (tile) {
                    case VOX_STONE: {
                        int row = y / 16;
                        int bx = (x + (row & 1) * 16) % 32;
                        int by = y % 16;
                        bool mortar = (bx < 2 || by < 2);
                        bool bevel = (!mortar && (bx == 2 || by == 2 || bx == 31 || by == 15));
                        float bvar = hash2_wrap((x + (row & 1) * 16) / 32, row, 8, 11) * 24.0f - 12.0f;
                        if (mortar) { cr = 58; cg = 60; cb = 66; ht = 0.08f; }
                        else {
                            float base = 118.0f + bvar + grain + (bevel ? -12.0f : 0.0f);
                            cr = base; cg = base + 3.0f; cb = base + 9.0f;
                            ht = (bevel ? 0.52f : 0.80f) + n2 * 0.18f;
                            gloss = 0.16f;
                        }
                        break;
                    }
                    case VOX_WOOD: {
                        int plank = y / 16;
                        int py = y % 16;
                        float wave = sinf((float)x * 0.12f + n1 * 5.0f + (float)plank * 2.1f);
                        bool groove = (py == 0 || py == 15);
                        bool nail = ((x == 6 || x == 57) && (py == 4 || py == 11));
                        float pvar = hash2_wrap(plank, 1, 8, 23) * 20.0f - 10.0f;
                        if (groove) { cr = 54; cg = 32; cb = 18; ht = 0.12f; }
                        else if (nail) { cr = 72; cg = 74; cb = 78; ht = 0.95f; gloss = 0.55f; }
                        else {
                            float streak = wave * 12.0f + grain * 0.6f + pvar;
                            cr = 132.0f + streak; cg = 80.0f + streak * 0.65f; cb = 44.0f + streak * 0.35f;
                            ht = 0.65f + wave * 0.14f + n2 * 0.12f;
                            gloss = 0.22f;
                        }
                        break;
                    }
                    case VOX_SNOW: {
                        float drift = n1 * 18.0f + n2 * 10.0f;
                        bool sparkle = (hash2_wrap(x, y, 64, 41) > 0.965f);
                        cr = 216.0f + drift + (sparkle ? 25.0f : 0.0f);
                        cg = 228.0f + drift + (sparkle ? 20.0f : 0.0f);
                        cb = 248.0f + (sparkle ? 7.0f : 0.0f);
                        ht = 0.5f + n1 * 0.4f;
                        gloss = sparkle ? 0.90f : 0.32f;
                        bump_strength = 1.4f;
                        break;
                    }
                    case VOX_BRICK: {
                        int row = y / 12;
                        int bx = (x + (row & 1) * 16) % 32;
                        int by = y % 12;
                        bool mortar = (bx < 2 || by < 2);
                        float bvar = hash2_wrap((x + (row & 1) * 16) / 32, row, 16, 55) * 34.0f - 17.0f;
                        if (mortar) { cr = 76; cg = 70; cb = 66; ht = 0.12f; }
                        else {
                            cr = 148.0f + bvar + grain * 0.8f;
                            cg = 60.0f + bvar * 0.45f + grain * 0.5f;
                            cb = 48.0f + bvar * 0.35f + grain * 0.4f;
                            ht = 0.78f + n2 * 0.18f;
                            gloss = 0.14f;
                        }
                        bump_strength = 3.2f;
                        break;
                    }
                    case VOX_CRACKED: {
                        float d1 = fabsf((float)(x - y) + (n1 - 0.5f) * 12.0f);
                        float d2 = fabsf((float)(x + y - 63) + (n2 - 0.5f) * 10.0f);
                        float d3 = fabsf((float)(x - 32) + (n1 - 0.5f) * 8.0f);
                        float md = fminf(d1, fminf(d2, d3));
                        bool inside = (x > 5 && x < 58 && y > 5 && y < 58);
                        if (inside && md < 2.2f) {
                            cr = 255; cg = 115; cb = 28; ht = 0.02f; gloss = 0.1f;
                        } else if (inside && md < 4.4f) {
                            cr = 44; cg = 28; cb = 24; ht = 0.25f;
                        } else {
                            cr = 154.0f + grain; cg = 82.0f + grain * 0.6f; cb = 56.0f + grain * 0.4f;
                            ht = 0.82f + n2 * 0.14f;
                        }
                        bump_strength = 3.6f;
                        break;
                    }
                    case VOX_NEON_PINK:
                    case VOX_NEON_CYAN: {
                        float tube = 0.5f + 0.5f * sinf((float)y * 0.49f);
                        bool frame = (x < 3 || x > 60 || y < 3 || y > 60);
                        if (frame) { cr = 46; cg = 48; cb = 62; ht = 0.85f; gloss = 0.65f; }
                        else if (tile == VOX_NEON_PINK) {
                            cr = 255; cg = 55 + tube * 135; cb = 185 + tube * 65; ht = 0.5f + tube * 0.4f; gloss = 0.9f;
                        } else {
                            cr = 45 + tube * 145; cg = 235 + tube * 20; cb = 255; ht = 0.5f + tube * 0.4f; gloss = 0.9f;
                        }
                        break;
                    }
                    case VOX_GOLD: {
                        int dx = abs(x - 32), dy = abs(y - 32);
                        bool outer = (x < 4 || x > 59 || y < 4 || y > 59);
                        bool groove = (x == 6 || x == 57 || y == 6 || y == 57);
                        bool filigree = (abs(dx + dy - 20) <= 2 || (dx < 8 && dy < 8 && (dx + dy) > 4));
                        float sheen = sinf((float)(x + y) * 0.16f) * 20.0f;
                        if (groove) {
                            cr = 108; cg = 74; cb = 18; ht = 0.20f; gloss = 0.55f;
                        } else if (outer || filigree) {
                            cr = 252.0f + sheen * 0.6f; cg = 212.0f + sheen * 0.5f; cb = 68.0f;
                            ht = 0.96f; gloss = 0.94f;
                        } else {
                            cr = 214.0f + sheen + grain * 0.5f;
                            cg = 162.0f + sheen * 0.7f + grain * 0.4f;
                            cb = 42.0f;
                            ht = 0.66f + n2 * 0.16f; gloss = 0.85f;
                        }
                        bump_strength = 3.5f;
                        break;
                    }
                    case VOX_LEAVES: {
                        float needle = sinf((float)(x * 3 + y * 5) * 0.35f + n1 * 6.0f);
                        cr = 24.0f + n1 * 28.0f + needle * 8.0f;
                        cg = 86.0f + n1 * 56.0f + needle * 18.0f;
                        cb = 32.0f + n1 * 24.0f;
                        ht = 0.45f + n2 * 0.50f;
                        gloss = 0.18f;
                        bump_strength = 3.0f;
                        break;
                    }
                    case VOX_FIRE: {
                        bool grate = (y > 46 && (x % 8 < 2 || y > 56)) || (x < 4 || x > 59);
                        float fl = (float)(60 - y) / 56.0f + (n1 - 0.42f) * 0.55f;
                        float tongue = sinf((float)x * 0.32f + n2 * 5.0f) * 0.22f;
                        float heat = fl + tongue;
                        if (grate) {
                            cr = 48; cg = 36; cb = 34; ht = 0.92f; gloss = 0.55f;
                        } else if (heat > 0.65f) {
                            cr = 255; cg = 220 + (heat - 0.65f) * 90; cb = 95 + (heat - 0.65f) * 180;
                            ht = 0.85f; gloss = 0.3f;
                        } else if (heat > 0.28f) {
                            cr = 248; cg = 92 + (heat - 0.28f) * 280; cb = 18;
                            ht = 0.60f; gloss = 0.2f;
                        } else {
                            cr = 108; cg = 26; cb = 16; ht = 0.25f; gloss = 0.1f;
                        }
                        break;
                    }
                    case VOX_ASPHALT: {
                        float puddle = (n1 > 0.62f) ? (n1 - 0.62f) * 2.6f : 0.0f;
                        float g = 46.0f + grain * 0.9f - puddle * 14.0f;
                        cr = g; cg = g + 2.0f; cb = g + 7.0f + puddle * 16.0f;
                        ht = 0.45f + (1.0f - puddle) * n2 * 0.42f;
                        gloss = 0.16f + puddle * 0.78f;
                        bump_strength = 2.0f;
                        break;
                    }
                    case VOX_METAL: {
                        bool border = (x < 3 || y < 3 || x > 60 || y > 60);
                        bool seam = (x == 32 || y == 32);
                        bool rivet = ((x == 7 || x == 56) && (y == 7 || y == 56));
                        float brush = fbm_tile(x, y / 4, 6, 73) * 18.0f - 9.0f;
                        float g = border ? 72.0f : (seam ? 58.0f : (rivet ? 215.0f : 134.0f + brush));
                        cr = g; cg = g + 6.0f; cb = g + 14.0f;
                        ht = seam ? 0.18f : (rivet ? 0.98f : (border ? 0.85f : 0.66f + brush * 0.008f));
                        gloss = 0.68f;
                        break;
                    }
                    case VOX_PORTAL_FRAME: {
                        bool rune = (x >= 24 && x <= 39 && ((y / 6) % 2 == 0) && (x == 25 || x == 38 || y % 12 < 3));
                        cr = rune ? 150.0f : 72.0f + grain * 0.6f;
                        cg = rune ? 175.0f : 68.0f + grain * 0.6f;
                        cb = rune ? 255.0f : 96.0f + grain * 0.7f;
                        ht = rune ? 0.30f : 0.78f + n2 * 0.15f;
                        gloss = rune ? 0.85f : 0.25f;
                        break;
                    }
                    case VOX_CARPET: {
                        int dx = abs((x % 16) - 8);
                        int dy = abs((y % 16) - 8);
                        bool gold_trim = (dx + dy == 7 || dx + dy == 8);
                        bool weave = ((x + y) & 1) == 0;
                        if (gold_trim) { cr = 212; cg = 166; cb = 52; ht = 0.82f; gloss = 0.30f; }
                        else {
                            cr = (weave ? 156.0f : 136.0f) + grain * 0.4f;
                            cg = 26.0f + n2 * 10.0f;
                            cb = 34.0f + n2 * 10.0f;
                            ht = weave ? 0.60f : 0.50f;
                            gloss = 0.06f;
                        }
                        break;
                    }
                    case VOX_CINEMA_STATIC: {
                        float st = hash2_wrap(x, y, 64, 199) * 195.0f + 40.0f;
                        cr = st; cg = st; cb = st + 14.0f; ht = 0.5f; gloss = 0.5f;
                        break;
                    }
                    case VOX_CINEMA_LIT: {
                        bool frame = (x < 4 || x > 59 || y < 4 || y > 59);
                        float rays = 0.5f + 0.5f * sinf(atan2f((float)(y - 32), (float)(x - 32)) * 8.0f);
                        cr = frame ? 145 : 255; cg = frame ? 105 : 210 + rays * 40; cb = frame ? 45 : 105 + rays * 65;
                        ht = frame ? 0.9f : 0.5f; gloss = 0.7f;
                        break;
                    }
                    case VOX_GLASS: {
                        bool mullion = (x < 3 || x > 60 || y < 3 || y > 60 || abs(x - 32) < 2 || abs(y - 32) < 2);
                        float glare = (abs((x + y) - 52) < 5 || abs((x + y) - 84) < 3) ? 68.0f : 0.0f;
                        if (mullion) { cr = 68; cg = 78; cb = 92; ht = 0.95f; gloss = 0.55f; }
                        else { cr = 135 + glare; cg = 205 + glare; cb = 245 + glare * 0.4f; ht = 0.45f; gloss = 0.96f; }
                        break;
                    }
                    case VOX_MIRROR: {
                        bool frame = (x < 4 || x > 59 || y < 4 || y > 59);
                        float sheen = sinf((float)(x * 2 + y) * 0.09f) * 32.0f;
                        if (frame) { cr = 165; cg = 142; cb = 86; ht = 0.95f; gloss = 0.78f; }
                        else { cr = 185 + sheen; cg = 232 + sheen; cb = 252; ht = 0.5f; gloss = 0.98f; }
                        break;
                    }
                    case VOX_VENT: {
                        int slat = y % 8;
                        bool frame = (x < 4 || x > 59);
                        if (frame) { cr = 112; cg = 118; cb = 128; ht = 0.92f; }
                        else {
                            float g = (slat < 2) ? 32.0f : 86.0f + (float)slat * 11.0f;
                            cr = g; cg = g + 5.0f; cb = g + 10.0f;
                            ht = (slat < 2) ? 0.12f : 0.40f + (float)slat * 0.09f;
                        }
                        gloss = 0.58f;
                        bump_strength = 3.5f;
                        break;
                    }
                    case VOX_HAZARD: {
                        bool stripe = (((x + y) / 10) & 1) == 0;
                        float wear = (n2 - 0.5f) * 24.0f;
                        cr = (stripe ? 236.0f : 36.0f) + wear;
                        cg = (stripe ? 188.0f : 36.0f) + wear;
                        cb = (stripe ? 26.0f : 42.0f) + wear * 0.5f;
                        ht = 0.65f + n2 * 0.2f;
                        gloss = 0.35f;
                        break;
                    }
                    case VOX_CONCRETE: {
                        bool seam = (x == 0 || y == 0 || x == 32 || y == 32);
                        float g = seam ? 78.0f : (124.0f + grain + (n1 - 0.5f) * 20.0f);
                        cr = g; cg = g + 2.0f; cb = g + 4.0f;
                        ht = seam ? 0.25f : 0.68f + n2 * 0.24f;
                        gloss = 0.14f;
                        break;
                    }
                    case VOX_ROAD_STRIPE: {
                        bool dash = (x >= 27 && x <= 36 && y >= 8 && y <= 54);
                        if (dash) { cr = 236 + grain * 0.4f; cg = 192 + grain * 0.4f; cb = 44; ht = 0.72f; gloss = 0.22f; }
                        else { cr = 48 + grain; cg = 50 + grain; cb = 56 + grain; ht = 0.55f + n2 * 0.3f; gloss = 0.38f; }
                        break;
                    }
                    case VOX_DOOR_BLUE:
                    case VOX_DOOR_YELLOW:
                    case VOX_DOOR_RED:
                    case VOX_DOOR_LOCKED: {
                        bool frame = (x < 5 || x > 58 || y < 5 || y > 58);
                        bool ribs = (y % 12 < 2);
                        bool panel = (x >= 20 && x <= 43 && y >= 20 && y <= 43);
                        float b = frame ? 62.0f : (panel ? 245.0f : (ribs ? 88.0f : 148.0f + grain * 0.4f));
                        if (tile == VOX_DOOR_BLUE)        { cr = b * 0.28f; cg = b * 0.65f; cb = b; }
                        else if (tile == VOX_DOOR_YELLOW) { cr = b; cg = b * 0.88f; cb = b * 0.18f; }
                        else if (tile == VOX_DOOR_RED)    { cr = b; cg = b * 0.26f; cb = b * 0.26f; }
                        else                              { cr = b; cg = b * 0.72f; cb = b * 0.24f; }
                        ht = frame ? 0.92f : (panel ? 0.88f : (ribs ? 0.25f : 0.65f));
                        gloss = 0.62f;
                        break;
                    }
                    case VOX_SEAT: {
                        bool tuft = (((x % 16) - 8) * ((x % 16) - 8) + ((y % 16) - 8) * ((y % 16) - 8) < 6);
                        cr = tuft ? 115 : 186 + grain * 0.5f; cg = tuft ? 22 : 34; cb = tuft ? 30 : 46;
                        ht = tuft ? 0.25f : 0.76f; gloss = 0.10f;
                        break;
                    }
                    case VOX_GIFT: {
                        bool ribbon = (x >= 26 && x <= 37) || (y >= 26 && y <= 37);
                        cr = ribbon ? 252 : 196 + grain * 0.3f; cg = ribbon ? 215 : 32; cb = ribbon ? 54 : 44;
                        ht = ribbon ? 0.90f : 0.65f; gloss = ribbon ? 0.75f : 0.25f;
                        break;
                    }
                    case VOX_RELAY_CORE: {
                        float coil = 0.5f + 0.5f * sinf((float)y * 0.55f);
                        bool cage = (x < 6 || x > 57 || abs(x - 32) < 3);
                        if (cage) { cr = 68; cg = 72; cb = 82; ht = 0.95f; gloss = 0.7f; }
                        else { cr = 255; cg = 40 + coil * 110; cb = 75 + coil * 80; ht = 0.4f + coil * 0.4f; gloss = 0.85f; }
                        break;
                    }
                    case VOX_SANDSTONE: {
                        int row = y / 16;
                        int bx = (x + (row & 1) * 20) % 32;
                        int by = y % 16;
                        bool seam = (bx < 2 || by < 2);
                        bool frieze = (y >= 26 && y <= 37 && ((x / 4) % 2 == 0) && (y == 27 || y == 36 || x % 8 == 2));
                        float bvar = hash2_wrap((x + (row & 1) * 20) / 32, row, 16, 129) * 24.0f - 12.0f;
                        if (seam) { cr = 124; cg = 96; cb = 62; ht = 0.14f; }
                        else if (frieze) { cr = 162; cg = 128; cb = 78; ht = 0.42f; }
                        else {
                            cr = 214.0f + bvar + grain * 0.7f;
                            cg = 180.0f + bvar * 0.8f + grain * 0.6f;
                            cb = 126.0f + bvar * 0.6f + grain * 0.4f;
                            ht = 0.78f + n2 * 0.16f;
                        }
                        gloss = 0.12f;
                        bump_strength = 3.0f;
                        break;
                    }
                    case VOX_WATER: {
                        float w1 = sinf((float)x * 0.25f + (float)y * 0.18f + n1 * 4.0f);
                        float w2 = cosf((float)x * 0.18f - (float)y * 0.28f + n2 * 4.0f);
                        float c = 0.5f + 0.25f * (w1 + w2);
                        cr = 28.0f + c * 92.0f;
                        cg = 152.0f + c * 88.0f;
                        cb = 215.0f + c * 40.0f;
                        ht = c;
                        gloss = 0.94f;
                        bump_strength = 2.4f;
                        break;
                    }
                    case VOX_LAVA: {
                        float v = n1 * 0.7f + n2 * 0.3f;
                        if (v < 0.42f) {
                            /* Dark cooled volcanic crust islands */
                            cr = 62 + v * 120; cg = 22 + v * 45; cb = 16; ht = 0.85f; gloss = 0.25f;
                        } else {
                            float hot = (v - 0.42f) / 0.58f;
                            cr = 255; cg = 75 + hot * 175; cb = 12 + hot * hot * 130;
                            ht = 0.35f - hot * 0.2f; gloss = 0.6f;
                        }
                        break;
                    }
                    case VOX_SLIME: {
                        float d1 = hypotf((float)(x - 18), (float)(y - 22));
                        float d2 = hypotf((float)(x - 46), (float)(y - 44));
                        bool bub = (d1 < 8.0f || d2 < 6.0f);
                        float glow = n1 * 0.6f + n2 * 0.4f;
                        cr = bub ? 155 : 26 + glow * 45;
                        cg = bub ? 255 : 165 + glow * 85;
                        cb = bub ? 85 : 24 + glow * 32;
                        ht = bub ? 0.92f : 0.45f + glow * 0.3f;
                        gloss = 0.88f;
                        break;
                    }
                    case VOX_RUST_BRASS: {
                        bool seam = (x < 2 || y < 2 || x == 32 || y == 32);
                        bool rivet = ((x == 6 || x == 26 || x == 38 || x == 58) &&
                                      (y == 6 || y == 26 || y == 38 || y == 58));
                        float patina = (n1 > 0.66f) ? (n1 - 0.66f) * 2.5f : 0.0f;
                        if (seam) { cr = 48; cg = 34; cb = 22; ht = 0.14f; }
                        else if (rivet) { cr = 222; cg = 180; cb = 108; ht = 0.98f; gloss = 0.82f; }
                        else {
                            cr = 138.0f + grain * 0.8f - patina * 65.0f;
                            cg = 94.0f + grain * 0.5f + patina * 42.0f;
                            cb = 52.0f + grain * 0.3f + patina * 48.0f;
                            ht = 0.72f + n2 * 0.22f;
                            gloss = 0.58f - patina * 0.35f;
                        }
                        bump_strength = 3.4f;
                        break;
                    }
                    case VOX_CRATE: {
                        bool frame = (x < 7 || x > 56 || y < 7 || y > 56);
                        bool brace = (abs(x - y) < 5);
                        bool plank = (!frame && !brace && (y % 12 == 0));
                        bool bracket = ((x < 9 || x > 54) && (y < 9 || y > 54));
                        if (bracket) { cr = 74; cg = 78; cb = 84; ht = 0.96f; gloss = 0.60f; }
                        else if (plank) { cr = 68; cg = 46; cb = 24; ht = 0.18f; }
                        else {
                            float lift = (frame || brace) ? 20.0f : 0.0f;
                            cr = 152.0f + lift + grain * 0.7f;
                            cg = 110.0f + lift * 0.8f + grain * 0.5f;
                            cb = 62.0f + lift * 0.5f + grain * 0.3f;
                            ht = (frame || brace) ? 0.88f : 0.56f + n2 * 0.15f;
                            gloss = 0.18f;
                        }
                        bump_strength = 3.5f;
                        break;
                    }
                    case VOX_RED_CARNIVAL: {
                        bool red = ((x / 16) & 1) == 0;
                        float fold = sinf((float)x * 0.39f) * 18.0f;
                        cr = (red ? 188.0f : 226.0f) + fold + grain * 0.4f;
                        cg = (red ? 28.0f : 206.0f) + fold * 0.6f;
                        cb = (red ? 34.0f : 172.0f) + fold * 0.5f;
                        ht = 0.55f + sinf((float)x * 0.39f) * 0.32f;
                        gloss = 0.14f;
                        break;
                    }
                    case VOX_CRYPT_STONE: {
                        int row = y / 16;
                        int bx = (x + (row & 1) * 14) % 32;
                        int by = y % 16;
                        bool seam = (bx < 2 || by < 2);
                        float moss = (n1 > 0.60f && y > 26) ? (n1 - 0.60f) * 2.2f : 0.0f;
                        if (seam) { cr = 26; cg = 28; cb = 36; ht = 0.12f; }
                        else {
                            cr = 74.0f + grain * 0.7f - moss * 32.0f;
                            cg = 72.0f + grain * 0.7f + moss * 52.0f;
                            cb = 88.0f + grain * 0.7f - moss * 24.0f;
                            ht = 0.76f + n2 * 0.22f;
                            gloss = 0.18f;
                        }
                        bump_strength = 3.4f;
                        break;
                    }
                    case VOX_TECH_WALL: {
                        bool rib = (x % 16 < 2 || y < 3 || y > 60 || abs(y - 32) < 2);
                        bool led = (y >= 24 && y <= 28 && (x >= 7 && x <= 12));
                        bool vent = (y >= 38 && y <= 54 && x >= 22 && x <= 42 && (y % 4 == 0));
                        if (led) { cr = 48; cg = 245; cb = 105; ht = 0.85f; gloss = 0.90f; }
                        else if (vent) { cr = 34; cg = 38; cb = 44; ht = 0.15f; gloss = 0.3f; }
                        else if (rib) { cr = 56; cg = 62; cb = 70; ht = 0.22f; gloss = 0.55f; }
                        else {
                            float g = 112.0f + grain * 0.7f;
                            cr = g; cg = g + 5.0f; cb = g + 12.0f;
                            ht = 0.74f + n2 * 0.15f; gloss = 0.60f;
                        }
                        bump_strength = 3.4f;
                        break;
                    }
                    case VOX_PUSH_BLOCK: {
                        bool rim = (x < 7 || x > 56 || y < 7 || y > 56);
                        float d = hypotf((float)(x - 32), (float)(y - 32));
                        float ray = sinf(atan2f((float)(y - 32), (float)(x - 32)) * 8.0f);
                        bool sun = (d < 13.0f || (d < 21.0f && ray > 0.35f));
                        if (sun) { cr = 248; cg = 205; cb = 62; ht = 0.96f; gloss = 0.85f; }
                        else if (rim) { cr = 148; cg = 112; cb = 52; ht = 0.88f; gloss = 0.32f; }
                        else { cr = 186 + grain * 0.6f; cg = 146 + grain * 0.5f; cb = 82; ht = 0.60f + n2 * 0.15f; }
                        bump_strength = 3.6f;
                        break;
                    }
                    case VOX_SPIKES: {
                        int sx = x % 16;
                        float dist_c = fabsf((float)sx - 7.5f);
                        bool blade = (y < 48 && dist_c < ((float)y / 48.0f) * 6.0f + 0.8f);
                        if (blade) {
                            bool blood = (y < 14);
                            cr = blood ? 205 : 182 - dist_c * 14;
                            cg = blood ? 26  : 188 - dist_c * 14;
                            cb = blood ? 28  : 198 - dist_c * 12;
                            ht = 0.92f - dist_c * 0.08f; gloss = 0.85f;
                        } else {
                            cr = 38; cg = 40; cb = 46; ht = 0.15f;
                        }
                        break;
                    }
                    case VOX_SLIPGATE: {
                        float dx = (float)(x - 32), dy = (float)(y - 32);
                        float d = hypotf(dx, dy);
                        float ang = atan2f(dy, dx);
                        float sw = 0.5f + 0.5f * sinf(d * 0.45f - ang * 3.0f);
                        bool frame = (x < 5 || x > 58 || y < 5 || y > 58);
                        if (frame) { cr = 128; cg = 88; cb = 52; ht = 0.95f; gloss = 0.65f; }
                        else {
                            cr = 112 + sw * 135; cg = 42 + sw * 92; cb = 205 + sw * 50;
                            ht = 0.4f + sw * 0.4f; gloss = 0.90f;
                        }
                        break;
                    }
                    default: break;
                }

                hmap[y][x] = ht;
                gmap[y][x] = gloss;
                int idx = ((ty + y) * W + (tx + x)) * 4;
                diff[idx + 0] = u8_clamp(cr);
                diff[idx + 1] = u8_clamp(cg);
                diff[idx + 2] = u8_clamp(cb);
                diff[idx + 3] = 255;
            }
        }

        /* Compute seamless tangent-space normal + gloss map via Sobel filter */
        for (int y = 0; y < T; ++y) {
            int ym = (y + T - 1) % T;
            int yp = (y + 1) % T;
            for (int x = 0; x < T; ++x) {
                int xm = (x + T - 1) % T;
                int xp = (x + 1) % T;
                float gx = (hmap[ym][xp] + 2.0f * hmap[y][xp] + hmap[yp][xp]) -
                           (hmap[ym][xm] + 2.0f * hmap[y][xm] + hmap[yp][xm]);
                float gy = (hmap[yp][xm] + 2.0f * hmap[yp][x] + hmap[yp][xp]) -
                           (hmap[ym][xm] + 2.0f * hmap[ym][x] + hmap[ym][xp]);
                float nx = -gx * bump_strength;
                float ny = -gy * bump_strength;
                float nz = 1.0f;
                float inv = 1.0f / sqrtf(nx * nx + ny * ny + nz * nz);
                int idx = ((ty + y) * W + (tx + x)) * 4;
                norm[idx + 0] = u8_clamp((nx * inv * 0.5f + 0.5f) * 255.0f);
                norm[idx + 1] = u8_clamp((ny * inv * 0.5f + 0.5f) * 255.0f);
                norm[idx + 2] = u8_clamp((nz * inv * 0.5f + 0.5f) * 255.0f);
                norm[idx + 3] = u8_clamp(gmap[y][x] * 255.0f);
            }
        }
    }

    free(hmap);
    free(gmap);
    *out_diffuse = diff;
    *out_normal = norm;
    *out_w = W;
    *out_h = H;
}

/* ============================================================================
 * 1280x128 (20x2 tiles of 64x64) HIGH-DETAIL SHADED & OUTLINED SPRITE ATLAS
 * ============================================================================ */
typedef struct {
    uint8_t px[RETRO_SPRITE_PX][RETRO_SPRITE_PX][4];
} SprCanvas;

static void spr_put(SprCanvas *c, int x, int y, float r, float g, float b, float a) {
    if (x < 0 || x >= RETRO_SPRITE_PX || y < 0 || y >= RETRO_SPRITE_PX) return;
    c->px[y][x][0] = u8_clamp(r);
    c->px[y][x][1] = u8_clamp(g);
    c->px[y][x][2] = u8_clamp(b);
    c->px[y][x][3] = u8_clamp(a);
}

static void spr_ellipse(SprCanvas *c, float cx, float cy, float rx, float ry,
                        float r, float g, float b, float shade) {
    int x0 = (int)floorf(cx - rx - 1.0f), x1 = (int)ceilf(cx + rx + 1.0f);
    int y0 = (int)floorf(cy - ry - 1.0f), y1 = (int)ceilf(cy + ry + 1.0f);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = ((float)x - cx) / rx;
            float dy = ((float)y - cy) / ry;
            float d2 = dx * dx + dy * dy;
            if (d2 <= 1.0f) {
                float nz = sqrtf(1.0f - d2);
                /* Classic 90s hand-painted sprite lighting + subtle gritty micro-texture */
                float lit = (-dx * 0.48f - dy * 0.54f + nz * 0.72f);
                float grit = (hash2_wrap(x, y, 64, 911) - 0.5f) * 20.0f;
                float mul = (1.0f - shade * 0.58f) + lit * shade * 1.08f;
                float spec = (lit > 0.86f) ? (lit - 0.86f) * 165.0f : 0.0f;
                spr_put(c, x, y, r * mul + spec + grit, g * mul + spec + grit * 0.85f, b * mul + spec + grit * 0.85f, 255.0f);
            }
        }
    }
}

static void spr_capsule(SprCanvas *c, float x0, float y0, float x1, float y1, float rad,
                        float r, float g, float b) {
    int bx0 = (int)floorf(fminf(x0, x1) - rad - 1.0f);
    int bx1 = (int)ceilf(fmaxf(x0, x1) + rad + 1.0f);
    int by0 = (int)floorf(fminf(y0, y1) - rad - 1.0f);
    int by1 = (int)ceilf(fmaxf(y0, y1) + rad + 1.0f);
    float vx = x1 - x0, vy = y1 - y0;
    float len2 = vx * vx + vy * vy;
    for (int y = by0; y <= by1; ++y) {
        for (int x = bx0; x <= bx1; ++x) {
            float t = (len2 > 1e-4f) ? (((float)x - x0) * vx + ((float)y - y0) * vy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float px = x0 + vx * t;
            float py = y0 + vy * t;
            float d = hypotf((float)x - px, (float)y - py);
            if (d <= rad) {
                float nz = sqrtf(fmaxf(0.0f, 1.0f - (d / rad) * (d / rad)));
                float dx = ((float)x - px) / (rad + 0.01f);
                float dy = ((float)y - py) / (rad + 0.01f);
                float grit = (hash2_wrap(x, y, 64, 577) - 0.5f) * 18.0f;
                float lit = 0.58f + (-dx * 0.38f - dy * 0.36f + nz * 0.62f) * 0.60f;
                spr_put(c, x, y, r * lit + grit, g * lit + grit * 0.85f, b * lit + grit * 0.85f, 255.0f);
            }
        }
    }
}

static void spr_box(SprCanvas *c, int x0, int y0, int x1, int y1, float r, float g, float b) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float mul = 1.0f;
            if (y == y0 || x == x0) mul = 1.25f;
            else if (y == y1 || x == x1) mul = 0.62f;
            spr_put(c, x, y, r * mul, g * mul, b * mul, 255.0f);
        }
    }
}

static void spr_outline(SprCanvas *c) {
    SprCanvas src = *c;
    for (int y = 1; y < RETRO_SPRITE_PX - 1; ++y) {
        for (int x = 1; x < RETRO_SPRITE_PX - 1; ++x) {
            if (src.px[y][x][3] > 20) continue;
            if (src.px[y - 1][x][3] > 180 || src.px[y + 1][x][3] > 180 ||
                src.px[y][x - 1][3] > 180 || src.px[y][x + 1][3] > 180) {
                spr_put(c, x, y, 16, 12, 20, 255);
            }
        }
    }
}

static void paint_sprite_tile(SprCanvas *c, int col, int frame) {
    memset(c, 0, sizeof(*c));
    float f = (frame == 0) ? -1.0f : 1.0f;
    float bob = (frame == 0) ? 0.0f : 2.0f;

    switch (col) {
        case SPR_ALIEN_TROOPER: /* Duke 3D Armored Lizard Trooper w/ Blaster */
            spr_capsule(c, 25, 42, 22 - f * 3, 59, 4.5f, 65, 138, 72);
            spr_capsule(c, 39, 42, 42 + f * 3, 59, 4.5f, 65, 138, 72);
            spr_box(c, 19 - (int)(f * 3), 56, 26 - (int)(f * 3), 61, 46, 50, 58);
            spr_box(c, 38 + (int)(f * 3), 56, 45 + (int)(f * 3), 61, 46, 50, 58);
            spr_ellipse(c, 32, 31 + bob, 11, 13, 172, 62, 58, 0.7f); /* Armor cuirass */
            spr_ellipse(c, 32, 29 + bob, 8, 8, 92, 178, 88, 0.6f);
            spr_capsule(c, 20, 24 + bob, 13, 35 + bob, 4.0f, 78, 165, 84);
            spr_capsule(c, 44, 24 + bob, 51, 33 + bob, 4.0f, 78, 165, 84);
            spr_capsule(c, 44, 33 + bob, 58, 30 + bob, 2.8f, 120, 128, 142); /* Blaster rifle */
            spr_ellipse(c, 32, 13 + bob, 7.5f, 8.5f, 88, 192, 92, 0.7f); /* Lizard head */
            spr_box(c, 26, 11 + (int)bob, 38, 14 + (int)bob, 255, 42, 42); /* Red glowing visor */
            spr_outline(c);
            break;

        case SPR_PIG_GUARD: /* Bulky Armored Boar-Cop Enforcer */
            spr_capsule(c, 24, 43, 21 - f * 3, 59, 5.5f, 52, 68, 128);
            spr_capsule(c, 40, 43, 43 + f * 3, 59, 5.5f, 52, 68, 128);
            spr_ellipse(c, 32, 32 + bob, 14, 14, 48, 72, 148, 0.75f); /* Kevlar police vest */
            spr_box(c, 27, 25 + (int)bob, 32, 29 + (int)bob, 248, 208, 62); /* Gold badge */
            spr_capsule(c, 18, 25 + bob, 11, 37 + bob, 4.8f, 218, 142, 142);
            spr_capsule(c, 46, 25 + bob, 53, 36 + bob, 4.8f, 218, 142, 142);
            spr_capsule(c, 42, 35 + bob, 60, 34 + bob, 3.2f, 88, 92, 102); /* Riot shotgun */
            spr_ellipse(c, 32, 14 + bob, 9.5f, 9.0f, 224, 146, 148, 0.7f); /* Boar head */
            spr_ellipse(c, 32, 16 + bob, 5.0f, 3.5f, 238, 158, 162, 0.5f); /* Snout */
            spr_ellipse(c, 27, 11 + bob, 2.0f, 2.0f, 255, 240, 60, 0.2f);
            spr_ellipse(c, 37, 11 + bob, 2.0f, 2.0f, 255, 240, 60, 0.2f);
            spr_outline(c);
            break;

        case SPR_OCTABRAIN: /* Floating Tentacled Brain Beast */
            for (int t = 0; t < 5; ++t) {
                float sx = 18.0f + (float)t * 7.0f;
                float sw = sinf((float)t * 1.4f + f * 1.5f) * 5.0f;
                spr_capsule(c, sx, 34 + bob, sx + sw, 56 + bob, 2.6f, 168, 92, 178);
            }
            spr_ellipse(c, 32, 23 + bob, 18, 15, 198, 118, 208, 0.8f); /* Pulsing brain dome */
            spr_ellipse(c, 32, 16 + bob, 14, 9, 224, 146, 232, 0.7f);
            spr_ellipse(c, 24, 24 + bob, 3.2f, 3.2f, 65, 255, 95, 0.2f);
            spr_ellipse(c, 32, 21 + bob, 3.8f, 3.8f, 65, 255, 95, 0.2f);
            spr_ellipse(c, 40, 24 + bob, 3.2f, 3.2f, 65, 255, 95, 0.2f);
            spr_ellipse(c, 32, 32 + bob, 6.0f, 3.5f, 42, 16, 36, 0.3f); /* Maw */
            spr_outline(c);
            break;

        case SPR_TURRET: /* Steel Tripod Laser Turret */
            spr_capsule(c, 32, 34, 14, 59, 3.5f, 96, 102, 116);
            spr_capsule(c, 32, 34, 50, 59, 3.5f, 96, 102, 116);
            spr_capsule(c, 32, 34, 32, 60, 3.5f, 76, 82, 94);
            spr_ellipse(c, 32, 24, 13, 11, 136, 144, 162, 0.8f);
            spr_box(c, 25, 19, 39, 28, 62, 68, 82);
            spr_ellipse(c, 32 + f * 2.0f, 23, 4.5f, 4.5f, 255, 38, 42, 0.2f); /* Glowing red eye */
            spr_outline(c);
            break;

        case SPR_WOLF: /* Tomb Raider Snarling Cave Wolf */
            spr_capsule(c, 20, 44, 16 - f * 4, 59, 3.5f, 118, 104, 90);
            spr_capsule(c, 44, 44, 48 + f * 4, 59, 3.5f, 118, 104, 90);
            spr_capsule(c, 27, 45, 29 + f * 3, 59, 3.2f, 96, 84, 72);
            spr_ellipse(c, 33, 38 + bob, 17, 10, 136, 120, 104, 0.75f); /* Fur torso */
            spr_capsule(c, 48, 34 + bob, 58, 28 + bob, 3.2f, 114, 98, 84); /* Tail */
            spr_ellipse(c, 20, 29 + bob, 8.5f, 7.5f, 142, 126, 110, 0.7f); /* Wolf head */
            spr_capsule(c, 16, 22 + bob, 14, 15 + bob, 2.4f, 116, 102, 88); /* Ear */
            spr_capsule(c, 24, 22 + bob, 26, 15 + bob, 2.4f, 116, 102, 88);
            spr_ellipse(c, 17, 27 + bob, 2.0f, 2.0f, 255, 205, 45, 0.2f); /* Amber eye */
            spr_ellipse(c, 22, 27 + bob, 2.0f, 2.0f, 255, 205, 45, 0.2f);
            spr_outline(c);
            break;

        case SPR_BAT: /* Swooping Cavern Bat (flapping wings!) */
            spr_capsule(c, 28, 30, 6,  20 + f * 12, 4.5f, 88, 52, 112);
            spr_capsule(c, 36, 30, 58, 20 + f * 12, 4.5f, 88, 52, 112);
            spr_capsule(c, 18, 26 + f * 6, 10, 36 + f * 6, 3.5f, 68, 38, 92);
            spr_capsule(c, 46, 26 + f * 6, 54, 36 + f * 6, 3.5f, 68, 38, 92);
            spr_ellipse(c, 32, 30, 8, 10, 106, 68, 128, 0.75f);
            spr_ellipse(c, 29, 27, 2.0f, 2.0f, 255, 55, 55, 0.2f);
            spr_ellipse(c, 35, 27, 2.0f, 2.0f, 255, 55, 55, 0.2f);
            spr_outline(c);
            break;

        case SPR_STONE_GUARD: /* Golden Atlantean Temple Warrior */
            spr_capsule(c, 25, 43, 23 - f * 3, 60, 4.5f, 168, 134, 72);
            spr_capsule(c, 39, 43, 41 + f * 3, 60, 4.5f, 168, 134, 72);
            spr_ellipse(c, 32, 30 + bob, 12, 14, 198, 158, 82, 0.8f);
            spr_ellipse(c, 32, 29 + bob, 4.5f, 4.5f, 48, 238, 255, 0.2f); /* Turquoise sun core */
            spr_capsule(c, 18, 24 + bob, 12, 38 + bob, 4.0f, 182, 146, 76);
            spr_capsule(c, 46, 24 + bob, 52, 36 + bob, 4.0f, 182, 146, 76);
            spr_capsule(c, 53, 12 + bob, 53, 52 + bob, 2.0f, 238, 202, 86); /* Golden spear */
            spr_ellipse(c, 32, 13 + bob, 7.5f, 8.5f, 214, 174, 88, 0.75f);
            spr_box(c, 27, 11 + (int)bob, 37, 14 + (int)bob, 58, 245, 255);
            spr_outline(c);
            break;

        case SPR_SYNDICATE: /* Counter-Strike Balaclava Tactical Commando */
            spr_capsule(c, 26, 42, 23 - f * 3, 60, 4.5f, 78, 84, 74);
            spr_capsule(c, 38, 42, 41 + f * 3, 60, 4.5f, 78, 84, 74);
            spr_ellipse(c, 32, 31 + bob, 11, 13, 62, 74, 64, 0.7f); /* Tactical rig */
            spr_box(c, 24, 24 + (int)bob, 40, 36 + (int)bob, 44, 52, 48); /* Armor pouches */
            spr_capsule(c, 20, 24 + bob, 16, 35 + bob, 3.8f, 82, 88, 76);
            spr_capsule(c, 44, 24 + bob, 48, 34 + bob, 3.8f, 82, 88, 76);
            spr_capsule(c, 32, 33 + bob, 57, 31 + bob, 2.4f, 48, 52, 58); /* Assault rifle */
            spr_ellipse(c, 32, 13 + bob, 7.2f, 8.0f, 40, 44, 48, 0.65f); /* Balaclava */
            spr_box(c, 27, 11 + (int)bob, 37, 14 + (int)bob, 68, 235, 165); /* NVG Visor */
            spr_outline(c);
            break;

        case SPR_OGRE_KNIGHT: /* Quake Horned Iron Ogre / Death-Knight */
            spr_capsule(c, 24, 43, 20 - f * 3, 60, 5.5f, 116, 86, 62);
            spr_capsule(c, 40, 43, 44 + f * 3, 60, 5.5f, 116, 86, 62);
            spr_ellipse(c, 32, 31 + bob, 15, 14, 148, 108, 74, 0.8f); /* Broad bronze torso */
            spr_ellipse(c, 17, 23 + bob, 6.5f, 5.5f, 96, 86, 78, 0.75f); /* Pauldrons */
            spr_ellipse(c, 47, 23 + bob, 6.5f, 5.5f, 96, 86, 78, 0.75f);
            spr_capsule(c, 47, 26 + bob, 58, 18 + bob, 3.5f, 168, 174, 186); /* Chainsaw/Blade */
            spr_ellipse(c, 32, 13 + bob, 8.0f, 8.5f, 92, 84, 78, 0.8f); /* Iron helm */
            spr_capsule(c, 24, 10 + bob, 15, 4 + bob, 2.6f, 215, 205, 182); /* Horns */
            spr_capsule(c, 40, 10 + bob, 49, 4 + bob, 2.6f, 215, 205, 182);
            spr_box(c, 27, 12 + (int)bob, 37, 15 + (int)bob, 255, 85, 28);
            spr_outline(c);
            break;

        case SPR_FIEND_GHOUL: /* Hunched Clawed Quake Fiend / Blood Ghoul */
            spr_capsule(c, 24, 43, 20 - f * 4, 60, 4.8f, 138, 76, 62);
            spr_capsule(c, 40, 43, 44 + f * 4, 60, 4.8f, 138, 76, 62);
            spr_ellipse(c, 32, 32 + bob, 13, 12, 164, 92, 74, 0.8f);
            spr_capsule(c, 19, 27 + bob, 8,  41 + bob + f * 3, 4.2f, 154, 86, 68);
            spr_capsule(c, 45, 27 + bob, 56, 41 + bob - f * 3, 4.2f, 154, 86, 68);
            spr_capsule(c, 8,  42 + bob, 5,  49 + bob, 2.2f, 232, 224, 210); /* Claws */
            spr_capsule(c, 56, 42 + bob, 59, 49 + bob, 2.2f, 232, 224, 210);
            spr_ellipse(c, 32, 17 + bob, 8.5f, 8.0f, 172, 98, 78, 0.75f);
            spr_ellipse(c, 28, 15 + bob, 2.2f, 2.2f, 255, 230, 55, 0.2f);
            spr_ellipse(c, 36, 15 + bob, 2.2f, 2.2f, 255, 230, 55, 0.2f);
            spr_outline(c);
            break;

        case SPR_CULTIST: /* Blood Hooded Crimson Cultist w/ Lit TNT! */
            spr_ellipse(c, 32, 42 + bob, 13, 19, 148, 26, 32, 0.8f); /* Flowing crimson robe */
            spr_capsule(c, 20, 26 + bob, 12, 38 + bob, 4.2f, 162, 30, 36);
            spr_capsule(c, 44, 26 + bob, 52, 34 + bob, 4.2f, 162, 30, 36);
            spr_capsule(c, 52, 28 + bob, 56, 38 + bob, 2.8f, 235, 52, 38); /* Red TNT stick */
            spr_ellipse(c, 55, 25 + bob, 3.0f, 3.0f, 255, 225, 65, 0.1f); /* Burning spark! */
            spr_ellipse(c, 32, 14 + bob, 10, 10, 166, 30, 38, 0.8f); /* Pointed hood */
            spr_ellipse(c, 32, 15 + bob, 6.2f, 6.2f, 18, 12, 18, 0.2f); /* Dark cowl shadow */
            spr_ellipse(c, 29, 14 + bob, 2.0f, 1.8f, 255, 238, 42, 0.1f); /* Blazing eyes */
            spr_ellipse(c, 35, 14 + bob, 2.0f, 1.8f, 255, 238, 42, 0.1f);
            spr_outline(c);
            break;

        case SPR_IMP: /* Doom Spiky Terracotta Imp w/ Glowing Fireball! */
            spr_capsule(c, 25, 42, 21 - f * 4, 60, 4.6f, 152, 86, 56);
            spr_capsule(c, 39, 42, 43 + f * 4, 60, 4.6f, 152, 86, 56);
            spr_ellipse(c, 32, 30 + bob, 11, 13, 168, 96, 62, 0.8f);
            /* Dorsal bone spikes */
            spr_capsule(c, 22, 22 + bob, 17, 17 + bob, 2.0f, 228, 218, 195);
            spr_capsule(c, 42, 22 + bob, 47, 17 + bob, 2.0f, 228, 218, 195);
            spr_capsule(c, 20, 26 + bob, 11, 35 + bob, 4.0f, 162, 92, 58);
            spr_capsule(c, 44, 26 + bob, 53, 30 + bob, 4.0f, 162, 92, 58);
            spr_ellipse(c, 54, 27 + bob, 5.5f + f, 5.5f + f, 255, 142, 32, 0.2f); /* Glowing fireball! */
            spr_ellipse(c, 32, 13 + bob, 7.8f, 8.2f, 174, 100, 66, 0.75f);
            spr_ellipse(c, 29, 12 + bob, 2.0f, 1.8f, 255, 235, 55, 0.1f);
            spr_ellipse(c, 35, 12 + bob, 2.0f, 1.8f, 255, 235, 55, 0.1f);
            spr_outline(c);
            break;

        case SPR_PINKY: /* Doom Muscular Horned Pinky Bull-Demon */
            spr_capsule(c, 22, 44, 18 - f * 4, 60, 6.2f, 212, 96, 132);
            spr_capsule(c, 42, 44, 46 + f * 4, 60, 6.2f, 212, 96, 132);
            spr_ellipse(c, 32, 33 + bob, 17, 15, 228, 108, 146, 0.8f);
            spr_capsule(c, 16, 28 + bob, 9,  42 + bob, 5.5f, 218, 100, 138);
            spr_capsule(c, 48, 28 + bob, 55, 42 + bob, 5.5f, 218, 100, 138);
            spr_ellipse(c, 32, 18 + bob, 12, 10, 236, 116, 154, 0.75f);
            spr_capsule(c, 22, 12 + bob, 14, 5 + bob, 3.0f, 232, 222, 198); /* Bull horns */
            spr_capsule(c, 42, 12 + bob, 50, 5 + bob, 3.0f, 232, 222, 198);
            spr_ellipse(c, 32, 22 + bob, 7.5f, 4.2f, 64, 18, 28, 0.2f); /* Open fanged jaws */
            spr_box(c, 27, 20 + (int)bob, 29, 22 + (int)bob, 248, 246, 238);
            spr_box(c, 35, 20 + (int)bob, 37, 22 + (int)bob, 248, 246, 238);
            spr_ellipse(c, 27, 14 + bob, 2.2f, 2.2f, 255, 235, 55, 0.1f);
            spr_ellipse(c, 37, 14 + bob, 2.2f, 2.2f, 255, 235, 55, 0.1f);
            spr_outline(c);
            break;

        case SPR_CACODEMON: /* Iconic Horned Floating Red Cacodemon w/ Emerald Eye! */
            /* Radial bone horns around crimson sphere */
            for (int h = 0; h < 6; ++h) {
                float a = -2.4f + (float)h * 0.96f;
                spr_capsule(c, 32 + cosf(a) * 18, 30 + sinf(a) * 18 + bob,
                               32 + cosf(a) * 27, 30 + sinf(a) * 27 + bob, 2.8f, 218, 204, 172);
            }
            spr_ellipse(c, 32, 31 + bob, 21, 20, 218, 38, 42, 0.85f); /* Crimson orb body */
            spr_ellipse(c, 32, 25 + bob, 7.5f, 7.5f, 42, 248, 92, 0.4f); /* Huge emerald eye */
            spr_ellipse(c, 32, 25 + bob, 3.2f, 4.2f, 18, 42, 22, 0.2f); /* Slit pupil */
            spr_ellipse(c, 32, 40 + bob, 12.0f, 6.0f + f, 36, 62, 178, 0.4f); /* Blue cavern mouth */
            for (int t = -2; t <= 2; ++t) {
                spr_box(c, 31 + t * 4, 35 + (int)bob, 33 + t * 4, 38 + (int)bob, 248, 246, 236); /* Grinning fangs */
            }
            spr_outline(c);
            break;

        case SPR_GARGOYLE: /* Blood Winged Stone Gargoyle */
            spr_capsule(c, 26, 28, 4,  14 + f * 10, 5.0f, 108, 102, 118);
            spr_capsule(c, 38, 28, 60, 14 + f * 10, 5.0f, 108, 102, 118);
            spr_capsule(c, 25, 42, 22, 58, 4.2f, 118, 112, 128);
            spr_capsule(c, 39, 42, 42, 58, 4.2f, 118, 112, 128);
            spr_ellipse(c, 32, 31 + bob, 11, 13, 134, 128, 144, 0.8f);
            spr_ellipse(c, 32, 14 + bob, 8, 8.5f, 142, 136, 152, 0.8f);
            spr_capsule(c, 26, 8 + bob, 20, 2 + bob, 2.2f, 92, 86, 98);
            spr_capsule(c, 38, 8 + bob, 44, 2 + bob, 2.2f, 92, 86, 98);
            spr_ellipse(c, 29, 13 + bob, 2.2f, 2.0f, 255, 42, 42, 0.1f);
            spr_ellipse(c, 35, 13 + bob, 2.2f, 2.0f, 255, 42, 42, 0.1f);
            spr_outline(c);
            break;

        case SPR_MEDKIT: /* Beveled Steel First-Aid Medkit */
            spr_box(c, 10, 18, 53, 52, 232, 236, 244);
            spr_box(c, 22, 11, 41, 18, 145, 152, 165);
            spr_box(c, 28, 24, 35, 46, 225, 32, 42);
            spr_box(c, 21, 31, 42, 38, 225, 32, 42);
            spr_outline(c);
            break;

        case SPR_HOLO_DECOY: /* Scanline Hologram Projection */
            spr_ellipse(c, 32, 14, 7, 8, 72, 242, 255, 0.4f);
            spr_ellipse(c, 32, 32, 11, 14, 58, 228, 255, 0.4f);
            spr_capsule(c, 26, 44, 24, 60, 4.0f, 52, 212, 255);
            spr_capsule(c, 38, 44, 40, 60, 4.0f, 52, 212, 255);
            for (int y = 0; y < RETRO_SPRITE_PX; ++y) {
                if ((y + frame) % 3 == 0) {
                    for (int x = 0; x < RETRO_SPRITE_PX; ++x) c->px[y][x][3] /= 3;
                }
            }
            break;

        case SPR_KEYCARD: /* Detailed Beveled Sci-Fi Keycard */
            spr_box(c, 12, 16, 51, 48, 232, 238, 250);
            spr_box(c, 15, 22, 48, 28, 42, 48, 62);
            spr_box(c, 17, 33, 28, 42, 248, 212, 62);
            spr_outline(c);
            break;

        case SPR_SHARD_WEAPON: /* Row 0 = Radiant Memory Shard Star | Row 1 = Weapon Pickup */
            if (frame == 0) {
                spr_capsule(c, 32, 6,  32, 58, 4.5f, 255, 222, 72);
                spr_capsule(c, 8,  32, 56, 32, 4.5f, 255, 222, 72);
                spr_capsule(c, 15, 15, 49, 49, 3.5f, 255, 185, 42);
                spr_capsule(c, 49, 15, 15, 49, 3.5f, 255, 185, 42);
                spr_ellipse(c, 32, 32, 11, 11, 255, 248, 195, 0.3f);
                spr_outline(c);
            } else {
                spr_capsule(c, 10, 34, 54, 24, 4.2f, 152, 162, 178); /* Gun barrel */
                spr_capsule(c, 12, 39, 38, 33, 3.6f, 118, 126, 142);
                spr_capsule(c, 12, 40, 26, 48, 5.2f, 142, 78, 38);   /* Wood stock */
                spr_outline(c);
            }
            break;

        case SPR_FX_ORB: /* Row 0 = Radiant Energy Orb/Fireball | Row 1 = Portal Swirl */
            for (int y = 0; y < RETRO_SPRITE_PX; ++y) {
                for (int x = 0; x < RETRO_SPRITE_PX; ++x) {
                    float u = ((float)x - 31.5f) / 30.0f;
                    float v = ((float)y - 31.5f) / 30.0f;
                    float d = hypotf(u, v);
                    if (d >= 1.0f) continue;
                    if (frame == 0) {
                        float g = powf(1.0f - d, 1.6f);
                        spr_put(c, x, y, 255.0f, 165.0f + g * 90.0f, 80.0f + g * 175.0f, g * 255.0f);
                    } else {
                        float ang = atan2f(v, u) + d * 8.5f;
                        float sw = 0.5f + 0.5f * sinf(ang * 3.0f);
                        float fade = 1.0f - d * d;
                        spr_put(c, x, y, 120 + sw * 135, 170 + sw * 85, 255, fade * 235.0f);
                    }
                }
            }
            break;

        default:
            break;
    }
}

void retro_art_generate_sprite_atlas(uint8_t **out_rgba, int *out_w, int *out_h) {
    const int S = RETRO_SPRITE_PX;
    const int W = RETRO_SPRITE_COLS * S; /* 1280 */
    const int H = RETRO_SPRITE_ROWS * S; /* 128 */
    uint8_t *px = (uint8_t *)calloc((size_t)(W * H * 4), 1);
    if (!px) return;

    SprCanvas *canvas = (SprCanvas *)calloc(1, sizeof(SprCanvas));
    if (!canvas) { free(px); return; }

    for (int row = 0; row < RETRO_SPRITE_ROWS; ++row) {
        for (int col = 0; col < RETRO_SPRITE_COLS; ++col) {
            paint_sprite_tile(canvas, col, row);
            int ox = col * S;
            int oy = row * S;
            for (int y = 0; y < S; ++y) {
                memcpy(&px[((oy + y) * W + ox) * 4], canvas->px[y], (size_t)(S * 4));
            }
        }
    }
    free(canvas);
    *out_rgba = px;
    *out_w = W;
    *out_h = H;
}
