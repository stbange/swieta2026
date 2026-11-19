#ifndef SWIETA_WORLD_H
#define SWIETA_WORLD_H

#include "core/calendar.h"
#include "core/math3d.h"
#include "net/net_proto.h"
#include "render/voxel_mesher.h"

#include <stdbool.h>
#include <stdint.h>

#define WORLD_MAX_PLAYERS 4
#define WORLD_MAX_ENEMIES 48
#define WORLD_MAX_PROJECTILES 64
#define WORLD_MAX_PICKUPS 56
#define WORLD_MAX_TRIPMINES 16
#define WORLD_MAX_DOORS 12
#define WORLD_MAX_RELAYS 4
#define WORLD_MAX_SECRETS 4
#define WORLD_MAX_PUSHBLOCKS 8
#define WORLD_MAX_SWITCHES 12
#define WORLD_MAX_BARRELS 24
#define WORLD_MAX_TELEPORTERS 8
#define WORLD_MAX_JUMPPADS 8
#define WORLD_MAX_BOULDERS 4
#define WORLD_MAX_FLARES 12
#define WORLD_MAX_LIGHTS 16
#define WORLD_MAX_SLOTS 5
#define WORLD_FIXED_DT (1.0f / 60.0f)

typedef enum {
    SCENE_HUB = 0,
    SCENE_DAY01 = 1,
    SCENE_PREVIEW = 2,
    SCENE_DAY02 = 3,
    SCENE_DAY03 = 4,
    SCENE_DAY04 = 5,
    SCENE_DAY05 = 6,
    SCENE_DAY06 = 7
} SceneId;

typedef enum {
    /* Day 1 — Duke Nukem 3D */
    WEAP_PISTOL = 0,
    WEAP_SHOTGUN = 1,
    WEAP_SHRINKER = 2,
    WEAP_PIPEBOMB = 3,
    WEAP_TRIPMINE = 4,
    /* Day 2 — Tomb Raider */
    WEAP_DUAL_PISTOLS = 5,
    WEAP_FLARE = 6,
    WEAP_MAGNUM = 7,
    /* Day 3 — Counter-Strike */
    WEAP_USP = 8,
    WEAP_DEAGLE = 9,
    WEAP_CARBINE = 10,
    WEAP_SNIPER = 11,
    WEAP_FLASH_SMOKE = 12,
    /* Day 4 — Quake */
    WEAP_NAILGUN = 13,
    WEAP_ROCKET = 14,
    WEAP_THUNDERBOLT = 15,
    /* Day 5 — Blood */
    WEAP_PITCHFORK = 16,
    WEAP_FLAREGUN = 17,
    WEAP_AEROSOL = 18,
    WEAP_TNT = 19,
    WEAP_VOODOO = 20,
    /* Day 6 — Doom */
    WEAP_CHAINSAW = 21,
    WEAP_SUPER_SHOTGUN = 22,
    WEAP_CHAINGUN = 23,
    WEAP_BFG = 24,
    WEAP_COUNT = 25
} WeaponId;

typedef enum {
    KEY_BLUE   = 1u << 0,
    KEY_RED    = 1u << 1,
    KEY_YELLOW = 1u << 2
} KeycardBits;

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
    float move_x;         /* -1..1 strafe right */
    float move_y;         /* -1..1 forward */
    float look_dx;        /* yaw delta rad */
    float look_dy;        /* pitch delta rad */
    uint16_t buttons;
    int8_t select_weapon; /* -1 = no change, 0..WEAP_COUNT-1 = explicit WeaponId */
} PlayerInput;

typedef struct {
    bool active;
    Vec3 pos;       /* feet position */
    Vec3 vel;
    float yaw;
    float pitch;
    bool on_ground;
    bool in_water;
    int hp;
    int max_hp;
    int armor;
    uint32_t keys;          /* KeycardBits */
    bool has_shotgun;       /* kept synced with owned_weapons for Day 1 compatibility */
    uint32_t owned_weapons; /* bitmask of (1u << WeaponId) */
    WeaponId weapon;
    int ammo_shells;
    int ammo_shrink;
    int ammo_pipebomb;
    int ammo_tripmine;
    int ammo_bullets;       /* Carbine / Nailgun / Chaingun / Magnum / Deagle */
    int ammo_rockets;       /* Rocket Launcher / TNT / Aerosol / Flare Gun */
    int ammo_cells;         /* Thunderbolt / Yule-BFG / Voodoo */
    int ammo_utility;       /* Magnesium Flares / CS Flashbang & Smoke */
    int cash;               /* Day 3 CS economy ($) */
    bool has_defuse_kit;    /* Day 3 CS Defuse Kit (2x speed) */
    bool zoomed;            /* Day 3 CS Sniper 2.5x scope toggle */
    float fire_cooldown;
    float jetpack_fuel;     /* 0..100 */
    bool jetpack_active;
    float holo_cooldown;
    float self_shrunk_timer;/* >0 when player shrunk via Day 1 Mirror reflection */
    float quad_timer;       /* >0 Day 4 Quad Damage 4x boost */
    float berserk_timer;    /* >0 Day 6 Berserk melee frenzy */
    float rad_suit_timer;   /* >0 Day 6 Radiation Suit slime immunity */
    float flash_blind_timer;/* >0 Day 3 Flashbang white fade */
    float dodge_cooldown;   /* Day 2 Tomb Raider acrobatic flip */
    float hazard_tick;      /* Lava / Slime / Spikes tick timer */
    float lightning_timer;  /* Day 4 Thunderbolt visual arc timer */
    Vec3 lightning_end;
    float defuse_progress;  /* Day 3 C4 defusal progress 0..1 */
    float walk_bob;
    float hitmarker_timer;
    float stomp_flash;
    float damage_flash;
    float muzzle_flash;
    int score;
    int stomps;
    uint16_t prev_buttons;
    float respawn_timer;
} Player;

typedef enum {
    /* Day 1 — Duke Nukem 3D */
    ENEMY_TROOPER = 0,
    ENEMY_PIGGUARD = 1,
    ENEMY_OCTABRAIN = 2,
    ENEMY_TURRET = 3,
    ENEMY_BOSS_MECH = 4,
    /* Day 2 — Tomb Raider */
    ENEMY_WOLF = 5,
    ENEMY_BAT = 6,
    ENEMY_STONE_GUARD = 7,
    ENEMY_BOSS_TREX = 8,
    /* Day 3 — Counter-Strike */
    ENEMY_SYNDICATE_RIFLE = 9,
    ENEMY_SYNDICATE_SNIPER = 10,
    /* Day 4 — Quake */
    ENEMY_OGRE = 11,
    ENEMY_FIEND = 12,
    ENEMY_DEATH_KNIGHT = 13,
    ENEMY_BOSS_SHAMBLER = 14,
    /* Day 5 — Blood */
    ENEMY_CULTIST = 15,
    ENEMY_GHOUL = 16,
    ENEMY_GARGOYLE = 17,
    ENEMY_BOSS_MATRIARCH = 18,
    /* Day 6 — Doom */
    ENEMY_IMP = 19,
    ENEMY_PINKY = 20,
    ENEMY_CACODEMON = 21,
    ENEMY_BOSS_CYBERTITAN = 22
} EnemyKind;

typedef struct {
    bool active;
    EnemyKind kind;
    Vec3 pos;
    Vec3 vel;
    float yaw;
    int hp;
    int max_hp;
    float shrunk_timer;  /* >0 means shrunk to 0.35x and stompable */
    float burn_timer;    /* >0 Day 5 Blood burning DoT */
    float burn_tick;
    int burn_owner;
    float blind_timer;   /* >0 Day 3 CS Flashbang stunned */
    int infight_target;  /* -1 = players/decoy, >=0 = enemy index (Doom infighting!) */
    float fire_cooldown;
    float anim_time;
    float hit_flash;
    float base_y;        /* hover height for flyers */
} Enemy;

typedef enum {
    PROJ_SHRINK_ORB = 0,
    PROJ_PIPEBOMB = 1,
    PROJ_ENEMY_PLASMA = 2,
    PROJ_BOSS_ROCKET = 3,
    PROJ_OCTA_WAVE = 4,
    PROJ_NAIL = 5,
    PROJ_PLAYER_ROCKET = 6,
    PROJ_FLARE_BOLT = 7,
    PROJ_FLAME_PUFF = 8,
    PROJ_MOLOTOV = 9,
    PROJ_TNT_BUNDLE = 10,
    PROJ_TNT_PROX = 11,
    PROJ_BFG_ORB = 12,
    PROJ_FLASHBANG = 13,
    PROJ_ENEMY_GRENADE = 14,
    PROJ_ENEMY_FIREBALL = 15
} ProjectileKind;

typedef struct {
    bool active;
    ProjectileKind kind;
    int owner_player; /* -1 for enemy */
    int owner_enemy;  /* >=0 for Doom monster infighting */
    bool reflected;   /* true after bouncing off VOX_MIRROR */
    Vec3 pos;
    Vec3 vel;
    float life;
} Projectile;

typedef enum {
    PICKUP_MEDKIT = 0,
    PICKUP_ARMOR = 1,
    PICKUP_SHOTGUN = 2,
    PICKUP_SHELLS = 3,
    PICKUP_SHRINK_AMMO = 4,
    PICKUP_PIPEBOMB = 5,
    PICKUP_TRIPMINE = 6,
    PICKUP_JETPACK = 7,
    PICKUP_KEY_BLUE = 8,
    PICKUP_KEY_RED = 9,
    PICKUP_SHARD = 10,
    PICKUP_KEY_YELLOW = 11,
    PICKUP_WEAPON_GRANT = 12, /* grants pickup.weapon_grant + ammo */
    PICKUP_BULLETS = 13,
    PICKUP_ROCKETS = 14,
    PICKUP_CELLS = 15,
    PICKUP_UTILITY = 16,
    PICKUP_QUAD = 17,
    PICKUP_BERSERK = 18,
    PICKUP_RADSUIT = 19,
    PICKUP_CASH = 20
} PickupKind;

typedef struct {
    bool active;
    PickupKind kind;
    WeaponId weapon_grant;
    Vec3 pos;
    float bob_phase;
} Pickup;

typedef struct {
    bool active;
    int owner_player;
    Vec3 pos;        /* wall mount position */
    Vec3 normal;     /* beam direction */
    Vec3 beam_end;   /* opposite wall hit point */
    float arm_timer; /* arms in 0.4s */
} Tripmine;

typedef struct {
    bool active;
    bool open;
    int x0, y0, z0;
    int x1, y1, z1;
    uint32_t required_key; /* 0 = switch controlled, else KeycardBits */
    uint8_t voxel_type;
    Vec3 center;
} DoorGate;

typedef struct {
    bool active;
    bool destroyed;
    int vx, vy, vz;
    int hp;
    Vec3 pos;
} RelayTarget;

typedef struct {
    bool active;
    bool found;
    Vec3 pos;
    float radius;
    const char *name;
} SecretZone;

typedef struct {
    bool active;
    bool settled;
    int vx, vy, vz;       /* bottom-west-north voxel corner of 2x2x2 push-block */
    int target_x, target_z;
    int door_idx;         /* door opened when block settles onto target plate */
    Vec3 center;
} PushBlock;

typedef enum {
    SW_DOOR_LEVER = 0,
    SW_CISTERN_VALVE = 1,
    SW_CS_BUY_CRATE = 2,
    SW_CS_C4_BOMB = 3,
    SW_GHOST_TRAIN = 4,
    SW_EXIT_SHARD = 5
} SwitchKind;

typedef struct {
    bool active;
    bool activated;
    SwitchKind kind;
    Vec3 pos;
    int door_idx;
    int tag;
    float progress;
    const char *label;
} SwitchTrigger;

typedef struct {
    bool active;
    Vec3 pos;
    int hp;
} Barrel;

typedef struct {
    bool active;
    Vec3 src;
    Vec3 dst;
    float dst_yaw;
} Teleporter;

typedef struct {
    bool active;
    Vec3 pos;
    Vec3 boost_vel;
} JumpPad;

typedef struct {
    bool active;
    bool rolling;
    Vec3 pos;
    Vec3 vel;
    Vec3 trigger_pos;
    float trigger_radius;
    float stop_z;
} BoulderTrap;

typedef struct {
    bool active;
    bool is_smoke;
    Vec3 pos;
    Vec3 vel;
    float timer;
    float r, g, b;
} FlareLight;

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
    int santa_log_index;

    /* Per-level weapon loadout mapped to keys 1..5 */
    WeaponId level_weapons[WORLD_MAX_SLOTS];
    int level_weapon_count;

    int player_count;
    Player players[WORLD_MAX_PLAYERS];

    VoxelWorld voxels;
    PortalGate portals[SWIETA_PORTAL_COUNT];
    Vec3 return_portal_pos;

    Enemy enemies[WORLD_MAX_ENEMIES];
    Projectile projectiles[WORLD_MAX_PROJECTILES];
    Pickup pickups[WORLD_MAX_PICKUPS];
    Tripmine tripmines[WORLD_MAX_TRIPMINES];
    DoorGate doors[WORLD_MAX_DOORS];
    int door_count;
    RelayTarget relays[WORLD_MAX_RELAYS];
    int relay_count;
    SecretZone secrets[WORLD_MAX_SECRETS];
    int secret_count;
    int secrets_found;

    /* Phase 2 shared level mechanics */
    PushBlock push_blocks[WORLD_MAX_PUSHBLOCKS];
    int push_block_count;
    SwitchTrigger switches[WORLD_MAX_SWITCHES];
    int switch_count;
    Barrel barrels[WORLD_MAX_BARRELS];
    int barrel_count;
    Teleporter teleporters[WORLD_MAX_TELEPORTERS];
    int teleporter_count;
    JumpPad jump_pads[WORLD_MAX_JUMPPADS];
    int jump_pad_count;
    BoulderTrap boulders[WORLD_MAX_BOULDERS];
    int boulder_count;
    FlareLight flares[WORLD_MAX_FLARES];

    HoloDecoy decoy;
    ExplosionFx explosions[8];

    /* Checkpoints & Mission Progression */
    Vec3 checkpoint_pos;
    float checkpoint_yaw;
    int checkpoint_stage;

    /* Day 1..6 Mission Flags */
    bool cinema_restored;
    bool secret_wall_blasted;
    bool relays_cleared;
    bool cistern_flooded;    /* Day 2 */
    int bombs_defused;       /* Day 3 (0..2) */
    bool train_activated;    /* Day 5 */
    int boss_phase;          /* 1, 2, 3 */
    bool boss_defeated;
    bool nuke_exit_ready;
    Vec3 cinema_switch_pos;
    Vec3 nuke_switch_pos;
    Vec3 santa_bios_pos;
    Vec3 objective_pos;
    char objective_text[96];

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
    SFX_PISTOL    = 1u << 0,
    SFX_SHOTGUN   = 1u << 1,
    SFX_SHRINK    = 1u << 2,
    SFX_STOMP     = 1u << 3,
    SFX_THROW     = 1u << 4,
    SFX_EXPLODE   = 1u << 5,
    SFX_JETPACK   = 1u << 6,
    SFX_PICKUP    = 1u << 7,
    SFX_SWITCH    = 1u << 8,
    SFX_PORTAL    = 1u << 9,
    SFX_HURT      = 1u << 10,
    SFX_SHARD     = 1u << 11,
    SFX_GLASS     = 1u << 12,
    SFX_DOOR      = 1u << 13,
    SFX_SECRET    = 1u << 14,
    SFX_ROCKET    = 1u << 15,
    SFX_LIGHTNING = 1u << 16,
    SFX_FLAME     = 1u << 17,
    SFX_MELEE     = 1u << 18,
    SFX_TELEPORT  = 1u << 19,
    SFX_QUAD      = 1u << 20,
    SFX_FLASH     = 1u << 21
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
float player_height(const Player *p);
Vec3 player_eye_pos(const Player *p);
Vec3 player_forward(const Player *p);
const PortalGate *world_find_nearby_portal(const GameWorld *w, Vec3 pos, float max_dist);
WeaponId world_slot_weapon(const GameWorld *w, int slot);
const char *world_weapon_short_name(WeaponId id);
int world_player_weapon_ammo(const Player *p, WeaponId id);
bool world_enemy_is_boss(EnemyKind kind);

#endif /* SWIETA_WORLD_H */
