#ifndef SWIETA_WORLD_H
#define SWIETA_WORLD_H

#include "core/calendar.h"
#include "core/math3d.h"
#include "net/net_proto.h"
#include "render/voxel_mesher.h"

#include <stdbool.h>
#include <stdint.h>

#define WORLD_MAX_PLAYERS 4
#define WORLD_MAX_ENEMIES 24
#define WORLD_MAX_PROJECTILES 32
#define WORLD_MAX_PICKUPS 24
#define WORLD_MAX_LIGHTS 16
#define WORLD_FIXED_DT (1.0f / 60.0f)

typedef enum {
    SCENE_HUB = 0,
    SCENE_DAY01 = 1,
    SCENE_PREVIEW = 2
} SceneId;

typedef enum {
    WEAP_PISTOL = 0,
    WEAP_SHRINKER = 1,
    WEAP_PIPEBOMB = 2,
    WEAP_COUNT = 3
} WeaponId;

typedef enum {
    INP_FIRE      = 1u << 0,
    INP_ALT       = 1u << 1,
    INP_JUMP      = 1u << 2,
    INP_USE       = 1u << 3,
    INP_JETPACK   = 1u << 4,
    INP_HOLO      = 1u << 5,
    INP_SPRINT    = 1u << 6,
    INP_NEXT_WEAP = 1u << 7
} InputButtonBits;

typedef struct {
    float move_x;   /* -1..1 strafe right */
    float move_y;   /* -1..1 forward */
    float look_dx;  /* yaw delta rad */
    float look_dy;  /* pitch delta rad */
    uint16_t buttons;
    int8_t select_weapon; /* -1 = no change, 0..2 = explicit */
} PlayerInput;

typedef struct {
    bool active;
    Vec3 pos;       /* feet position */
    Vec3 vel;
    float yaw;
    float pitch;
    bool on_ground;
    int hp;
    int max_hp;
    int armor;
    WeaponId weapon;
    int ammo_shrink;
    int ammo_pipebomb;
    float fire_cooldown;
    float jetpack_fuel;     /* 0..100 */
    bool jetpack_active;
    float holo_cooldown;
    float stomp_flash;
    float damage_flash;
    float muzzle_flash;
    int score;
    int stomps;
    uint16_t prev_buttons;
    float respawn_timer;
} Player;

typedef enum {
    ENEMY_TROOPER = 0,
    ENEMY_PIGGUARD = 1,
    ENEMY_BOSS_MECH = 2
} EnemyKind;

typedef struct {
    bool active;
    EnemyKind kind;
    Vec3 pos;
    Vec3 vel;
    float yaw;
    int hp;
    int max_hp;
    float shrunk_timer;  /* >0 means shrunk to 0.3x and stompable */
    float fire_cooldown;
    float anim_time;
    float hit_flash;
} Enemy;

typedef enum {
    PROJ_SHRINK_ORB = 0,
    PROJ_PIPEBOMB = 1,
    PROJ_ENEMY_PLASMA = 2,
    PROJ_BOSS_ROCKET = 3
} ProjectileKind;

typedef struct {
    bool active;
    ProjectileKind kind;
    int owner_player; /* -1 for enemy */
    Vec3 pos;
    Vec3 vel;
    float life;
} Projectile;

typedef enum {
    PICKUP_MEDKIT = 0,
    PICKUP_ARMOR = 1,
    PICKUP_SHRINK_AMMO = 2,
    PICKUP_PIPEBOMB = 3,
    PICKUP_JETPACK = 4,
    PICKUP_SHARD = 5
} PickupKind;

typedef struct {
    bool active;
    PickupKind kind;
    Vec3 pos;
    float bob_phase;
} Pickup;

typedef struct {
    bool active;
    int owner_player;
    Vec3 pos;
    float timer;
} HoloDecoy;

typedef struct {
    int day;
    Vec3 pos;
    float facing_yaw;
} PortalGate;

typedef struct {
    Vec3 pos;
    float r, g, b;
    float intensity;
    float radius;
} DynamicLight;

typedef struct {
    bool active;
    Vec3 pos;
    float radius;
    float timer;
    float r, g, b;
} ExplosionFx;

typedef struct {
    SceneId scene;
    int active_day;
    uint32_t tick;
    float time_sec;

    SwietaDateTime now;
    bool unlock_all;
    uint32_t shard_mask;

    int player_count;
    Player players[WORLD_MAX_PLAYERS];

    VoxelWorld voxels;
    PortalGate portals[SWIETA_PORTAL_COUNT];
    Vec3 return_portal_pos;

    Enemy enemies[WORLD_MAX_ENEMIES];
    Projectile projectiles[WORLD_MAX_PROJECTILES];
    Pickup pickups[WORLD_MAX_PICKUPS];
    HoloDecoy decoy;
    ExplosionFx explosions[8];

    /* Day 1 Mission Flags */
    bool cinema_restored;
    bool secret_wall_blasted;
    bool boss_defeated;
    bool nuke_exit_ready;
    Vec3 cinema_switch_pos;
    Vec3 nuke_switch_pos;
    Vec3 santa_bios_pos;

    /* Dynamic light list rebuilt each tick */
    int light_count;
    DynamicLight lights[WORLD_MAX_LIGHTS];

    /* HUD banner message */
    char banner[128];
    float banner_timer;

    /* Sound event bitmask consumed by audio engine */
    uint32_t sound_events;
} GameWorld;

typedef enum {
    SFX_PISTOL   = 1u << 0,
    SFX_SHRINK   = 1u << 1,
    SFX_STOMP    = 1u << 2,
    SFX_THROW    = 1u << 3,
    SFX_EXPLODE  = 1u << 4,
    SFX_JETPACK  = 1u << 5,
    SFX_PICKUP   = 1u << 6,
    SFX_SWITCH   = 1u << 7,
    SFX_PORTAL   = 1u << 8,
    SFX_HURT     = 1u << 9,
    SFX_SHARD    = 1u << 10
} SoundEventBits;

void world_init(GameWorld *w, SwietaDateTime now, bool unlock_all, int player_count);
void world_set_player_count(GameWorld *w, int count);
void world_load_hub(GameWorld *w);
void world_load_day(GameWorld *w, int day);
void world_step(GameWorld *w, const PlayerInput inputs[WORLD_MAX_PLAYERS], float dt);

/* Network snapshot helpers */
void world_build_snapshot(const GameWorld *w, NetSnapshotPacket *out_snap);
void world_apply_snapshot(GameWorld *w, const NetSnapshotPacket *snap, int local_player_slot);

/* Utility queries */
Vec3 player_eye_pos(const Player *p);
Vec3 player_forward(const Player *p);
const PortalGate *world_find_nearby_portal(const GameWorld *w, Vec3 pos, float max_dist);

#endif /* SWIETA_WORLD_H */
