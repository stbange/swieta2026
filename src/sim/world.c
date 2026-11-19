#include "sim/world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float kPlayerRadius = 0.36f;
static const float kPlayerTallHeight = 1.72f;
static const float kPlayerTallEye = 1.52f;
static const float kPlayerShrunkHeight = 0.62f;
static const float kPlayerShrunkEye = 0.46f;
static const float kGravity = 22.0f;

static const char *kSantaLogs[6] = {
    "SANTA-BIOS [1999-12-24]: NODE-24 ONLINE! 24 CLASSIC SECTORS LOADED FOR XMAS LAN!",
    "SANTA-BIOS [2026-12-01]: WARNING! KRAMPUS.EXE FROZE ALL 24 PORTALS INTO STATIC!",
    "SANTA-BIOS [TIP]: IN DAY 1, SHOOT THE CINEMA MIRROR WITH SHRINK-RAY TO ENTER THE VENT!",
    "SANTA-BIOS [LOG #4]: FOUND PIZZA BOXES ON PENTIUM II SERVER. TEMPERATURE NOMINAL.",
    "SANTA-BIOS [TIP]: PLANT LASER TRIPMINES ON WALLS & USE HOLO-DECOY AGAINST TURRETS!",
    "SANTA-BIOS [GOAL]: COLLECT MEMORY SHARDS TO LIGHT ALL 24 ORNAMENTS ON THE GRAND TREE!"
};

static void set_banner(GameWorld *w, const char *msg, float duration) {
    if (!w || !msg) return;
    snprintf(w->banner, sizeof(w->banner), "%s", msg);
    w->banner_timer = duration;
}

static void set_objective(GameWorld *w, Vec3 pos, const char *txt) {
    w->objective_pos = pos;
    snprintf(w->objective_text, sizeof(w->objective_text), "%s", txt ? txt : "");
}

static void add_light(GameWorld *w, Vec3 pos, float r, float g, float b, float intensity, float radius) {
    if (w->light_count >= WORLD_MAX_LIGHTS) return;
    DynamicLight *l = &w->lights[w->light_count++];
    l->pos = pos;
    l->r = r;
    l->g = g;
    l->b = b;
    l->intensity = intensity;
    l->radius = radius;
}

static void spawn_explosion(GameWorld *w, Vec3 pos, float radius, float r, float g, float b) {
    for (int i = 0; i < 8; ++i) {
        if (!w->explosions[i].active) {
            w->explosions[i].active = true;
            w->explosions[i].pos = pos;
            w->explosions[i].radius = radius;
            w->explosions[i].timer = 0.45f;
            w->explosions[i].r = r;
            w->explosions[i].g = g;
            w->explosions[i].b = b;
            return;
        }
    }
}

float player_height(const Player *p) {
    return (p && p->self_shrunk_timer > 0.0f) ? kPlayerShrunkHeight : kPlayerTallHeight;
}

Vec3 player_eye_pos(const Player *p) {
    float eye = (p && p->self_shrunk_timer > 0.0f) ? kPlayerShrunkEye : kPlayerTallEye;
    return vec3_make(p->pos.x, p->pos.y + eye, p->pos.z);
}

Vec3 player_forward(const Player *p) {
    float cp = cosf(p->pitch);
    return vec3_make(sinf(p->yaw) * cp, sinf(p->pitch), -cosf(p->yaw) * cp);
}

static void reset_player_for_spawn(Player *p, Vec3 pos, float yaw, bool keep_progress) {
    int prev_score = keep_progress ? p->score : 0;
    int prev_stomps = keep_progress ? p->stomps : 0;
    uint32_t prev_keys = keep_progress ? p->keys : 0u;
    bool prev_shotgun = keep_progress ? p->has_shotgun : false;
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->pos = pos;
    p->yaw = yaw;
    p->pitch = 0.0f;
    p->hp = 100;
    p->max_hp = 100;
    p->armor = 25;
    p->keys = prev_keys;
    p->has_shotgun = prev_shotgun;
    p->weapon = prev_shotgun ? WEAP_SHOTGUN : WEAP_PISTOL;
    p->ammo_shells = 16;
    p->ammo_shrink = 14;
    p->ammo_pipebomb = 6;
    p->ammo_tripmine = 4;
    p->jetpack_fuel = 100.0f;
    p->score = prev_score;
    p->stomps = prev_stomps;
}

static void add_enemy(GameWorld *w, EnemyKind kind, Vec3 pos) {
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        if (!w->enemies[i].active) {
            Enemy *e = &w->enemies[i];
            memset(e, 0, sizeof(*e));
            e->active = true;
            e->kind = kind;
            e->pos = pos;
            e->base_y = pos.y;
            switch (kind) {
                case ENEMY_TROOPER:   e->max_hp = 45;  break;
                case ENEMY_PIGGUARD:  e->max_hp = 85;  break;
                case ENEMY_OCTABRAIN: e->max_hp = 75;  break;
                case ENEMY_TURRET:    e->max_hp = 90;  break;
                case ENEMY_BOSS_MECH: e->max_hp = 360; break;
            }
            e->hp = e->max_hp;
            e->fire_cooldown = 0.9f + (float)(i % 4) * 0.25f;
            return;
        }
    }
}

static void add_pickup(GameWorld *w, PickupKind kind, Vec3 pos) {
    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        if (!w->pickups[i].active) {
            w->pickups[i].active = true;
            w->pickups[i].kind = kind;
            w->pickups[i].pos = pos;
            w->pickups[i].bob_phase = (float)i * 0.65f;
            return;
        }
    }
}

static void add_door(GameWorld *w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t key, uint8_t vtype) {
    if (w->door_count >= WORLD_MAX_DOORS) return;
    DoorGate *d = &w->doors[w->door_count++];
    d->active = true;
    d->open = false;
    d->x0 = x0; d->y0 = y0; d->z0 = z0;
    d->x1 = x1; d->y1 = y1; d->z1 = z1;
    d->required_key = key;
    d->voxel_type = vtype;
    d->center = vec3_make((float)(x0 + x1 + 1) * 0.5f, (float)y0 + 1.0f, (float)(z0 + z1 + 1) * 0.5f);
    voxel_fill_box(&w->voxels, x0, y0, z0, x1, y1, z1, vtype);
}

static void open_door(GameWorld *w, DoorGate *d) {
    if (!d || !d->active || d->open) return;
    d->open = true;
    voxel_fill_box(&w->voxels, d->x0, d->y0, d->z0, d->x1, d->y1, d->z1, VOX_AIR);
    w->sound_events |= SFX_DOOR;
}

static void add_relay(GameWorld *w, int vx, int vy, int vz) {
    if (w->relay_count >= WORLD_MAX_RELAYS) return;
    RelayTarget *rt = &w->relays[w->relay_count++];
    rt->active = true;
    rt->destroyed = false;
    rt->vx = vx; rt->vy = vy; rt->vz = vz;
    rt->hp = 60;
    rt->pos = vec3_make((float)vx + 0.5f, (float)vy + 1.0f, (float)vz + 0.5f);
    voxel_fill_box(&w->voxels, vx, vy, vz, vx, vy + 2, vz, VOX_RELAY_CORE);
}

static void add_secret(GameWorld *w, Vec3 pos, float radius, const char *name) {
    if (w->secret_count >= WORLD_MAX_SECRETS) return;
    SecretZone *sz = &w->secrets[w->secret_count++];
    sz->active = true;
    sz->found = false;
    sz->pos = pos;
    sz->radius = radius;
    sz->name = name;
}

static void clear_entities(GameWorld *w) {
    memset(w->enemies, 0, sizeof(w->enemies));
    memset(w->projectiles, 0, sizeof(w->projectiles));
    memset(w->pickups, 0, sizeof(w->pickups));
    memset(w->tripmines, 0, sizeof(w->tripmines));
    memset(w->doors, 0, sizeof(w->doors));
    w->door_count = 0;
    memset(w->relays, 0, sizeof(w->relays));
    w->relay_count = 0;
    memset(w->secrets, 0, sizeof(w->secrets));
    w->secret_count = 0;
    w->secrets_found = 0;
    memset(&w->decoy, 0, sizeof(w->decoy));
    memset(w->explosions, 0, sizeof(w->explosions));
}

static void setup_hub_portals(GameWorld *w) {
    /* Act I (Days 1..6): West Wing (x=28.5), Day 1 closest to spawn */
    static const float wing_z[6] = {84.5f, 75.5f, 66.5f, 57.5f, 48.5f, 39.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[i].day = i + 1;
        w->portals[i].pos = vec3_make(28.5f, 1.0f, wing_z[i]);
        w->portals[i].facing_yaw = SW_PI * 0.5f;
    }
    /* Act II (Days 7..12): East Wing (x=99.5) */
    for (int i = 0; i < 6; ++i) {
        w->portals[6 + i].day = 7 + i;
        w->portals[6 + i].pos = vec3_make(99.5f, 1.0f, wing_z[i]);
        w->portals[6 + i].facing_yaw = -SW_PI * 0.5f;
    }
    /* Act III (Days 13..18): South Wing (z=99.5) */
    static const float south_x[6] = {38.5f, 48.5f, 58.5f, 70.5f, 80.5f, 90.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[12 + i].day = 13 + i;
        w->portals[12 + i].pos = vec3_make(south_x[i], 1.0f, 99.5f);
        w->portals[12 + i].facing_yaw = 0.0f;
    }
    /* Act IV (Days 19..24): North Wing flanking Grand Fireplace (z=28.5) */
    static const float north_x[6] = {34.5f, 42.5f, 50.5f, 78.5f, 86.5f, 94.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[18 + i].day = 19 + i;
        w->portals[18 + i].pos = vec3_make(north_x[i], 1.0f, 28.5f);
        w->portals[18 + i].facing_yaw = SW_PI;
    }
}

void world_load_hub(GameWorld *w) {
    if (!w) return;
    w->scene = SCENE_HUB;
    w->active_day = 0;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    /* Vaulted Cathedral footprint: x=26..101, z=26..101, y=0..16 */
    voxel_fill_box(&w->voxels, 26, 0, 26, 101, 0, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 61, 0, 30, 67, 0, 96, VOX_CARPET);
    voxel_fill_box(&w->voxels, 30, 0, 61, 96, 0, 67, VOX_CARPET);
    /* Red runner straight to Day 1 Portal */
    voxel_fill_box(&w->voxels, 30, 0, 83, 62, 0, 86, VOX_CARPET);

    /* Outer Cathedral stone walls + vaulted timber ceiling */
    voxel_fill_box(&w->voxels, 26, 1, 26, 101, 15, 26, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 1, 101, 101, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 1, 26, 26, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 101, 1, 26, 101, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 16, 26, 101, 16, 101, VOX_WOOD);

    /* Stained-glass high windows */
    for (int z = 38; z <= 90; z += 18) {
        voxel_fill_box(&w->voxels, 26, 8, z, 26, 13, z + 4, VOX_GLASS);
        voxel_fill_box(&w->voxels, 101, 8, z, 101, 13, z + 4, VOX_GLASS);
    }

    /* Timber colonnade pillars & roof beams */
    for (int p = 42; p <= 86; p += 22) {
        voxel_fill_box(&w->voxels, 44, 1, p, 45, 15, p + 1, VOX_WOOD);
        voxel_fill_box(&w->voxels, 82, 1, p, 83, 15, p + 1, VOX_WOOD);
        voxel_fill_box(&w->voxels, 44, 14, p, 83, 15, p + 1, VOX_WOOD);
    }

    /* Grand Fireplace on North Wall (x=59..69, z=27..29) */
    voxel_fill_box(&w->voxels, 58, 1, 27, 70, 9, 29, VOX_BRICK);
    voxel_fill_box(&w->voxels, 60, 1, 28, 68, 4, 29, VOX_AIR);
    voxel_fill_box(&w->voxels, 61, 1, 28, 67, 3, 28, VOX_FIRE);
    voxel_fill_box(&w->voxels, 57, 5, 30, 71, 5, 30, VOX_WOOD);
    voxel_set(&w->voxels, 58, 6, 30, VOX_GOLD);
    voxel_set(&w->voxels, 70, 6, 30, VOX_GOLD);

    /* Towering 24-Ornament Voxel Christmas Tree at (64, 1..14, 64) */
    voxel_fill_box(&w->voxels, 63, 1, 63, 65, 3, 65, VOX_WOOD);
    voxel_fill_box(&w->voxels, 59, 4, 59, 69, 6, 69, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 60, 7, 60, 68, 9, 68, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 61, 10, 61, 67, 11, 67, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 62, 12, 62, 66, 13, 66, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 63, 14, 63, 65, 14, 65, VOX_LEAVES);
    voxel_set(&w->voxels, 64, 15, 64, VOX_GOLD);

    /* Wrapped gifts beneath the Grand Tree */
    voxel_set(&w->voxels, 60, 1, 61, VOX_GIFT);
    voxel_set(&w->voxels, 68, 1, 62, VOX_GIFT);
    voxel_set(&w->voxels, 62, 1, 68, VOX_GIFT);
    voxel_set(&w->voxels, 67, 1, 67, VOX_GIFT);

    /* 24 distinct ornament positions on the Grand Tree — illuminate per collected Memory Shard! */
    static const int kOrn[24][3] = {
        {59,4,64},{69,4,64},{64,4,59},{64,4,69},{60,5,60},{68,5,60},{60,5,68},{68,5,68},
        {60,7,64},{68,7,64},{64,7,60},{64,7,68},{61,8,61},{67,8,61},{61,8,67},{67,8,67},
        {61,10,64},{67,10,64},{64,10,61},{64,10,67},{62,12,64},{66,12,64},{64,12,62},{64,12,66}
    };
    for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
        if (w->shard_mask & (1u << (unsigned)i)) {
            uint8_t col = (i % 3 == 0) ? VOX_GOLD : ((i % 3 == 1) ? VOX_NEON_PINK : VOX_NEON_CYAN);
            voxel_set(&w->voxels, kOrn[i][0], kOrn[i][1], kOrn[i][2], col);
        }
    }

    /* SANTA-BIOS 1999 Server Terminal */
    w->santa_bios_pos = vec3_make(64.5f, 1.0f, 52.0f);
    voxel_fill_box(&w->voxels, 63, 1, 50, 66, 1, 51, VOX_METAL);
    voxel_fill_box(&w->voxels, 64, 2, 50, 65, 2, 50, VOX_NEON_CYAN);

    /* Portal Stone Arches (2-voxel wide doorways) */
    setup_hub_portals(w);
    for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
        int px = (int)floorf(w->portals[i].pos.x);
        int pz = (int)floorf(w->portals[i].pos.z);
        if (px <= 30 || px >= 98) {
            voxel_fill_box(&w->voxels, px, 1, pz - 1, px, 5, pz - 1, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px, 1, pz + 2, px, 5, pz + 2, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px, 5, pz - 1, px, 5, pz + 2, VOX_PORTAL_FRAME);
        } else {
            voxel_fill_box(&w->voxels, px - 1, 1, pz, px - 1, 5, pz, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px + 2, 1, pz, px + 2, 5, pz, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px - 1, 5, pz, px + 2, 5, pz, VOX_PORTAL_FRAME);
        }
    }
    /* Highlight Day 1 Portal with neon beacon accents */
    voxel_set(&w->voxels, 29, 5, 83, VOX_NEON_PINK);
    voxel_set(&w->voxels, 29, 5, 86, VOX_NEON_CYAN);

    w->checkpoint_pos = vec3_make(63.5f, 1.0f, 84.5f);
    w->checkpoint_yaw = 0.0f;
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(62.5f + (float)i * 1.2f, 1.0f, 84.5f);
            reset_player_for_spawn(&w->players[i], sp, 0.0f, false);
        } else {
            w->players[i].active = false;
        }
    }

    int shards = swieta_progress_count_shards(w->shard_mask);
    set_objective(w, w->portals[0].pos, "ENTER DAY 1 PORTAL (WEST CARPET)");
    char msg[128];
    snprintf(msg, sizeof(msg), "GRAND FESTIVE HALL | SHARDS: %d/24 | FOLLOW WEST CARPET TO DAY 1", shards);
    set_banner(w, msg, 5.5f);
}

/* ============================================================================
 * DAY 1 (Dec 1) — DUKE NUKEM 3D HOMAGE: "NEON STRIP '96" (128x24x128 4-Beat Map)
 * ============================================================================ */
static void build_day01_duke(GameWorld *w) {
    w->scene = SCENE_DAY01;
    w->active_day = 1;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->cinema_restored = false;
    w->secret_wall_blasted = false;
    w->relays_cleared = false;
    w->boss_phase = 1;
    w->boss_defeated = false;
    w->nuke_exit_ready = false;
    w->checkpoint_stage = 0;

    /* Ground Plane: Snow-lined urban boulevard */
    voxel_fill_box(&w->voxels, 2, 0, 2, 125, 0, 125, VOX_SNOW);
    voxel_fill_box(&w->voxels, 18, 0, 74, 42, 0, 122, VOX_ASPHALT);
    voxel_fill_box(&w->voxels, 29, 0, 76, 30, 0, 120, VOX_ROAD_STRIPE);
    voxel_fill_box(&w->voxels, 42, 0, 84, 66, 0, 106, VOX_ASPHALT);

    /* Outer City Perimeter Skyline */
    voxel_fill_box(&w->voxels, 2, 1, 2, 125, 18, 2, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 125, 125, 18, 125, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 2, 2, 18, 125, VOX_BRICK);
    voxel_fill_box(&w->voxels, 125, 1, 2, 125, 18, 125, VOX_BRICK);
    /* Distant skyline neon bands */
    voxel_fill_box(&w->voxels, 16, 15, 3, 40, 15, 3, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 78, 15, 3, 108, 15, 3, VOX_NEON_CYAN);

    /* ---- BEAT 1: SNOWY NEON BOULEVARD, PIXEL PAWNSHOP & ALLEY SECURITY KIOSK ---- */
    /* Spawn alley cover crates */
    voxel_fill_box(&w->voxels, 19, 1, 112, 21, 2, 114, VOX_WOOD);
    voxel_fill_box(&w->voxels, 36, 1, 106, 38, 2, 109, VOX_METAL);

    /* Pixel Pawnshop (x=6..18, z=88..104, y=1..6) with Breakable Glass storefront */
    voxel_fill_box(&w->voxels, 6, 1, 88, 18, 6, 104, VOX_BRICK);
    voxel_fill_box(&w->voxels, 7, 1, 89, 17, 5, 103, VOX_AIR);
    voxel_fill_box(&w->voxels, 18, 1, 93, 18, 3, 98, VOX_GLASS); /* Shoot glass for Shotgun! */
    voxel_fill_box(&w->voxels, 18, 5, 91, 18, 5, 100, VOX_NEON_CYAN);

    /* Alley Security Kiosk (x=38..52, z=88..104, y=1..6) holding BLUE KEYCARD */
    voxel_fill_box(&w->voxels, 38, 1, 88, 52, 6, 104, VOX_STONE);
    voxel_fill_box(&w->voxels, 39, 1, 89, 51, 5, 103, VOX_AIR);
    voxel_fill_box(&w->voxels, 38, 1, 94, 38, 3, 98, VOX_AIR); /* West doorway */
    voxel_fill_box(&w->voxels, 38, 5, 92, 38, 5, 100, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 44, 1, 95, 47, 1, 97, VOX_HAZARD); /* Keycard pedestal */

    /* ---- BEAT 2: YULE CINEMA, MIRROR SELF-SHRINK PUZZLE & SECRET RETRO ARCADE ---- */
    /* Main Yule Cinema shell (x=10..48, z=36..72, y=1..8) */
    voxel_fill_box(&w->voxels, 10, 1, 36, 48, 8, 72, VOX_BRICK);
    voxel_fill_box(&w->voxels, 11, 1, 37, 47, 7, 71, VOX_AIR);
    voxel_fill_box(&w->voxels, 11, 0, 37, 47, 0, 71, VOX_CARPET);

    /* Neon Marquee above front entrance */
    voxel_fill_box(&w->voxels, 20, 6, 73, 36, 6, 73, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 22, 5, 73, 34, 5, 73, VOX_NEON_CYAN);

    /* Door #0: Blue Keycard Entrance into Yule Cinema */
    add_door(w, 26, 1, 72, 30, 4, 72, KEY_BLUE, VOX_DOOR_BLUE);

    /* Auditorium Velvet Seats & Hijacked Static Screen */
    for (int sz = 43; sz <= 55; sz += 4) {
        voxel_fill_box(&w->voxels, 24, 1, sz, 32, 1, sz, VOX_SEAT);
        voxel_fill_box(&w->voxels, 36, 1, sz, 44, 1, sz, VOX_SEAT);
    }
    voxel_fill_box(&w->voxels, 22, 2, 37, 44, 6, 37, VOX_CINEMA_STATIC);
    /* Balcony overlook on east cinema wall */
    voxel_fill_box(&w->voxels, 42, 4, 58, 47, 4, 70, VOX_WOOD);

    /* Mirror Room (x=11..20, z=60..70) & Sealed Projection Booth (x=11..20, z=48..58) */
    voxel_fill_box(&w->voxels, 20, 1, 48, 20, 6, 70, VOX_STONE);
    voxel_fill_box(&w->voxels, 20, 1, 64, 20, 3, 67, VOX_AIR); /* Open entry into Mirror Room */
    voxel_fill_box(&w->voxels, 11, 1, 48, 20, 6, 48, VOX_STONE);
    /* Divider wall with 1-VOXEL-TALL MINIATURE AIR DUCT at x=15..16, y=1, z=59! */
    voxel_fill_box(&w->voxels, 11, 1, 59, 20, 6, 59, VOX_VENT);
    voxel_fill_box(&w->voxels, 15, 1, 59, 16, 1, 59, VOX_AIR); /* Shrunk player crawls right through! */
    /* Mirror wall on West side of Mirror Room (x=11, y=1..4, z=63..67) */
    voxel_fill_box(&w->voxels, 11, 1, 63, 11, 4, 67, VOX_MIRROR);

    /* Door #1: Projection Booth Security Exit (opens when Cinema Projector restored!) */
    add_door(w, 20, 1, 52, 20, 3, 54, 0u, VOX_DOOR_LOCKED);
    w->cinema_switch_pos = vec3_make(15.5f, 1.5f, 52.5f);
    voxel_set(&w->voxels, 15, 1, 52, VOX_GOLD);

    /* Secret #1: Cracked Backstage Wall -> Secret Retro Arcade (x=49..58, z=44..60) */
    voxel_fill_box(&w->voxels, 48, 1, 51, 48, 3, 54, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 49, 1, 44, 58, 6, 60, VOX_STONE);
    voxel_fill_box(&w->voxels, 49, 1, 45, 57, 5, 59, VOX_AIR);
    voxel_fill_box(&w->voxels, 49, 0, 45, 57, 0, 59, VOX_CARPET);
    voxel_set(&w->voxels, 56, 2, 48, VOX_NEON_CYAN);
    voxel_set(&w->voxels, 56, 2, 52, VOX_NEON_PINK);
    voxel_set(&w->voxels, 56, 2, 56, VOX_GOLD);
    add_secret(w, vec3_make(53.5f, 1.5f, 52.0f), 4.0f, "SECRET RETRO ARCADE");

    /* East Cinema Freight Exit Corridor -> Door #2 (opens when Cinema restored!) */
    voxel_fill_box(&w->voxels, 48, 1, 65, 58, 5, 71, VOX_METAL);
    voxel_fill_box(&w->voxels, 48, 1, 66, 58, 4, 70, VOX_AIR);
    add_door(w, 57, 1, 66, 57, 4, 70, 0u, VOX_DOOR_LOCKED);

    /* ---- BEAT 3: THREE CONNECTED ROOFTOPS & BROADCAST TOWER (y=8, y=10, y=12) ---- */
    /* Stepped Freight Ramp from Street (y=1) up to Roof A (y=8) along x=60..63, z=86..93 */
    for (int s = 0; s < 7; ++s) {
        voxel_fill_box(&w->voxels, 60, 1, 86 + s, 63, 1 + s, 86 + s, VOX_HAZARD);
    }

    /* Roof A: Staging Skyscraper (x=64..90, z=82..114, top surface y=8.0) */
    voxel_fill_box(&w->voxels, 64, 1, 82, 90, 7, 114, VOX_CONCRETE);
    voxel_fill_box(&w->voxels, 64, 8, 114, 90, 8, 114, VOX_METAL); /* Parapet */
    voxel_fill_box(&w->voxels, 64, 8, 82, 72, 8, 82, VOX_HAZARD);

    /* Backup narrow steel catwalk connecting Roof A -> Roof B on East edge (x=88..89) */
    voxel_fill_box(&w->voxels, 88, 7, 76, 89, 7, 81, VOX_METAL);
    voxel_fill_box(&w->voxels, 88, 8, 76, 89, 8, 78, VOX_HAZARD);

    /* Roof B: Billboard Deck (x=64..96, z=48..75, top surface y=10.0) */
    voxel_fill_box(&w->voxels, 64, 1, 48, 96, 9, 75, VOX_CONCRETE);
    /* Secret #2: Hidden Penthouse Closet beneath Breakable Glass Skylight (x=70..75, z=54..59) */
    voxel_fill_box(&w->voxels, 70, 6, 54, 76, 8, 60, VOX_AIR);
    voxel_fill_box(&w->voxels, 71, 9, 55, 74, 9, 58, VOX_GLASS); /* Break skylight glass! */
    voxel_set(&w->voxels, 74, 6, 58, VOX_HAZARD);                /* Walkable exit stairs */
    voxel_fill_box(&w->voxels, 74, 6, 57, 74, 7, 57, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 74, 6, 56, 74, 8, 56, VOX_HAZARD);
    voxel_set(&w->voxels, 70, 7, 56, VOX_NEON_CYAN);
    add_secret(w, vec3_make(72.5f, 7.0f, 57.0f), 3.5f, "SKYLIGHT SMUGGLER STASH");

    /* Glowing Neon Billboard on Roof B */
    voxel_fill_box(&w->voxels, 66, 10, 49, 78, 13, 49, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 68, 11, 50, 76, 12, 50, VOX_NEON_CYAN);
    /* Broadcast Relay #1 */
    add_relay(w, 84, 10, 60);

    /* Roof C: Broadcast Tower (x=100..122, z=38..78, top surface y=12.0) */
    voxel_fill_box(&w->voxels, 100, 1, 38, 122, 11, 78, VOX_CONCRETE);
    /* Step-bridge from Roof B (y=10) to Roof C (y=12) at z=62..65 */
    voxel_fill_box(&w->voxels, 97, 9, 62, 98, 9, 65, VOX_METAL);
    voxel_fill_box(&w->voxels, 99, 10, 62, 99, 10, 65, VOX_HAZARD);
    /* Broadcast Relay #2 */
    add_relay(w, 111, 12, 58);

    /* Secret #3: Cracked Tower Vent Cache on Roof C (x=117..121, z=40..44) */
    voxel_fill_box(&w->voxels, 116, 12, 39, 121, 15, 45, VOX_METAL);
    voxel_fill_box(&w->voxels, 117, 12, 40, 120, 14, 44, VOX_AIR);
    voxel_fill_box(&w->voxels, 116, 12, 41, 116, 13, 43, VOX_CRACKED);
    add_secret(w, vec3_make(118.5f, 12.5f, 42.0f), 3.0f, "BROADCAST TOWER VAULT");

    /* Red Keycard Vault Door #3 on Roof C (opens when both Broadcast Relays destroyed!) */
    voxel_fill_box(&w->voxels, 104, 12, 39, 112, 15, 46, VOX_STONE);
    voxel_fill_box(&w->voxels, 105, 12, 40, 111, 14, 45, VOX_AIR);
    add_door(w, 107, 12, 46, 109, 14, 46, 0u, VOX_DOOR_LOCKED);

    /* Neon Skybridge from Roof C (z=38) directly to South Face of Red Penthouse Gate (x=85..89, z=29) */
    voxel_fill_box(&w->voxels, 100, 11, 29, 106, 11, 38, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 11, 29, 106, 11, 34, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 11, 34, 106, 11, 34, VOX_NEON_CYAN);

    /* ---- BEAT 4: NORTH PENTHOUSE & 3-PHASE BATTLE-MECH ARENA (y=12.0) ---- */
    voxel_fill_box(&w->voxels, 24, 1, 6, 92, 11, 28, VOX_CONCRETE);
    /* Arena perimeter guardrails */
    voxel_fill_box(&w->voxels, 24, 12, 6, 92, 15, 6, VOX_METAL);
    voxel_fill_box(&w->voxels, 24, 12, 6, 24, 15, 28, VOX_METAL);
    voxel_fill_box(&w->voxels, 24, 12, 28, 84, 14, 28, VOX_METAL);
    /* Door #4: Red Keycard Penthouse Gate at x=85..89, y=12..15, z=28 */
    voxel_fill_box(&w->voxels, 92, 12, 6, 92, 15, 28, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 12, 28, 92, 15, 28, VOX_STONE);
    add_door(w, 85, 12, 28, 89, 15, 28, KEY_RED, VOX_DOOR_RED);

    /* 4 Destructible Cracked Cover Pillars inside Penthouse Boss Arena */
    voxel_fill_box(&w->voxels, 42, 12, 12, 43, 14, 13, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 42, 12, 21, 43, 14, 22, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 64, 12, 12, 65, 14, 13, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 64, 12, 21, 65, 14, 22, VOX_CRACKED);

    /* Crimson Nuke-Exit Pedestal at West end of Penthouse */
    w->nuke_switch_pos = vec3_make(30.5f, 12.5f, 17.0f);
    voxel_fill_box(&w->voxels, 29, 12, 16, 31, 12, 18, VOX_HAZARD);
    voxel_set(&w->voxels, 30, 13, 17, VOX_NEON_PINK);

    /* Return Portal in Spawn Alley */
    w->return_portal_pos = vec3_make(8.5f, 1.0f, 118.0f);

    /* ---- ENEMIES (4 Beats of Varied Classic Encounters) ---- */
    /* Beat 1: Boulevard & Kiosk */
    add_enemy(w, ENEMY_TROOPER,   vec3_make(24.0f, 1.0f, 102.0f));
    add_enemy(w, ENEMY_TROOPER,   vec3_make(34.0f, 1.0f, 94.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(45.0f, 1.0f, 99.0f));
    add_enemy(w, ENEMY_TROOPER,   vec3_make(28.0f, 1.0f, 82.0f));

    /* Beat 2: Yule Cinema */
    add_enemy(w, ENEMY_TROOPER,   vec3_make(30.0f, 1.0f, 64.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(34.0f, 1.0f, 50.0f));
    add_enemy(w, ENEMY_OCTABRAIN, vec3_make(44.0f, 5.2f, 62.0f));
    add_enemy(w, ENEMY_TROOPER,   vec3_make(22.0f, 1.0f, 44.0f));

    /* Beat 3: Rooftops A, B, C */
    add_enemy(w, ENEMY_TROOPER,   vec3_make(76.0f, 8.0f, 96.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(82.0f, 8.0f, 88.0f));
    add_enemy(w, ENEMY_TURRET,    vec3_make(79.0f, 10.0f, 66.0f));
    add_enemy(w, ENEMY_OCTABRAIN, vec3_make(88.0f, 12.2f, 56.0f));
    add_enemy(w, ENEMY_TURRET,    vec3_make(108.0f, 12.0f, 64.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(114.0f, 12.0f, 50.0f));

    /* Beat 4: Penthouse Boss (index 14!) */
    add_enemy(w, ENEMY_BOSS_MECH, vec3_make(50.0f, 12.0f, 17.0f));

    /* ---- PICKUPS & GEAR PROGRESSION ---- */
    add_pickup(w, PICKUP_SHOTGUN,     vec3_make(12.5f, 1.2f, 96.0f));  /* Behind Pawnshop Glass */
    add_pickup(w, PICKUP_MEDKIT,      vec3_make(12.5f, 1.2f, 92.0f));
    add_pickup(w, PICKUP_KEY_BLUE,    vec3_make(45.5f, 2.2f, 96.0f));  /* Alley Security Kiosk */
    add_pickup(w, PICKUP_SHRINK_AMMO, vec3_make(24.0f, 1.2f, 68.0f));  /* Cinema Lobby */
    add_pickup(w, PICKUP_PIPEBOMB,    vec3_make(42.0f, 1.2f, 54.0f));  /* Cinema East */
    add_pickup(w, PICKUP_ARMOR,       vec3_make(53.5f, 1.2f, 52.0f));  /* Secret #1 Arcade */
    add_pickup(w, PICKUP_PIPEBOMB,    vec3_make(53.5f, 1.2f, 48.0f));
    add_pickup(w, PICKUP_JETPACK,     vec3_make(70.0f, 8.2f, 102.0f)); /* Roof A */
    add_pickup(w, PICKUP_TRIPMINE,    vec3_make(74.0f, 8.2f, 92.0f));  /* Roof A */
    add_pickup(w, PICKUP_SHELLS,      vec3_make(73.0f, 6.2f, 57.0f));  /* Secret #2 Skylight */
    add_pickup(w, PICKUP_ARMOR,       vec3_make(71.5f, 6.2f, 57.0f));
    add_pickup(w, PICKUP_MEDKIT,      vec3_make(88.0f, 10.2f, 68.0f)); /* Roof B */
    add_pickup(w, PICKUP_KEY_RED,     vec3_make(108.0f, 12.2f, 42.5f));/* Roof C Vault */
    add_pickup(w, PICKUP_TRIPMINE,    vec3_make(118.5f, 12.2f, 42.0f));/* Secret #3 Tower */
    add_pickup(w, PICKUP_MEDKIT,      vec3_make(76.0f, 12.2f, 22.0f)); /* Penthouse Arena */
    add_pickup(w, PICKUP_SHELLS,      vec3_make(76.0f, 12.2f, 12.0f));
    add_pickup(w, PICKUP_SHRINK_AMMO, vec3_make(54.0f, 12.2f, 10.0f));
    add_pickup(w, PICKUP_ARMOR,       vec3_make(54.0f, 12.2f, 24.0f));

    w->checkpoint_pos = vec3_make(24.0f, 1.0f, 114.0f);
    w->checkpoint_yaw = 0.0f;
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(24.0f + (float)i * 1.2f, 1.0f, 114.0f);
            reset_player_for_spawn(&w->players[i], sp, 0.0f, false);
        } else {
            w->players[i].active = false;
        }
    }

    set_objective(w, vec3_make(45.5f, 1.5f, 96.0f), "GET BLUE KEYCARD IN KIOSK - GLASS PAWNSHOP HAS SHOTGUN");
    set_banner(w, "DAY 1: NEON STRIP 96 - GET BLUE KEYCARD, BREACH CINEMA!", 6.0f);
}

static void build_preview_chamber(GameWorld *w, int day) {
    w->scene = SCENE_PREVIEW;
    w->active_day = day;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    voxel_fill_box(&w->voxels, 48, 0, 48, 80, 0, 80, VOX_STONE);
    voxel_fill_box(&w->voxels, 48, 1, 48, 80, 9, 48, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 1, 80, 80, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 1, 48, 48, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 80, 1, 48, 80, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 10, 48, 80, 10, 80, VOX_WOOD);
    voxel_fill_box(&w->voxels, 63, 1, 58, 65, 2, 60, VOX_GOLD);

    w->return_portal_pos = vec3_make(64.0f, 1.0f, 75.0f);
    add_pickup(w, PICKUP_SHARD, vec3_make(64.5f, 3.0f, 59.5f));
    w->checkpoint_pos = vec3_make(64.0f, 1.0f, 70.0f);
    w->checkpoint_yaw = 0.0f;

    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(63.0f + (float)i * 1.1f, 1.0f, 70.0f);
            reset_player_for_spawn(&w->players[i], sp, 0.0f, false);
        } else {
            w->players[i].active = false;
        }
    }

    const PortalMeta *meta = swieta_portal_meta(day);
    set_objective(w, vec3_make(64.5f, 3.0f, 59.5f), "COLLECT PREVIEW SHARD OR RETURN TO GRAND HALL");
    char msg[128];
    snprintf(msg, sizeof(msg), "DAY %02d PREVIEW: %s (%s) | FULL LEVEL COMING IN PHASES 2-5",
             day, meta ? meta->portal_title : "?", meta ? meta->inspiration : "?");
    set_banner(w, msg, 6.0f);
}

void world_load_day(GameWorld *w, int day) {
    if (!w || day < 1 || day > SWIETA_PORTAL_COUNT) return;
    if (!swieta_portal_is_unlocked(day, w->now, w->unlock_all)) {
        set_banner(w, "PORTAL FROZEN BY KRAMPUS.EXE! WAIT FOR ADVENT DAY OR ENABLE UNLOCK-ALL", 3.5f);
        return;
    }
    w->sound_events |= SFX_PORTAL;
    if (day == 1) {
        build_day01_duke(w);
    } else {
        build_preview_chamber(w, day);
    }
}

void world_init(GameWorld *w, SwietaDateTime now, bool unlock_all, int player_count) {
    if (!w) return;
    memset(w, 0, sizeof(*w));
    w->now = now;
    w->unlock_all = unlock_all;
    w->player_count = (player_count < 1) ? 1 : (player_count > WORLD_MAX_PLAYERS ? WORLD_MAX_PLAYERS : player_count);
    world_load_hub(w);
}

void world_set_player_count(GameWorld *w, int count) {
    if (!w) return;
    if (count < 1) count = 1;
    if (count > WORLD_MAX_PLAYERS) count = WORLD_MAX_PLAYERS;
    w->player_count = count;
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < count && !w->players[i].active) {
            Vec3 base = w->players[0].pos;
            reset_player_for_spawn(&w->players[i], vec3_make(base.x + (float)i * 0.9f, base.y, base.z), w->players[0].yaw, false);
        } else if (i >= count) {
            w->players[i].active = false;
        }
    }
}

static bool player_collides_h(const VoxelWorld *vw, Vec3 pos, float height) {
    int x0 = (int)floorf(pos.x - kPlayerRadius);
    int x1 = (int)floorf(pos.x + kPlayerRadius);
    int y0 = (int)floorf(pos.y + 0.001f);
    int y1 = (int)floorf(pos.y + height - 0.001f);
    int z0 = (int)floorf(pos.z - kPlayerRadius);
    int z1 = (int)floorf(pos.z + kPlayerRadius);

    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                if (voxel_is_solid(vw, x, y, z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void damage_player(GameWorld *w, Player *p, int dmg) {
    if (dmg <= 0 || p->hp <= 0) return;
    int absorbed = dmg / 2;
    if (absorbed > p->armor) absorbed = p->armor;
    p->armor -= absorbed;
    p->hp -= (dmg - absorbed);
    p->damage_flash = 0.35f;
    w->sound_events |= SFX_HURT;
    if (p->hp <= 0) {
        p->hp = 0;
        p->respawn_timer = 2.0f;
    }
}

static void damage_relay(GameWorld *w, RelayTarget *rt, Player *attacker, int dmg) {
    if (!rt || !rt->active || rt->destroyed || dmg <= 0) return;
    rt->hp -= dmg;
    if (attacker) attacker->hitmarker_timer = 0.16f;
    if (rt->hp <= 0) {
        rt->destroyed = true;
        voxel_fill_box(&w->voxels, rt->vx, rt->vy, rt->vz, rt->vx, rt->vy + 2, rt->vz, VOX_AIR);
        spawn_explosion(w, rt->pos, 3.2f, 1.0f, 0.35f, 0.25f);
        w->sound_events |= SFX_EXPLODE;
        if (attacker) attacker->score += 300;

        int remaining = 0;
        for (int i = 0; i < w->relay_count; ++i) {
            if (w->relays[i].active && !w->relays[i].destroyed) remaining++;
        }
        if (remaining == 0 && !w->relays_cleared) {
            w->relays_cleared = true;
            if (w->door_count > 3) open_door(w, &w->doors[3]); /* Open Red Keycard Vault! */
            w->checkpoint_pos = vec3_make(106.0f, 12.0f, 48.0f);
            w->checkpoint_stage = 3;
            set_objective(w, vec3_make(108.0f, 12.2f, 42.5f), "GRAB RED KEYCARD ON ROOF C -> CROSS TO PENTHOUSE");
            set_banner(w, "ALL BROADCAST RELAYS DESTROYED! RED KEYCARD VAULT OPENED! [CHECKPOINT 3]", 5.5f);
        } else {
            set_banner(w, "BROADCAST RELAY SHATTERED! DESTROY REMAINING RELAY ON ROOF C!", 4.0f);
            if (w->relay_count > 1 && !w->relays[1].destroyed) {
                set_objective(w, w->relays[1].pos, "JETPACK TO ROOF C -> DESTROY RELAY #2");
            }
        }
    }
}

static void damage_enemy(GameWorld *w, Enemy *e, Player *attacker, int dmg) {
    if (!e->active || dmg <= 0) return;
    e->hp -= dmg;
    e->hit_flash = 0.18f;
    if (attacker) attacker->hitmarker_timer = 0.14f;

    /* 3-Phase Boss Transitions for KRAMPUS-MECH MK.IV */
    if (e->kind == ENEMY_BOSS_MECH && e->hp > 0) {
        if (w->boss_phase == 1 && e->hp <= (e->max_hp * 2) / 3) {
            w->boss_phase = 2;
            add_enemy(w, ENEMY_TROOPER, vec3_make(46.0f, 12.0f, 12.0f));
            add_enemy(w, ENEMY_TROOPER, vec3_make(46.0f, 12.0f, 22.0f));
            set_banner(w, "KRAMPUS-MECH PHASE 2: ALIEN ESCORT DEPLOYED! SHRINK & STOMP THEM!", 4.5f);
        } else if (w->boss_phase == 2 && e->hp <= e->max_hp / 3) {
            w->boss_phase = 3;
            set_banner(w, "KRAMPUS-MECH PHASE 3: OVERDRIVE BARRAGE! FINISH IT!", 4.5f);
        }
    }

    if (e->hp <= 0) {
        e->active = false;
        if (attacker) {
            attacker->score += (e->kind == ENEMY_BOSS_MECH) ? 1000 : 120;
        }
        spawn_explosion(w, vec3_add(e->pos, vec3_make(0, 0.9f, 0)), 1.8f, 1.0f, 0.55f, 0.15f);
        if (e->kind == ENEMY_BOSS_MECH) {
            w->boss_defeated = true;
            w->sound_events |= SFX_EXPLODE;
            set_objective(w, w->nuke_switch_pos, "PRESS USE (E) ON RED NUKE-EXIT BUTTON!");
            set_banner(w, "KRAMPUS-MECH MK.IV DESTROYED! PUNCH THE RED NUKE-EXIT BUTTON!", 6.0f);
        }
    }
}

static void explode_at(GameWorld *w, Vec3 pos, float radius, int max_dmg, int owner_player) {
    spawn_explosion(w, pos, radius, 1.0f, 0.48f, 0.12f);
    w->sound_events |= SFX_EXPLODE;

    int destroyed = voxel_explode_cracked(&w->voxels, pos, radius * 0.9f);
    if (destroyed > 0 && !w->secret_wall_blasted && vec3_dist(pos, vec3_make(48.0f, 2.0f, 52.5f)) < 6.0f) {
        w->secret_wall_blasted = true;
        if (owner_player >= 0) w->players[owner_player].score += 250;
        set_banner(w, "CRACKED WALL BLASTED! SECRET RETRO ARCADE UNCOVERED! (+250)", 4.5f);
    }

    Player *att = (owner_player >= 0 && owner_player < WORLD_MAX_PLAYERS) ? &w->players[owner_player] : NULL;
    for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
        Enemy *e = &w->enemies[ei];
        if (!e->active) continue;
        float d = vec3_dist(pos, vec3_add(e->pos, vec3_make(0.0f, 0.9f, 0.0f)));
        if (d < radius * 1.2f) {
            int dmg = (int)((float)max_dmg * (1.0f - d / (radius * 1.3f)));
            if (dmg > 0) damage_enemy(w, e, att, dmg);
        }
    }
    for (int ri = 0; ri < w->relay_count; ++ri) {
        if (w->relays[ri].active && !w->relays[ri].destroyed &&
            vec3_dist(pos, w->relays[ri].pos) < radius * 1.25f) {
            damage_relay(w, &w->relays[ri], att, max_dmg);
        }
    }
}

static void fire_hitscan_ray(GameWorld *w, Player *p, Vec3 eye, Vec3 dir, int dmg) {
    VoxelRayHit wh;
    float max_t = 48.0f;
    if (voxel_raycast_ext(&w->voxels, eye, dir, max_t, &wh)) {
        max_t = wh.dist;
        if (wh.type == VOX_GLASS) {
            voxel_break_glass(&w->voxels, wh.point, 1.8f);
            w->sound_events |= SFX_GLASS;
        } else if (wh.type == VOX_RELAY_CORE) {
            for (int ri = 0; ri < w->relay_count; ++ri) {
                if (w->relays[ri].active && !w->relays[ri].destroyed &&
                    w->relays[ri].vx == wh.vx && w->relays[ri].vz == wh.vz) {
                    damage_relay(w, &w->relays[ri], p, dmg);
                }
            }
        }
    }

    int best_enemy = -1;
    float best_t = max_t;
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float rad = (e->kind == ENEMY_BOSS_MECH) ? 1.25f : (e->shrunk_timer > 0.0f ? 0.35f : 0.68f);
        float h = (e->kind == ENEMY_BOSS_MECH) ? 2.6f : (e->shrunk_timer > 0.0f ? 0.6f : 1.78f);
        AABB box = {
            vec3_make(e->pos.x - rad, e->pos.y, e->pos.z - rad),
            vec3_make(e->pos.x + rad, e->pos.y + h, e->pos.z + rad)
        };
        float t = 0.0f;
        if (ray_aabb_hit(eye, dir, box, best_t, &t)) {
            best_t = t;
            best_enemy = i;
        }
    }
    if (best_enemy >= 0) {
        damage_enemy(w, &w->enemies[best_enemy], p, dmg);
    }
}

static void fire_player_weapon(GameWorld *w, int pi, Player *p) {
    Vec3 eye = player_eye_pos(p);
    Vec3 fwd = player_forward(p);

    if (p->weapon == WEAP_PISTOL) {
        p->fire_cooldown = 0.20f;
        p->muzzle_flash = 0.10f;
        w->sound_events |= SFX_PISTOL;
        fire_hitscan_ray(w, p, eye, fwd, 24);
    } else if (p->weapon == WEAP_SHOTGUN) {
        if (!p->has_shotgun || p->ammo_shells <= 0) {
            p->weapon = WEAP_PISTOL;
            return;
        }
        p->ammo_shells--;
        p->fire_cooldown = 0.58f;
        p->muzzle_flash = 0.16f;
        w->sound_events |= SFX_SHOTGUN;
        Vec3 right = vec3_norm(vec3_cross(fwd, vec3_make(0, 1, 0)));
        Vec3 up = vec3_cross(right, fwd);
        static const float sp[7][2] = {
            {0,0}, {-0.045f,0.01f}, {0.045f,0.01f}, {-0.025f,-0.03f}, {0.025f,-0.03f}, {-0.06f,0.03f}, {0.06f,0.03f}
        };
        for (int k = 0; k < 7; ++k) {
            Vec3 d = vec3_norm(vec3_add(fwd, vec3_add(vec3_scale(right, sp[k][0]), vec3_scale(up, sp[k][1]))));
            fire_hitscan_ray(w, p, eye, d, 14);
        }
    } else if (p->weapon == WEAP_SHRINKER && p->ammo_shrink > 0) {
        p->ammo_shrink--;
        p->fire_cooldown = 0.42f;
        p->muzzle_flash = 0.14f;
        w->sound_events |= SFX_SHRINK;
        for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
            if (!w->projectiles[i].active) {
                Projectile *pr = &w->projectiles[i];
                memset(pr, 0, sizeof(*pr));
                pr->active = true;
                pr->kind = PROJ_SHRINK_ORB;
                pr->owner_player = pi;
                pr->pos = vec3_add(eye, vec3_scale(fwd, 0.7f));
                pr->vel = vec3_scale(fwd, 19.0f);
                pr->life = 3.0f;
                break;
            }
        }
    } else if (p->weapon == WEAP_PIPEBOMB && p->ammo_pipebomb > 0) {
        p->ammo_pipebomb--;
        p->fire_cooldown = 0.45f;
        w->sound_events |= SFX_THROW;
        for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
            if (!w->projectiles[i].active) {
                Projectile *pr = &w->projectiles[i];
                memset(pr, 0, sizeof(*pr));
                pr->active = true;
                pr->kind = PROJ_PIPEBOMB;
                pr->owner_player = pi;
                pr->pos = vec3_add(eye, vec3_scale(fwd, 0.6f));
                pr->vel = vec3_add(vec3_scale(fwd, 11.5f), vec3_make(0.0f, 3.6f, 0.0f));
                pr->life = 35.0f;
                break;
            }
        }
    } else if (p->weapon == WEAP_TRIPMINE && p->ammo_tripmine > 0) {
        VoxelRayHit wh;
        if (voxel_raycast_ext(&w->voxels, eye, fwd, 4.5f, &wh) && wh.type != VOX_GLASS) {
            for (int i = 0; i < WORLD_MAX_TRIPMINES; ++i) {
                if (!w->tripmines[i].active) {
                    p->ammo_tripmine--;
                    p->fire_cooldown = 0.45f;
                    w->sound_events |= SFX_SWITCH;
                    Tripmine *tm = &w->tripmines[i];
                    tm->active = true;
                    tm->owner_player = pi;
                    tm->pos = vec3_add(wh.point, vec3_scale(wh.normal, 0.12f));
                    tm->normal = wh.normal;
                    tm->arm_timer = 0.40f;
                    VoxelRayHit opp;
                    if (voxel_raycast_ext(&w->voxels, vec3_add(tm->pos, vec3_scale(tm->normal, 0.2f)), tm->normal, 18.0f, &opp)) {
                        tm->beam_end = opp.point;
                    } else {
                        tm->beam_end = vec3_add(tm->pos, vec3_scale(tm->normal, 18.0f));
                    }
                    set_banner(w, "LASER TRIPMINE PLANTED! BEAM ARMED!", 2.5f);
                    break;
                }
            }
        }
    }
}

static void detonate_pipebombs(GameWorld *w, int owner_player) {
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        Projectile *pr = &w->projectiles[i];
        if (!pr->active || pr->kind != PROJ_PIPEBOMB || pr->owner_player != owner_player) {
            continue;
        }
        pr->active = false;
        explode_at(w, pr->pos, 3.8f, 110, owner_player);
    }
}

static void try_open_nearby_doors(GameWorld *w, Player *p) {
    for (int i = 0; i < w->door_count; ++i) {
        DoorGate *d = &w->doors[i];
        if (!d->active || d->open || d->required_key == 0u) continue;
        if (vec3_dist(p->pos, d->center) < 3.4f) {
            if ((p->keys & d->required_key) != 0u) {
                open_door(w, d);
                if (d->required_key == KEY_BLUE) {
                    set_objective(w, vec3_make(13.0f, 1.5f, 65.0f), "ENTER MIRROR ROOM -> SELF-SHRINK -> CRAWL VENT TO PROJECTOR");
                    set_banner(w, "BLUE KEYCARD ACCEPTED! WELCOME TO YULE CINEMA!", 4.5f);
                } else if (d->required_key == KEY_RED) {
                    w->checkpoint_pos = vec3_make(78.0f, 12.0f, 22.0f);
                    w->checkpoint_stage = 4;
                    set_objective(w, vec3_make(50.0f, 12.5f, 17.0f), "DEFEAT KRAMPUS-MECH MK.IV ON THE PENTHOUSE!");
                    set_banner(w, "RED KEYCARD ACCEPTED! PENTHOUSE BOSS ARENA! [CHECKPOINT 4]", 5.0f);
                }
            } else {
                set_banner(w, d->required_key == KEY_BLUE ? "LOCKED! REQUIRES BLUE KEYCARD!" : "LOCKED! REQUIRES RED KEYCARD!", 2.5f);
            }
        }
    }
}

static void handle_player_use(GameWorld *w, Player *p) {
    if (w->scene == SCENE_HUB) {
        if (vec3_dist(p->pos, w->santa_bios_pos) < 3.8f) {
            w->sound_events |= SFX_SWITCH;
            set_banner(w, kSantaLogs[w->santa_log_index % 6], 5.0f);
            w->santa_log_index = (w->santa_log_index + 1) % 6;
        }
    } else if (w->scene == SCENE_DAY01) {
        try_open_nearby_doors(w, p);
        if (!w->cinema_restored && vec3_dist(p->pos, w->cinema_switch_pos) < 3.2f) {
            w->cinema_restored = true;
            voxel_replace_all(&w->voxels, VOX_CINEMA_STATIC, VOX_CINEMA_LIT);
            if (w->door_count > 1) open_door(w, &w->doors[1]); /* Projection Booth exit */
            if (w->door_count > 2) open_door(w, &w->doors[2]); /* East Freight Exit to Rooftops */
            w->checkpoint_pos = vec3_make(44.0f, 1.0f, 66.0f);
            w->checkpoint_stage = 2;
            p->score += 350;
            w->sound_events |= SFX_SWITCH;
            set_objective(w, w->relays[0].pos, "TAKE EAST FREIGHT RAMP TO ROOFTOPS -> DESTROY 2 RELAYS");
            set_banner(w, "YULE CINEMA RESTORED! EAST FREIGHT EXIT OPEN! [CHECKPOINT 2]", 5.5f);
        } else if (vec3_dist(p->pos, w->nuke_switch_pos) < 3.4f) {
            if (!w->cinema_restored || !w->boss_defeated) {
                set_banner(w, "RESTORE CINEMA PROJECTOR & DEFEAT KRAMPUS-MECH FIRST!", 3.5f);
            } else {
                w->nuke_exit_ready = true;
                w->shard_mask |= (1u << 0);
                p->score += 1500;
                w->sound_events |= SFX_SHARD;
                world_load_hub(w);
                set_banner(w, "DAY 1 COMPLETE! GOLDEN SHADES SHARD LIT ON THE GRAND TREE!", 6.5f);
            }
        }
    }
}

const PortalGate *world_find_nearby_portal(const GameWorld *w, Vec3 pos, float max_dist) {
    if (!w || w->scene != SCENE_HUB) return NULL;
    const PortalGate *best = NULL;
    float best_d = max_dist;
    for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
        float d = vec3_dist(pos, w->portals[i].pos);
        if (d < best_d) {
            best_d = d;
            best = &w->portals[i];
        }
    }
    return best;
}

static void step_player(GameWorld *w, int pi, const PlayerInput *inp, float dt) {
    Player *p = &w->players[pi];
    if (!p->active) return;

    if (p->hp <= 0) {
        p->respawn_timer -= dt;
        if (p->respawn_timer <= 0.0f) {
            reset_player_for_spawn(p, w->checkpoint_pos, w->checkpoint_yaw, true);
            set_banner(w, "RESPAWNED AT LAST CHECKPOINT! GET BACK IN THE FIGHT!", 3.0f);
        }
        return;
    }

    p->yaw = sw_wrap_angle(p->yaw + inp->look_dx);
    p->pitch = sw_clampf(p->pitch - inp->look_dy, -1.35f, 1.35f);

    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;
    if (p->holo_cooldown > 0.0f) p->holo_cooldown -= dt;
    if (p->hitmarker_timer > 0.0f) p->hitmarker_timer -= dt;
    if (p->stomp_flash > 0.0f) p->stomp_flash -= dt;
    if (p->damage_flash > 0.0f) p->damage_flash -= dt;
    if (p->muzzle_flash > 0.0f) p->muzzle_flash -= dt;

    /* Safe un-shrink: never stand up inside a 1-voxel miniature air duct! */
    if (p->self_shrunk_timer > 0.0f) {
        p->self_shrunk_timer -= dt;
        if (p->self_shrunk_timer <= 0.0f) {
            if (player_collides_h(&w->voxels, p->pos, kPlayerTallHeight)) {
                p->self_shrunk_timer = 0.25f; /* Keep shrunk until clear of vent ceiling */
            } else {
                p->self_shrunk_timer = 0.0f;
                set_banner(w, "RETURNED TO FULL SIZE!", 2.0f);
            }
        }
    }

    uint16_t pressed = (uint16_t)(inp->buttons & (uint16_t)(~p->prev_buttons));
    p->prev_buttons = inp->buttons;

    if (inp->select_weapon >= 0 && inp->select_weapon < WEAP_COUNT) {
        if (inp->select_weapon != WEAP_SHOTGUN || p->has_shotgun) {
            p->weapon = (WeaponId)inp->select_weapon;
        }
    } else if (pressed & INP_NEXT_WEAP) {
        WeaponId next = (WeaponId)((p->weapon + 1) % WEAP_COUNT);
        if (next == WEAP_SHOTGUN && !p->has_shotgun) next = WEAP_SHRINKER;
        p->weapon = next;
    }

    /* Movement in XZ */
    Vec3 fwd_xz = vec3_make(sinf(p->yaw), 0.0f, -cosf(p->yaw));
    Vec3 right_xz = vec3_make(cosf(p->yaw), 0.0f, sinf(p->yaw));
    float speed = (inp->buttons & INP_SPRINT) ? 9.5f : 6.6f;
    if (p->self_shrunk_timer > 0.0f) speed *= 0.8f;
    Vec3 wish = vec3_add(vec3_scale(fwd_xz, sw_clampf(inp->move_y, -1.0f, 1.0f)),
                         vec3_scale(right_xz, sw_clampf(inp->move_x, -1.0f, 1.0f)));
    if (vec3_len_sq(wish) > 1.0f) wish = vec3_norm(wish);
    p->vel.x = wish.x * speed;
    p->vel.z = wish.z * speed;

    if (p->on_ground && (fabsf(p->vel.x) + fabsf(p->vel.z)) > 0.5f) {
        p->walk_bob += dt * 10.5f;
    }

    /* Jump & Jetpack */
    p->jetpack_active = false;
    if ((pressed & INP_JUMP) && p->on_ground) {
        p->vel.y = 8.4f;
        p->on_ground = false;
    } else if (((inp->buttons & INP_JETPACK) || ((inp->buttons & INP_JUMP) && !p->on_ground && p->vel.y < 1.0f)) &&
               p->jetpack_fuel > 1.0f && w->scene == SCENE_DAY01) {
        p->vel.y = sw_lerpf(p->vel.y, 6.8f, 0.28f);
        p->jetpack_fuel = sw_clampf(p->jetpack_fuel - 22.0f * dt, 0.0f, 100.0f);
        p->jetpack_active = true;
        w->sound_events |= SFX_JETPACK;
    } else {
        p->vel.y -= kGravity * dt;
        if (p->on_ground) {
            p->jetpack_fuel = sw_clampf(p->jetpack_fuel + 15.0f * dt, 0.0f, 100.0f);
        }
    }

    float ph = player_height(p);

    /* Axis-separated voxel collision + clean auto step-up */
    if (fabsf(p->vel.x) > 1e-5f) {
        Vec3 try_x = vec3_make(p->pos.x + p->vel.x * dt, p->pos.y, p->pos.z);
        if (!player_collides_h(&w->voxels, try_x, ph)) {
            p->pos.x = try_x.x;
        } else if (p->on_ground && !player_collides_h(&w->voxels, vec3_make(try_x.x, p->pos.y + 1.01f, p->pos.z), ph)) {
            p->pos.x = try_x.x;
            p->pos.y = floorf(p->pos.y + 1.01f);
        }
    }

    if (fabsf(p->vel.z) > 1e-5f) {
        Vec3 try_z = vec3_make(p->pos.x, p->pos.y, p->pos.z + p->vel.z * dt);
        if (!player_collides_h(&w->voxels, try_z, ph)) {
            p->pos.z = try_z.z;
        } else if (p->on_ground && !player_collides_h(&w->voxels, vec3_make(p->pos.x, p->pos.y + 1.01f, try_z.z), ph)) {
            p->pos.z = try_z.z;
            p->pos.y = floorf(p->pos.y + 1.01f);
        }
    }

    Vec3 try_y = vec3_make(p->pos.x, p->pos.y + p->vel.y * dt, p->pos.z);
    if (!player_collides_h(&w->voxels, try_y, ph)) {
        p->pos.y = try_y.y;
        p->on_ground = false;
    } else {
        if (p->vel.y < 0.0f) {
            int hit_y = (int)floorf(try_y.y + 0.001f);
            p->pos.y = (float)(hit_y + 1);
            p->on_ground = true;
        } else if (p->vel.y > 0.0f) {
            int hit_y = (int)floorf(try_y.y + ph - 0.001f);
            p->pos.y = (float)hit_y - ph;
        }
        p->vel.y = 0.0f;
    }

    /* Auto-open keycard doors when walking up with matching key */
    if (w->scene == SCENE_DAY01) {
        for (int i = 0; i < w->door_count; ++i) {
            DoorGate *d = &w->doors[i];
            if (d->active && !d->open && d->required_key != 0u &&
                (p->keys & d->required_key) != 0u &&
                vec3_dist(p->pos, d->center) < 2.8f) {
                try_open_nearby_doors(w, p);
            }
        }
    }

    /* Weapons & gadgets */
    if ((inp->buttons & INP_FIRE) && p->fire_cooldown <= 0.0f) {
        fire_player_weapon(w, pi, p);
    }
    if (pressed & INP_ALT) {
        detonate_pipebombs(w, pi);
    }
    if ((pressed & INP_HOLO) && p->holo_cooldown <= 0.0f && w->scene == SCENE_DAY01) {
        w->decoy.active = true;
        w->decoy.owner_player = pi;
        w->decoy.pos = vec3_add(p->pos, vec3_scale(fwd_xz, 1.8f));
        w->decoy.timer = 9.0f;
        p->holo_cooldown = 10.0f;
        w->sound_events |= SFX_SWITCH;
        set_banner(w, "HOLO-DECOY DEPLOYED! TURRETS & ALIENS DISTRACTED!", 2.5f);
    }
    if (pressed & INP_USE) {
        handle_player_use(w, p);
    }

    /* Mighty Boot Stomp on shrunk enemies */
    for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
        Enemy *e = &w->enemies[ei];
        if (!e->active || e->shrunk_timer <= 0.0f) continue;
        if (vec3_dist(p->pos, e->pos) < 1.35f) {
            e->active = false;
            p->stomps++;
            p->score += 200;
            p->hp = (p->hp + 12 > p->max_hp) ? p->max_hp : p->hp + 12;
            p->stomp_flash = 0.32f;
            w->sound_events |= SFX_STOMP;
            set_banner(w, "MIGHTY BOOT STOMP! SQUISHED! (+200, +12 HP)", 2.2f);
        }
    }

    /* Discoverable Secret Zones */
    for (int si = 0; si < w->secret_count; ++si) {
        SecretZone *sz = &w->secrets[si];
        if (sz->active && !sz->found && vec3_dist(p->pos, sz->pos) < sz->radius) {
            sz->found = true;
            w->secrets_found++;
            p->score += 300;
            w->sound_events |= SFX_SECRET;
            char sb[128];
            snprintf(sb, sizeof(sb), "SECRET %d/%d FOUND: %s! (+300)", w->secrets_found, w->secret_count, sz->name);
            set_banner(w, sb, 4.0f);
        }
    }

    /* Pickups */
    for (int ki = 0; ki < WORLD_MAX_PICKUPS; ++ki) {
        Pickup *pk = &w->pickups[ki];
        if (!pk->active) continue;
        if (vec3_dist(vec3_add(p->pos, vec3_make(0, ph * 0.5f, 0)), pk->pos) < 1.55f) {
            pk->active = false;
            w->sound_events |= SFX_PICKUP;
            switch (pk->kind) {
                case PICKUP_MEDKIT:      p->hp = (p->hp + 35 > p->max_hp) ? p->max_hp : p->hp + 35; break;
                case PICKUP_ARMOR:       p->armor = (p->armor + 50 > 100) ? 100 : p->armor + 50; break;
                case PICKUP_SHOTGUN:
                    p->has_shotgun = true;
                    p->ammo_shells += 16;
                    p->weapon = WEAP_SHOTGUN;
                    set_banner(w, "SHOTGUN ACQUIRED! (KEY 2)", 3.0f);
                    break;
                case PICKUP_SHELLS:      p->ammo_shells += 12; break;
                case PICKUP_SHRINK_AMMO: p->ammo_shrink += 8; break;
                case PICKUP_PIPEBOMB:    p->ammo_pipebomb += 4; break;
                case PICKUP_TRIPMINE:    p->ammo_tripmine += 4; break;
                case PICKUP_JETPACK:     p->jetpack_fuel = 100.0f; break;
                case PICKUP_KEY_BLUE:
                    for (int k = 0; k < w->player_count; ++k) w->players[k].keys |= KEY_BLUE;
                    w->checkpoint_pos = vec3_make(30.0f, 1.0f, 82.0f);
                    w->checkpoint_stage = 1;
                    set_objective(w, vec3_make(28.0f, 1.5f, 72.0f), "UNLOCK YULE CINEMA BLUE DOOR (NORTH)");
                    set_banner(w, "BLUE ACCESS CARD COLLECTED! [CHECKPOINT 1] BREACH YULE CINEMA!", 5.0f);
                    break;
                case PICKUP_KEY_RED:
                    for (int k = 0; k < w->player_count; ++k) w->players[k].keys |= KEY_RED;
                    set_objective(w, vec3_make(87.0f, 12.5f, 28.0f), "CROSS NEON SKYBRIDGE -> UNLOCK RED PENTHOUSE GATE");
                    set_banner(w, "RED ACCESS CARD COLLECTED! CROSS THE SKYBRIDGE TO THE PENTHOUSE!", 5.0f);
                    break;
                case PICKUP_SHARD:
                    if (w->active_day >= 1 && w->active_day <= SWIETA_PORTAL_COUNT) {
                        w->shard_mask |= (1u << (unsigned)(w->active_day - 1));
                        w->sound_events |= SFX_SHARD;
                        set_banner(w, "PREVIEW MEMORY SHARD COLLECTED!", 3.0f);
                    }
                    break;
            }
        }
    }

    /* Portal transitions (Hub <-> Day) */
    if (w->scene == SCENE_HUB) {
        const PortalGate *gate = world_find_nearby_portal(w, p->pos, 1.65f);
        if (gate) {
            world_load_day(w, gate->day);
        }
    } else if (vec3_dist(p->pos, w->return_portal_pos) < 1.6f) {
        w->sound_events |= SFX_PORTAL;
        world_load_hub(w);
    }
}

static void step_tripmines(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_TRIPMINES; ++i) {
        Tripmine *tm = &w->tripmines[i];
        if (!tm->active) continue;
        if (tm->arm_timer > 0.0f) {
            tm->arm_timer -= dt;
            continue;
        }
        Vec3 ab = vec3_sub(tm->beam_end, tm->pos);
        float len_sq = vec3_len_sq(ab);
        if (len_sq < 1e-4f) continue;

        for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
            Enemy *e = &w->enemies[ei];
            if (!e->active) continue;
            Vec3 ep = vec3_add(e->pos, vec3_make(0, 0.9f, 0));
            float t = sw_clampf(vec3_dot(vec3_sub(ep, tm->pos), ab) / len_sq, 0.0f, 1.0f);
            Vec3 closest = vec3_add(tm->pos, vec3_scale(ab, t));
            if (vec3_dist(ep, closest) < 0.95f) {
                tm->active = false;
                explode_at(w, closest, 3.6f, 120, tm->owner_player);
                set_banner(w, "LASER TRIPMINE TRIGGERED!", 2.0f);
                break;
            }
        }
    }
}

static void step_projectiles(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        Projectile *pr = &w->projectiles[i];
        if (!pr->active) continue;
        pr->life -= dt;
        if (pr->life <= 0.0f) {
            pr->active = false;
            continue;
        }
        if (pr->kind == PROJ_PIPEBOMB) {
            pr->vel.y -= kGravity * 0.75f * dt;
            Vec3 next = vec3_add(pr->pos, vec3_scale(pr->vel, dt));
            if (voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y), (int)floorf(next.z))) {
                pr->vel = vec3_scale(pr->vel, 0.35f);
                pr->vel.y = fabsf(pr->vel.y) * 0.4f;
                if (vec3_len_sq(pr->vel) < 0.25f) pr->vel = vec3_make(0, 0, 0);
            } else {
                pr->pos = next;
            }
            continue;
        }

        Vec3 next = vec3_add(pr->pos, vec3_scale(pr->vel, dt));
        int nvx = (int)floorf(next.x);
        int nvy = (int)floorf(next.y);
        int nvz = (int)floorf(next.z);
        uint8_t hit_type = voxel_get(&w->voxels, nvx, nvy, nvz);

        if (hit_type != VOX_AIR && hit_type != VOX_FIRE) {
            /* Signature Duke 3D Mirror Self-Shrink reflection! */
            if (pr->kind == PROJ_SHRINK_ORB && hit_type == VOX_MIRROR && !pr->reflected) {
                pr->reflected = true;
                pr->vel = vec3_scale(pr->vel, -1.0f);
                w->sound_events |= SFX_SHRINK;
                continue;
            }
            if (hit_type == VOX_GLASS) {
                voxel_break_glass(&w->voxels, next, 1.8f);
                w->sound_events |= SFX_GLASS;
            }
            if (pr->kind == PROJ_BOSS_ROCKET) {
                spawn_explosion(w, pr->pos, 2.2f, 1.0f, 0.3f, 0.1f);
            }
            pr->active = false;
            continue;
        }
        pr->pos = next;

        if (pr->kind == PROJ_SHRINK_ORB) {
            /* Reflected Shrink-Orb shrinks the player for miniature air-duct traversal! */
            if (pr->reflected) {
                for (int pi = 0; pi < w->player_count; ++pi) {
                    Player *p = &w->players[pi];
                    if (!p->active || p->hp <= 0) continue;
                    if (vec3_dist(pr->pos, vec3_add(p->pos, vec3_make(0, 1.0f, 0))) < 1.15f) {
                        pr->active = false;
                        p->self_shrunk_timer = 15.0f;
                        p->stomp_flash = 0.25f;
                        w->sound_events |= SFX_SHRINK;
                        set_banner(w, "MIRROR REFLECTION! YOU SHRUNK! CRAWL THROUGH THE VENT TO THE PROJECTOR!", 5.0f);
                        set_objective(w, w->cinema_switch_pos, "CRAWL THROUGH 1-BLOCK AIR VENT -> FLIP CINEMA PROJECTOR");
                        break;
                    }
                }
                continue;
            }
            for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                Enemy *e = &w->enemies[ei];
                if (!e->active) continue;
                if (vec3_dist(pr->pos, vec3_add(e->pos, vec3_make(0, 0.9f, 0))) < 1.15f) {
                    pr->active = false;
                    Player *att = (pr->owner_player >= 0) ? &w->players[pr->owner_player] : NULL;
                    if (e->kind == ENEMY_BOSS_MECH || e->kind == ENEMY_TURRET) {
                        damage_enemy(w, e, att, 30);
                    } else {
                        e->shrunk_timer = 8.0f;
                        e->hit_flash = 0.25f;
                        if (att) att->hitmarker_timer = 0.16f;
                        set_banner(w, "ENEMY SHRUNK! WALK OVER THEM TO STOMP!", 2.5f);
                    }
                    break;
                }
            }
        } else {
            for (int pi = 0; pi < w->player_count; ++pi) {
                Player *p = &w->players[pi];
                if (!p->active || p->hp <= 0) continue;
                if (vec3_dist(pr->pos, vec3_add(p->pos, vec3_make(0, player_height(p) * 0.55f, 0))) < 0.85f) {
                    int dmg = (pr->kind == PROJ_BOSS_ROCKET) ? 16 : (pr->kind == PROJ_OCTA_WAVE ? 12 : 8);
                    damage_player(w, p, dmg);
                    pr->active = false;
                    break;
                }
            }
        }
    }
}

static void step_enemies(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        e->anim_time += dt;
        if (e->hit_flash > 0.0f) e->hit_flash -= dt;
        if (e->shrunk_timer > 0.0f) {
            e->shrunk_timer -= dt;
            continue;
        }

        if (e->kind == ENEMY_OCTABRAIN) {
            e->pos.y = e->base_y + 0.45f * sinf(e->anim_time * 2.6f);
        }

        Vec3 target = e->pos;
        bool has_target = false;
        if (w->decoy.active && vec3_dist(e->pos, w->decoy.pos) < 22.0f) {
            target = w->decoy.pos;
            has_target = true;
        } else {
            float best_d = (e->kind == ENEMY_BOSS_MECH) ? 32.0f : 22.0f;
            for (int pi = 0; pi < w->player_count; ++pi) {
                Player *p = &w->players[pi];
                if (!p->active || p->hp <= 0) continue;
                float d = vec3_dist(e->pos, p->pos);
                if (d < best_d) {
                    best_d = d;
                    target = p->pos;
                    has_target = true;
                }
            }
        }
        if (!has_target) continue;

        Vec3 delta = vec3_sub(target, e->pos);
        float dist = vec3_len(delta);
        e->yaw = atan2f(delta.x, -delta.z);

        /* Advance toward target (Turrets stay bolted; Octabrains glide in air) */
        if (dist > 4.5f && e->kind != ENEMY_BOSS_MECH && e->kind != ENEMY_TURRET) {
            Vec3 dir = vec3_norm(vec3_make(delta.x, 0.0f, delta.z));
            float spd = (e->kind == ENEMY_TROOPER) ? 2.6f : (e->kind == ENEMY_OCTABRAIN ? 2.1f : 1.9f);
            Vec3 next = vec3_add(e->pos, vec3_scale(dir, spd * dt));
            bool blocked = voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y + 0.5f), (int)floorf(next.z));
            bool has_floor = (e->kind == ENEMY_OCTABRAIN) ||
                             voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y - 0.2f), (int)floorf(next.z));
            if (!blocked && has_floor) {
                e->pos.x = next.x;
                e->pos.z = next.z;
            }
        }

        e->fire_cooldown -= dt;
        if (e->fire_cooldown <= 0.0f && dist < 24.0f) {
            if (e->kind == ENEMY_BOSS_MECH) {
                e->fire_cooldown = (w->boss_phase == 3) ? 0.65f : 1.10f;
            } else if (e->kind == ENEMY_TURRET) {
                e->fire_cooldown = 0.85f;
            } else {
                e->fire_cooldown = 1.65f;
            }
            for (int pi = 0; pi < WORLD_MAX_PROJECTILES; ++pi) {
                if (!w->projectiles[pi].active) {
                    Projectile *pr = &w->projectiles[pi];
                    memset(pr, 0, sizeof(*pr));
                    pr->active = true;
                    pr->kind = (e->kind == ENEMY_BOSS_MECH) ? PROJ_BOSS_ROCKET :
                               (e->kind == ENEMY_OCTABRAIN ? PROJ_OCTA_WAVE : PROJ_ENEMY_PLASMA);
                    pr->owner_player = -1;
                    pr->pos = vec3_add(e->pos, vec3_make(0.0f, 1.3f, 0.0f));
                    pr->vel = vec3_scale(vec3_norm(vec3_sub(vec3_add(target, vec3_make(0, 1.0f, 0)), pr->pos)), 12.5f);
                    pr->life = 2.8f;
                    break;
                }
            }
        }
    }
}

static void rebuild_dynamic_lights(GameWorld *w) {
    w->light_count = 0;
    float flick = 0.88f + 0.12f * sinf(w->time_sec * 13.0f) * cosf(w->time_sec * 7.0f);

    if (w->scene == SCENE_HUB) {
        add_light(w, vec3_make(64.0f, 3.5f, 31.0f), 1.0f, 0.48f, 0.14f, 3.0f * flick, 28.0f);
        int shards = swieta_progress_count_shards(w->shard_mask);
        add_light(w, vec3_make(64.5f, 15.5f, 64.5f), 1.0f, 0.86f, 0.40f, 1.8f + 0.08f * (float)shards, 34.0f);
        add_light(w, vec3_make(31.0f, 3.5f, 84.5f), 1.0f, 0.25f, 0.78f, 2.0f, 16.0f);
        add_light(w, vec3_make(64.5f, 3.2f, 52.0f), 0.2f, 0.92f, 1.0f, 1.4f, 14.0f);
        add_light(w, vec3_make(45.0f, 9.0f, 64.0f), 1.0f, 0.76f, 0.46f, 1.4f, 24.0f);
        add_light(w, vec3_make(83.0f, 9.0f, 64.0f), 1.0f, 0.76f, 0.46f, 1.4f, 24.0f);
    } else if (w->scene == SCENE_DAY01) {
        /* Place lights nearest to P1 plus signature city neon landmarks */
        add_light(w, vec3_make(28.0f, 6.5f, 75.0f), 1.0f, 0.24f, 0.78f, 2.8f, 26.0f); /* Yule Marquee */
        add_light(w, vec3_make(19.5f, 5.5f, 96.0f), 0.18f, 0.92f, 1.0f, 2.0f, 18.0f); /* Pawnshop Neon */
        add_light(w, vec3_make(45.0f, 4.5f, 96.0f), 0.25f, 0.65f, 1.0f, 1.8f, 16.0f); /* Blue Kiosk */
        add_light(w, vec3_make(32.0f, 5.5f, 46.0f),
                  w->cinema_restored ? 1.0f : 0.45f,
                  w->cinema_restored ? 0.88f : 0.55f,
                  w->cinema_restored ? 0.35f : 0.78f,
                  w->cinema_restored ? 2.5f : 1.3f * flick, 24.0f);
        add_light(w, vec3_make(15.5f, 3.2f, 65.0f), 0.35f, 0.96f, 1.0f, 1.7f, 14.0f); /* Mirror Room */
        add_light(w, vec3_make(53.5f, 3.2f, 52.0f), 1.0f, 0.28f, 0.85f, 1.8f, 14.0f); /* Secret Arcade */
        add_light(w, vec3_make(78.0f, 12.0f, 58.0f), 1.0f, 0.25f, 0.55f, 2.2f, 24.0f);/* Roof B Billboard */
        add_light(w, vec3_make(110.0f, 14.5f, 54.0f), 1.0f, 0.22f, 0.22f, 2.2f * flick, 24.0f); /* Roof C */
        add_light(w, vec3_make(50.0f, 16.5f, 17.0f), 1.0f, 0.32f, 0.20f, 2.6f * flick, 32.0f);  /* Penthouse */
    } else {
        const PortalMeta *meta = swieta_portal_meta(w->active_day);
        float r = meta ? meta->color_r : 0.8f;
        float g = meta ? meta->color_g : 0.8f;
        float b = meta ? meta->color_b : 1.0f;
        add_light(w, vec3_make(64.0f, 6.5f, 62.0f), r, g, b, 2.5f * flick, 26.0f);
    }

    for (int i = 0; i < w->player_count; ++i) {
        if (w->players[i].active && w->players[i].muzzle_flash > 0.0f) {
            add_light(w, player_eye_pos(&w->players[i]), 1.0f, 0.85f, 0.4f, 2.4f, 10.0f);
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (w->explosions[i].active) {
            add_light(w, w->explosions[i].pos, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b,
                      3.4f * (w->explosions[i].timer / 0.45f), 14.0f);
        }
    }
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        if (w->projectiles[i].active && w->projectiles[i].kind == PROJ_SHRINK_ORB) {
            add_light(w, w->projectiles[i].pos, 0.2f, 1.0f, 0.35f, 1.8f, 8.0f);
        }
    }
}

void world_step(GameWorld *w, const PlayerInput inputs[WORLD_MAX_PLAYERS], float dt) {
    if (!w || !inputs || dt <= 0.0f) return;
    w->tick++;
    w->time_sec += dt;
    w->sound_events = 0u;

    if (w->banner_timer > 0.0f) {
        w->banner_timer -= dt;
        if (w->banner_timer < 0.0f) w->banner_timer = 0.0f;
    }

    if (w->decoy.active) {
        w->decoy.timer -= dt;
        if (w->decoy.timer <= 0.0f) w->decoy.active = false;
    }

    for (int i = 0; i < 8; ++i) {
        if (w->explosions[i].active) {
            w->explosions[i].timer -= dt;
            if (w->explosions[i].timer <= 0.0f) w->explosions[i].active = false;
        }
    }

    SceneId start_scene = w->scene;
    for (int i = 0; i < w->player_count; ++i) {
        step_player(w, i, &inputs[i], dt);
        if (w->scene != start_scene) break;
    }

    if (w->scene == start_scene) {
        step_tripmines(w, dt);
        step_projectiles(w, dt);
        step_enemies(w, dt);
    }

    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        if (w->pickups[i].active) w->pickups[i].bob_phase += dt * 3.0f;
    }

    rebuild_dynamic_lights(w);
}

void world_build_snapshot(const GameWorld *w, NetSnapshotPacket *out_snap) {
    if (!w || !out_snap) return;
    memset(out_snap, 0, sizeof(*out_snap));
    out_snap->tick = w->tick;
    out_snap->scene_id = (uint8_t)w->scene;
    out_snap->active_day = (uint8_t)w->active_day;
    out_snap->shard_mask = w->shard_mask;
    out_snap->player_count = (uint8_t)w->player_count;
    for (int i = 0; i < w->player_count && i < NET_MAX_PLAYERS; ++i) {
        const Player *p = &w->players[i];
        NetPlayerState *ps = &out_snap->players[i];
        ps->active = p->active ? 1 : 0;
        ps->pos = p->pos;
        ps->yaw = p->yaw;
        ps->pitch = p->pitch;
        ps->hp = (int16_t)p->hp;
        ps->score = (int16_t)p->score;
        ps->weapon = (uint8_t)p->weapon;
    }
}

void world_apply_snapshot(GameWorld *w, const NetSnapshotPacket *snap, int local_player_slot) {
    if (!w || !snap) return;
    w->shard_mask |= snap->shard_mask;
    if ((SceneId)snap->scene_id != w->scene || (int)snap->active_day != w->active_day) {
        if ((SceneId)snap->scene_id == SCENE_HUB) {
            world_load_hub(w);
        } else if (snap->active_day >= 1 && snap->active_day <= SWIETA_PORTAL_COUNT) {
            bool prev_unlock = w->unlock_all;
            w->unlock_all = true;
            world_load_day(w, snap->active_day);
            w->unlock_all = prev_unlock;
        }
    }
    if (snap->player_count >= 1 && snap->player_count <= WORLD_MAX_PLAYERS) {
        w->player_count = snap->player_count;
    }
    for (int i = 0; i < (int)snap->player_count && i < WORLD_MAX_PLAYERS; ++i) {
        const NetPlayerState *ps = &snap->players[i];
        Player *p = &w->players[i];
        p->active = ps->active != 0;
        if (i == local_player_slot) {
            if (vec3_dist(p->pos, ps->pos) > 1.5f) {
                p->pos = ps->pos;
            } else {
                p->pos = vec3_lerp(p->pos, ps->pos, 0.25f);
            }
        } else {
            p->pos = vec3_lerp(p->pos, ps->pos, 0.5f);
            p->yaw = ps->yaw;
            p->pitch = ps->pitch;
        }
        p->hp = ps->hp;
        p->score = ps->score;
        if (ps->weapon < WEAP_COUNT) p->weapon = (WeaponId)ps->weapon;
    }
}
