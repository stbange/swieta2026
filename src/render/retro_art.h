#ifndef SWIETA_RETRO_ART_H
#define SWIETA_RETRO_ART_H

#include <stdint.h>

#define RETRO_VOX_TILE_PX 64
#define RETRO_SPRITE_PX   64
#define RETRO_SPRITE_COLS 20
#define RETRO_SPRITE_ROWS 2

/* Sprite archetype columns (2 animation rows: row 0 = frame A, row 1 = frame B) */
typedef enum {
    SPR_ALIEN_TROOPER = 0,
    SPR_PIG_GUARD     = 1,
    SPR_OCTABRAIN     = 2,
    SPR_TURRET        = 3,
    SPR_WOLF          = 4,
    SPR_BAT           = 5,
    SPR_STONE_GUARD   = 6,
    SPR_SYNDICATE     = 7,
    SPR_OGRE_KNIGHT   = 8,
    SPR_FIEND_GHOUL   = 9,
    SPR_CULTIST       = 10,
    SPR_IMP           = 11,
    SPR_PINKY         = 12,
    SPR_CACODEMON     = 13,
    SPR_GARGOYLE      = 14,
    SPR_MEDKIT        = 15,
    SPR_HOLO_DECOY    = 16,
    SPR_KEYCARD       = 17,
    SPR_SHARD_WEAPON  = 18,
    SPR_FX_ORB        = 19
} SpriteCol;

/* Generates 1024x192 (16x3 tiles of 64x64) diffuse RGBA + tangent-space normal/gloss RGBA.
 * Caller must free(*out_diffuse) and free(*out_normal). */
void retro_art_generate_voxel_atlases(uint8_t **out_diffuse, uint8_t **out_normal, int *out_w, int *out_h);

/* Generates 1280x128 (20x2 tiles of 64x64) hand-shaded, dark-outlined 2-frame sprite atlas.
 * Caller must free(*out_rgba). */
void retro_art_generate_sprite_atlas(uint8_t **out_rgba, int *out_w, int *out_h);

#endif /* SWIETA_RETRO_ART_H */
