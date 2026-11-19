#include "sim/world.h"
#include "sim/world_internal.h"
#include "sim/levels_act1.h"

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
    "SANTA-BIOS [TIP]: ACT I PORTALS (DAYS 1-6) ARE ALL ONLINE IN THE WEST WING!",
    "SANTA-BIOS [GOAL]: COLLECT MEMORY SHARDS TO LIGHT ALL 24 ORNAMENTS ON THE GRAND TREE!"
};

void world_set_banner(GameWorld *w, const char *msg, float duration) {
    if (!w || !msg) return;
    snprintf(w->banner, sizeof(w->banner), "%s", msg);
    w->banner_timer = duration;
}

void world_set_objective(GameWorld *w, Vec3 pos, const char *txt) {
    if (!w) return;
    w->objective_pos = pos;
    snprintf(w->objective_text, sizeof(w->objective_text), "%s", txt ? txt : "");
}

void world_add_light(GameWorld *w, Vec3 pos, float r, float g, float b, float intensity, float radius) {
    if (!w || w->light_count >= WORLD_MAX_LIGHTS) return;
    DynamicLight *l = &w->lights[w->light_count++];
    l->pos = pos;
    l->r = r;
    l->g = g;
    l->b = b;
    l->intensity = intensity;
    l->radius = radius;
}

void world_spawn_explosion(GameWorld *w, Vec3 pos, float radius, float r, float g, float b) {
    if (!w) return;
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

bool world_enemy_is_boss(EnemyKind kind) {
    return kind == ENEMY_BOSS_MECH || kind == ENEMY_BOSS_TREX ||
           kind == ENEMY_BOSS_SHAMBLER || kind == ENEMY_BOSS_MATRIARCH ||
           kind == ENEMY_BOSS_CYBERTITAN;
}

WeaponId world_slot_weapon(const GameWorld *w, int slot) {
    if (!w || slot < 0 || slot >= w->level_weapon_count || slot >= WORLD_MAX_SLOTS) {
        return WEAP_PISTOL;
    }
    return w->level_weapons[slot];
}

const char *world_weapon_short_name(WeaponId id) {
    static const char *kNames[WEAP_COUNT] = {
        "PISTOL", "SHOTGN", "SHRINK", "BOMB",   "MINE",
        "DUAL-P", "FLARE",  "MAGNUM",
        "USP",    "DEAGLE", "CARBNE", "SNIPER", "FL/SMK",
        "NAILGN", "ROCKET", "THUNDR",
        "P-FORK", "FLAREG", "FLAMER", "TNT",    "VOODOO",
        "CHNSAW", "SSG",    "CHAING", "Y-BFG"
    };
    return (id >= 0 && id < WEAP_COUNT) ? kNames[id] : "PISTOL";
}

int world_player_weapon_ammo(const Player *p, WeaponId id) {
    if (!p) return 0;
    switch (id) {
        case WEAP_PISTOL:
        case WEAP_DUAL_PISTOLS:
        case WEAP_USP:
        case WEAP_PITCHFORK:
        case WEAP_CHAINSAW:      return 99;
        case WEAP_SHOTGUN:
        case WEAP_SUPER_SHOTGUN: return p->ammo_shells;
        case WEAP_SHRINKER:      return p->ammo_shrink;
        case WEAP_PIPEBOMB:      return p->ammo_pipebomb;
        case WEAP_TRIPMINE:      return p->ammo_tripmine;
        case WEAP_MAGNUM:
        case WEAP_DEAGLE:
        case WEAP_CARBINE:
        case WEAP_SNIPER:
        case WEAP_NAILGUN:
        case WEAP_CHAINGUN:      return p->ammo_bullets;
        case WEAP_ROCKET:
        case WEAP_FLAREGUN:
        case WEAP_AEROSOL:
        case WEAP_TNT:           return p->ammo_rockets;
        case WEAP_THUNDERBOLT:
        case WEAP_VOODOO:
        case WEAP_BFG:           return p->ammo_cells;
        case WEAP_FLARE:
        case WEAP_FLASH_SMOKE:   return p->ammo_utility;
        default:                 return 99;
    }
}

static void reset_player_for_spawn(Player *p, Vec3 pos, float yaw, bool keep_progress,
                                   uint32_t default_owned, WeaponId default_weap) {
    int prev_score = keep_progress ? p->score : 0;
    int prev_stomps = keep_progress ? p->stomps : 0;
    uint32_t prev_keys = keep_progress ? p->keys : 0u;
    uint32_t prev_owned = keep_progress ? (p->owned_weapons | default_owned) : default_owned;
    bool prev_shotgun = (prev_owned & (1u << WEAP_SHOTGUN)) != 0u;
    int prev_cash = keep_progress ? p->cash : 0;
    bool prev_kit = keep_progress ? p->has_defuse_kit : false;
    WeaponId prev_weap = keep_progress ? p->weapon : default_weap;
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->pos = pos;
    p->yaw = yaw;
    p->pitch = 0.0f;
    p->hp = 100;
    p->max_hp = 100;
    p->armor = 25;
    p->keys = prev_keys;
    p->owned_weapons = prev_owned;
    p->has_shotgun = prev_shotgun;
    p->weapon = (prev_owned & (1u << prev_weap)) ? prev_weap : default_weap;
    p->ammo_shells = 24;
    p->ammo_shrink = 14;
    p->ammo_pipebomb = 6;
    p->ammo_tripmine = 4;
    p->ammo_bullets = 90;
    p->ammo_rockets = 24;
    p->ammo_cells = 80;
    p->ammo_utility = 10;
    p->cash = prev_cash;
    p->has_defuse_kit = prev_kit;
    p->jetpack_fuel = 100.0f;
    p->score = prev_score;
    p->stomps = prev_stomps;
}

void world_set_loadout(GameWorld *w, const WeaponId *weapons, int count, uint32_t initial_owned) {
    (void)initial_owned;
    w->level_weapon_count = (count > WORLD_MAX_SLOTS) ? WORLD_MAX_SLOTS : count;
    for (int i = 0; i < w->level_weapon_count; ++i) {
        w->level_weapons[i] = weapons[i];
    }
}

void world_spawn_players(GameWorld *w, Vec3 spawn_pos, float yaw, uint32_t initial_owned, WeaponId start_weap) {
    w->checkpoint_pos = spawn_pos;
    w->checkpoint_yaw = yaw;
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(spawn_pos.x + (float)i * 1.1f, spawn_pos.y, spawn_pos.z);
            reset_player_for_spawn(&w->players[i], sp, yaw, false, initial_owned, start_weap);
        } else {
            w->players[i].active = false;
        }
    }
}

int world_add_enemy(GameWorld *w, EnemyKind kind, Vec3 pos) {
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        if (!w->enemies[i].active) {
            Enemy *e = &w->enemies[i];
            memset(e, 0, sizeof(*e));
            e->active = true;
            e->kind = kind;
            e->pos = pos;
            e->base_y = pos.y;
            e->infight_target = -1;
            e->burn_owner = -1;
            switch (kind) {
                case ENEMY_TROOPER:          e->max_hp = 45;  break;
                case ENEMY_PIGGUARD:         e->max_hp = 85;  break;
                case ENEMY_OCTABRAIN:        e->max_hp = 75;  break;
                case ENEMY_TURRET:           e->max_hp = 90;  break;
                case ENEMY_BOSS_MECH:        e->max_hp = 360; break;
                case ENEMY_WOLF:             e->max_hp = 42;  break;
                case ENEMY_BAT:              e->max_hp = 28;  break;
                case ENEMY_STONE_GUARD:      e->max_hp = 95;  break;
                case ENEMY_BOSS_TREX:        e->max_hp = 420; break;
                case ENEMY_SYNDICATE_RIFLE:  e->max_hp = 55;  break;
                case ENEMY_SYNDICATE_SNIPER: e->max_hp = 48;  break;
                case ENEMY_OGRE:             e->max_hp = 105; break;
                case ENEMY_FIEND:            e->max_hp = 80;  break;
                case ENEMY_DEATH_KNIGHT:     e->max_hp = 120; break;
                case ENEMY_BOSS_SHAMBLER:    e->max_hp = 440; break;
                case ENEMY_CULTIST:          e->max_hp = 55;  break;
                case ENEMY_GHOUL:            e->max_hp = 78;  break;
                case ENEMY_GARGOYLE:         e->max_hp = 85;  break;
                case ENEMY_BOSS_MATRIARCH:   e->max_hp = 420; break;
                case ENEMY_IMP:              e->max_hp = 55;  break;
                case ENEMY_PINKY:            e->max_hp = 95;  break;
                case ENEMY_CACODEMON:        e->max_hp = 120; break;
                case ENEMY_BOSS_CYBERTITAN:  e->max_hp = 520; break;
            }
            e->hp = e->max_hp;
            e->fire_cooldown = 0.85f + (float)(i % 4) * 0.25f;
            return i;
        }
    }
    return -1;
}

void world_add_pickup(GameWorld *w, PickupKind kind, Vec3 pos) {
    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        if (!w->pickups[i].active) {
            w->pickups[i].active = true;
            w->pickups[i].kind = kind;
            w->pickups[i].weapon_grant = WEAP_PISTOL;
            w->pickups[i].pos = pos;
            w->pickups[i].bob_phase = (float)i * 0.65f;
            return;
        }
    }
}

void world_add_weapon_pickup(GameWorld *w, WeaponId weap, Vec3 pos) {
    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        if (!w->pickups[i].active) {
            w->pickups[i].active = true;
            w->pickups[i].kind = PICKUP_WEAPON_GRANT;
            w->pickups[i].weapon_grant = weap;
            w->pickups[i].pos = pos;
            w->pickups[i].bob_phase = (float)i * 0.65f;
            return;
        }
    }
}

int world_add_door(GameWorld *w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t key, uint8_t vtype) {
    if (w->door_count >= WORLD_MAX_DOORS) return -1;
    int idx = w->door_count++;
    DoorGate *d = &w->doors[idx];
    d->active = true;
    d->open = false;
    d->x0 = x0; d->y0 = y0; d->z0 = z0;
    d->x1 = x1; d->y1 = y1; d->z1 = z1;
    d->required_key = key;
    d->voxel_type = vtype;
    d->center = vec3_make((float)(x0 + x1 + 1) * 0.5f, (float)y0 + 1.0f, (float)(z0 + z1 + 1) * 0.5f);
    voxel_fill_box(&w->voxels, x0, y0, z0, x1, y1, z1, vtype);
    return idx;
}

void world_open_door(GameWorld *w, DoorGate *d) {
    if (!d || !d->active || d->open) return;
    d->open = true;
    voxel_fill_box(&w->voxels, d->x0, d->y0, d->z0, d->x1, d->y1, d->z1, VOX_AIR);
    w->sound_events |= SFX_DOOR;
}

void world_add_relay(GameWorld *w, int vx, int vy, int vz) {
    if (w->relay_count >= WORLD_MAX_RELAYS) return;
    RelayTarget *rt = &w->relays[w->relay_count++];
    rt->active = true;
    rt->destroyed = false;
    rt->vx = vx; rt->vy = vy; rt->vz = vz;
    rt->hp = 60;
    rt->pos = vec3_make((float)vx + 0.5f, (float)vy + 1.0f, (float)vz + 0.5f);
    voxel_fill_box(&w->voxels, vx, vy, vz, vx, vy + 2, vz, VOX_RELAY_CORE);
}

void world_add_secret(GameWorld *w, Vec3 pos, float radius, const char *name) {
    if (w->secret_count >= WORLD_MAX_SECRETS) return;
    SecretZone *sz = &w->secrets[w->secret_count++];
    sz->active = true;
    sz->found = false;
    sz->pos = pos;
    sz->radius = radius;
    sz->name = name;
}

int world_add_pushblock(GameWorld *w, int vx, int vy, int vz, int target_x, int target_z, int door_idx) {
    if (w->push_block_count >= WORLD_MAX_PUSHBLOCKS) return -1;
    int idx = w->push_block_count++;
    PushBlock *pb = &w->push_blocks[idx];
    pb->active = true;
    pb->settled = false;
    pb->vx = vx; pb->vy = vy; pb->vz = vz;
    pb->target_x = target_x;
    pb->target_z = target_z;
    pb->door_idx = door_idx;
    pb->center = vec3_make((float)vx + 1.0f, (float)vy + 1.0f, (float)vz + 1.0f);
    voxel_fill_box(&w->voxels, vx, vy, vz, vx + 1, vy + 1, vz + 1, VOX_PUSH_BLOCK);
    return idx;
}

int world_add_switch(GameWorld *w, SwitchKind kind, Vec3 pos, int door_idx, int tag, const char *label) {
    if (w->switch_count >= WORLD_MAX_SWITCHES) return -1;
    int idx = w->switch_count++;
    SwitchTrigger *sw = &w->switches[idx];
    sw->active = true;
    sw->activated = false;
    sw->kind = kind;
    sw->pos = pos;
    sw->door_idx = door_idx;
    sw->tag = tag;
    sw->progress = 0.0f;
    sw->label = label;
    return idx;
}

void world_add_barrel(GameWorld *w, Vec3 pos) {
    if (w->barrel_count >= WORLD_MAX_BARRELS) return;
    Barrel *b = &w->barrels[w->barrel_count++];
    b->active = true;
    b->pos = pos;
    b->hp = 20;
}

void world_add_teleporter(GameWorld *w, Vec3 src, Vec3 dst, float dst_yaw) {
    if (w->teleporter_count >= WORLD_MAX_TELEPORTERS) return;
    Teleporter *t = &w->teleporters[w->teleporter_count++];
    t->active = true;
    t->src = src;
    t->dst = dst;
    t->dst_yaw = dst_yaw;
}

void world_add_jumppad(GameWorld *w, Vec3 pos, Vec3 boost_vel) {
    if (w->jump_pad_count >= WORLD_MAX_JUMPPADS) return;
    JumpPad *jp = &w->jump_pads[w->jump_pad_count++];
    jp->active = true;
    jp->pos = pos;
    jp->boost_vel = boost_vel;
}

void world_add_boulder(GameWorld *w, Vec3 start_pos, Vec3 vel, Vec3 trigger_pos, float trigger_radius, float stop_z) {
    if (w->boulder_count >= WORLD_MAX_BOULDERS) return;
    BoulderTrap *b = &w->boulders[w->boulder_count++];
    b->active = true;
    b->rolling = false;
    b->pos = start_pos;
    b->vel = vel;
    b->trigger_pos = trigger_pos;
    b->trigger_radius = trigger_radius;
    b->stop_z = stop_z;
}

void world_clear_entities(GameWorld *w) {
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
    memset(w->push_blocks, 0, sizeof(w->push_blocks));
    w->push_block_count = 0;
    memset(w->switches, 0, sizeof(w->switches));
    w->switch_count = 0;
    memset(w->barrels, 0, sizeof(w->barrels));
    w->barrel_count = 0;
    memset(w->teleporters, 0, sizeof(w->teleporters));
    w->teleporter_count = 0;
    memset(w->jump_pads, 0, sizeof(w->jump_pads));
    w->jump_pad_count = 0;
    memset(w->boulders, 0, sizeof(w->boulders));
    w->boulder_count = 0;
    memset(w->flares, 0, sizeof(w->flares));
    memset(&w->decoy, 0, sizeof(w->decoy));
    memset(w->explosions, 0, sizeof(w->explosions));
}

void world_award_day_shard(GameWorld *w, int day, Player *p, const char *msg) {
    if (!w || day < 1 || day > SWIETA_PORTAL_COUNT) return;
    w->shard_mask |= (1u << (unsigned)(day - 1));
    if (p) p->score += 1500;
    w->sound_events |= SFX_SHARD;
    world_load_hub(w);
    world_set_banner(w, msg, 6.5f);
}

static void setup_hub_portals(GameWorld *w) {
    static const float wing_z[6] = {84.5f, 75.5f, 66.5f, 57.5f, 48.5f, 39.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[i].day = i + 1;
        w->portals[i].pos = vec3_make(28.5f, 1.0f, wing_z[i]);
        w->portals[i].facing_yaw = SW_PI * 0.5f;
    }
    for (int i = 0; i < 6; ++i) {
        w->portals[6 + i].day = 7 + i;
        w->portals[6 + i].pos = vec3_make(99.5f, 1.0f, wing_z[i]);
        w->portals[6 + i].facing_yaw = -SW_PI * 0.5f;
    }
    static const float south_x[6] = {38.5f, 48.5f, 58.5f, 70.5f, 80.5f, 90.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[12 + i].day = 13 + i;
        w->portals[12 + i].pos = vec3_make(south_x[i], 1.0f, 99.5f);
        w->portals[12 + i].facing_yaw = 0.0f;
    }
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
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    static const WeaponId kHubLoadout[1] = { WEAP_PISTOL };
    world_set_loadout(w, kHubLoadout, 1, 1u << WEAP_PISTOL);

    /* Vaulted Cathedral footprint: x=26..101, z=26..101, y=0..16 */
    voxel_fill_box(&w->voxels, 26, 0, 26, 101, 0, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 61, 0, 30, 67, 0, 96, VOX_CARPET);
    voxel_fill_box(&w->voxels, 30, 0, 61, 96, 0, 67, VOX_CARPET);
    voxel_fill_box(&w->voxels, 30, 0, 83, 62, 0, 86, VOX_CARPET);

    voxel_fill_box(&w->voxels, 26, 1, 26, 101, 15, 26, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 1, 101, 101, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 1, 26, 26, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 101, 1, 26, 101, 15, 101, VOX_STONE);
    voxel_fill_box(&w->voxels, 26, 16, 26, 101, 16, 101, VOX_WOOD);

    for (int z = 38; z <= 90; z += 18) {
        voxel_fill_box(&w->voxels, 26, 8, z, 26, 13, z + 4, VOX_GLASS);
        voxel_fill_box(&w->voxels, 101, 8, z, 101, 13, z + 4, VOX_GLASS);
    }

    for (int p = 42; p <= 86; p += 22) {
        voxel_fill_box(&w->voxels, 44, 1, p, 45, 15, p + 1, VOX_WOOD);
        voxel_fill_box(&w->voxels, 82, 1, p, 83, 15, p + 1, VOX_WOOD);
        voxel_fill_box(&w->voxels, 44, 14, p, 83, 15, p + 1, VOX_WOOD);
    }

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

    voxel_set(&w->voxels, 60, 1, 61, VOX_GIFT);
    voxel_set(&w->voxels, 68, 1, 62, VOX_GIFT);
    voxel_set(&w->voxels, 62, 1, 68, VOX_GIFT);
    voxel_set(&w->voxels, 67, 1, 67, VOX_GIFT);

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

    w->santa_bios_pos = vec3_make(64.5f, 1.0f, 52.0f);
    voxel_fill_box(&w->voxels, 63, 1, 50, 66, 1, 51, VOX_METAL);
    voxel_fill_box(&w->voxels, 64, 2, 50, 65, 2, 50, VOX_NEON_CYAN);

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
    voxel_set(&w->voxels, 29, 5, 83, VOX_NEON_PINK);
    voxel_set(&w->voxels, 29, 5, 86, VOX_NEON_CYAN);

    world_spawn_players(w, vec3_make(62.5f, 1.0f, 84.5f), 0.0f, 1u << WEAP_PISTOL, WEAP_PISTOL);

    int shards = swieta_progress_count_shards(w->shard_mask);
    world_set_objective(w, w->portals[0].pos, "WEST WING: ACT I PORTALS (DAYS 1-6) READY!");
    char msg[128];
    snprintf(msg, sizeof(msg), "GRAND FESTIVE HALL - SHARDS: %d/24 - ACT I (DAYS 1-6) WEST WING!", shards);
    world_set_banner(w, msg, 5.5f);
}

/* ============================================================================
 * DAY 1 (Dec 1) — DUKE NUKEM 3D HOMAGE: "NEON STRIP '96" (128x24x128 4-Beat Map)
 * ============================================================================ */
static void build_day01_duke(GameWorld *w) {
    w->scene = SCENE_DAY01;
    w->active_day = 1;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->cinema_restored = false;
    w->secret_wall_blasted = false;
    w->relays_cleared = false;
    w->boss_phase = 1;
    w->boss_defeated = false;
    w->nuke_exit_ready = false;
    w->checkpoint_stage = 0;

    static const WeaponId kDay1Loadout[5] = {
        WEAP_PISTOL, WEAP_SHOTGUN, WEAP_SHRINKER, WEAP_PIPEBOMB, WEAP_TRIPMINE
    };
    uint32_t init_owned = (1u << WEAP_PISTOL) | (1u << WEAP_SHRINKER) |
                          (1u << WEAP_PIPEBOMB) | (1u << WEAP_TRIPMINE);
    world_set_loadout(w, kDay1Loadout, 5, init_owned);

    voxel_fill_box(&w->voxels, 2, 0, 2, 125, 0, 125, VOX_SNOW);
    voxel_fill_box(&w->voxels, 18, 0, 74, 42, 0, 122, VOX_ASPHALT);
    voxel_fill_box(&w->voxels, 29, 0, 76, 30, 0, 120, VOX_ROAD_STRIPE);
    voxel_fill_box(&w->voxels, 42, 0, 84, 66, 0, 106, VOX_ASPHALT);

    voxel_fill_box(&w->voxels, 2, 1, 2, 125, 18, 2, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 125, 125, 18, 125, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 2, 2, 18, 125, VOX_BRICK);
    voxel_fill_box(&w->voxels, 125, 1, 2, 125, 18, 125, VOX_BRICK);
    voxel_fill_box(&w->voxels, 16, 15, 3, 40, 15, 3, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 78, 15, 3, 108, 15, 3, VOX_NEON_CYAN);

    voxel_fill_box(&w->voxels, 19, 1, 112, 21, 2, 114, VOX_WOOD);
    voxel_fill_box(&w->voxels, 36, 1, 106, 38, 2, 109, VOX_METAL);

    /* Pixel Pawnshop with breakable glass */
    voxel_fill_box(&w->voxels, 6, 1, 88, 18, 6, 104, VOX_BRICK);
    voxel_fill_box(&w->voxels, 7, 1, 89, 17, 5, 103, VOX_AIR);
    voxel_fill_box(&w->voxels, 18, 1, 93, 18, 3, 98, VOX_GLASS);
    voxel_fill_box(&w->voxels, 18, 5, 91, 18, 5, 100, VOX_NEON_CYAN);

    /* Alley Security Kiosk */
    voxel_fill_box(&w->voxels, 38, 1, 88, 52, 6, 104, VOX_STONE);
    voxel_fill_box(&w->voxels, 39, 1, 89, 51, 5, 103, VOX_AIR);
    voxel_fill_box(&w->voxels, 38, 1, 94, 38, 3, 98, VOX_AIR);
    voxel_fill_box(&w->voxels, 38, 5, 92, 38, 5, 100, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 44, 1, 95, 47, 1, 97, VOX_HAZARD);

    /* Beat 2: Yule Cinema */
    voxel_fill_box(&w->voxels, 10, 1, 36, 48, 8, 72, VOX_BRICK);
    voxel_fill_box(&w->voxels, 11, 1, 37, 47, 7, 71, VOX_AIR);
    voxel_fill_box(&w->voxels, 11, 0, 37, 47, 0, 71, VOX_CARPET);
    voxel_fill_box(&w->voxels, 20, 6, 73, 36, 6, 73, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 22, 5, 73, 34, 5, 73, VOX_NEON_CYAN);
    world_add_door(w, 26, 1, 72, 30, 4, 72, KEY_BLUE, VOX_DOOR_BLUE);

    for (int sz = 43; sz <= 55; sz += 4) {
        voxel_fill_box(&w->voxels, 24, 1, sz, 32, 1, sz, VOX_SEAT);
        voxel_fill_box(&w->voxels, 36, 1, sz, 44, 1, sz, VOX_SEAT);
    }
    voxel_fill_box(&w->voxels, 22, 2, 37, 44, 6, 37, VOX_CINEMA_STATIC);
    voxel_fill_box(&w->voxels, 42, 4, 58, 47, 4, 70, VOX_WOOD);

    /* Mirror Room & 1-Voxel Air Vent into Projection Booth */
    voxel_fill_box(&w->voxels, 20, 1, 48, 20, 6, 70, VOX_STONE);
    voxel_fill_box(&w->voxels, 20, 1, 64, 20, 3, 67, VOX_AIR);
    voxel_fill_box(&w->voxels, 11, 1, 48, 20, 6, 48, VOX_STONE);
    voxel_fill_box(&w->voxels, 11, 1, 59, 20, 6, 59, VOX_VENT);
    voxel_fill_box(&w->voxels, 15, 1, 59, 16, 1, 59, VOX_AIR);
    voxel_fill_box(&w->voxels, 11, 1, 63, 11, 4, 67, VOX_MIRROR);

    world_add_door(w, 20, 1, 52, 20, 3, 54, 0u, VOX_DOOR_LOCKED);
    w->cinema_switch_pos = vec3_make(15.5f, 1.5f, 52.5f);
    voxel_set(&w->voxels, 15, 1, 52, VOX_GOLD);

    /* Secret #1 Retro Arcade */
    voxel_fill_box(&w->voxels, 48, 1, 51, 48, 3, 54, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 49, 1, 44, 58, 6, 60, VOX_STONE);
    voxel_fill_box(&w->voxels, 49, 1, 45, 57, 5, 59, VOX_AIR);
    voxel_fill_box(&w->voxels, 49, 0, 45, 57, 0, 59, VOX_CARPET);
    voxel_set(&w->voxels, 56, 2, 48, VOX_NEON_CYAN);
    voxel_set(&w->voxels, 56, 2, 52, VOX_NEON_PINK);
    voxel_set(&w->voxels, 56, 2, 56, VOX_GOLD);
    world_add_secret(w, vec3_make(53.5f, 1.5f, 52.0f), 4.0f, "SECRET RETRO ARCADE");

    /* East Cinema Freight Exit Corridor */
    voxel_fill_box(&w->voxels, 48, 1, 65, 58, 5, 71, VOX_METAL);
    voxel_fill_box(&w->voxels, 48, 1, 66, 58, 4, 70, VOX_AIR);
    world_add_door(w, 57, 1, 66, 57, 4, 70, 0u, VOX_DOOR_LOCKED);

    /* Beat 3: Rooftops A, B, C */
    for (int s = 0; s < 7; ++s) {
        voxel_fill_box(&w->voxels, 60, 1, 86 + s, 63, 1 + s, 86 + s, VOX_HAZARD);
    }
    voxel_fill_box(&w->voxels, 64, 1, 82, 90, 7, 114, VOX_CONCRETE);
    voxel_fill_box(&w->voxels, 64, 8, 114, 90, 8, 114, VOX_METAL);
    voxel_fill_box(&w->voxels, 64, 8, 82, 72, 8, 82, VOX_HAZARD);

    voxel_fill_box(&w->voxels, 88, 7, 76, 89, 7, 81, VOX_METAL);
    voxel_fill_box(&w->voxels, 88, 8, 76, 89, 8, 78, VOX_HAZARD);

    voxel_fill_box(&w->voxels, 64, 1, 48, 96, 9, 75, VOX_CONCRETE);
    voxel_fill_box(&w->voxels, 70, 6, 54, 76, 8, 60, VOX_AIR);
    voxel_fill_box(&w->voxels, 71, 9, 55, 74, 9, 58, VOX_GLASS);
    voxel_set(&w->voxels, 74, 6, 58, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 74, 6, 57, 74, 7, 57, VOX_HAZARD);
    voxel_fill_box(&w->voxels, 74, 6, 56, 74, 8, 56, VOX_HAZARD);
    voxel_set(&w->voxels, 70, 7, 56, VOX_NEON_CYAN);
    world_add_secret(w, vec3_make(72.5f, 7.0f, 57.0f), 3.5f, "SKYLIGHT SMUGGLER STASH");

    voxel_fill_box(&w->voxels, 66, 10, 49, 78, 13, 49, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 68, 11, 50, 76, 12, 50, VOX_NEON_CYAN);
    world_add_relay(w, 84, 10, 60);

    voxel_fill_box(&w->voxels, 100, 1, 38, 122, 11, 78, VOX_CONCRETE);
    voxel_fill_box(&w->voxels, 97, 9, 62, 98, 9, 65, VOX_METAL);
    voxel_fill_box(&w->voxels, 99, 10, 62, 99, 10, 65, VOX_HAZARD);
    world_add_relay(w, 111, 12, 58);

    voxel_fill_box(&w->voxels, 116, 12, 39, 121, 15, 45, VOX_METAL);
    voxel_fill_box(&w->voxels, 117, 12, 40, 120, 14, 44, VOX_AIR);
    voxel_fill_box(&w->voxels, 116, 12, 41, 116, 13, 43, VOX_CRACKED);
    world_add_secret(w, vec3_make(118.5f, 12.5f, 42.0f), 3.0f, "BROADCAST TOWER VAULT");

    voxel_fill_box(&w->voxels, 104, 12, 39, 112, 15, 46, VOX_STONE);
    voxel_fill_box(&w->voxels, 105, 12, 40, 111, 14, 45, VOX_AIR);
    world_add_door(w, 107, 12, 46, 109, 14, 46, 0u, VOX_DOOR_LOCKED);

    voxel_fill_box(&w->voxels, 100, 11, 29, 106, 11, 38, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 11, 29, 106, 11, 34, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 11, 34, 106, 11, 34, VOX_NEON_CYAN);

    /* Beat 4: Penthouse Boss Arena */
    voxel_fill_box(&w->voxels, 24, 1, 6, 92, 11, 28, VOX_CONCRETE);
    voxel_fill_box(&w->voxels, 24, 12, 6, 92, 15, 6, VOX_METAL);
    voxel_fill_box(&w->voxels, 24, 12, 6, 24, 15, 28, VOX_METAL);
    voxel_fill_box(&w->voxels, 24, 12, 28, 84, 14, 28, VOX_METAL);
    voxel_fill_box(&w->voxels, 92, 12, 6, 92, 15, 28, VOX_METAL);
    voxel_fill_box(&w->voxels, 84, 12, 28, 92, 15, 28, VOX_STONE);
    world_add_door(w, 85, 12, 28, 89, 15, 28, KEY_RED, VOX_DOOR_RED);

    voxel_fill_box(&w->voxels, 42, 12, 12, 43, 14, 13, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 42, 12, 21, 43, 14, 22, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 64, 12, 12, 65, 14, 13, VOX_CRACKED);
    voxel_fill_box(&w->voxels, 64, 12, 21, 65, 14, 22, VOX_CRACKED);

    w->nuke_switch_pos = vec3_make(30.5f, 12.5f, 17.0f);
    voxel_fill_box(&w->voxels, 29, 12, 16, 31, 12, 18, VOX_HAZARD);
    voxel_set(&w->voxels, 30, 13, 17, VOX_NEON_PINK);
    w->return_portal_pos = vec3_make(8.5f, 1.0f, 118.0f);

    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(24.0f, 1.0f, 102.0f));
    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(34.0f, 1.0f, 94.0f));
    world_add_enemy(w, ENEMY_PIGGUARD,  vec3_make(45.0f, 1.0f, 99.0f));
    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(28.0f, 1.0f, 82.0f));
    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(30.0f, 1.0f, 64.0f));
    world_add_enemy(w, ENEMY_PIGGUARD,  vec3_make(34.0f, 1.0f, 50.0f));
    world_add_enemy(w, ENEMY_OCTABRAIN, vec3_make(44.0f, 5.2f, 62.0f));
    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(22.0f, 1.0f, 44.0f));
    world_add_enemy(w, ENEMY_TROOPER,   vec3_make(76.0f, 8.0f, 96.0f));
    world_add_enemy(w, ENEMY_PIGGUARD,  vec3_make(82.0f, 8.0f, 88.0f));
    world_add_enemy(w, ENEMY_TURRET,    vec3_make(79.0f, 10.0f, 66.0f));
    world_add_enemy(w, ENEMY_OCTABRAIN, vec3_make(88.0f, 12.2f, 56.0f));
    world_add_enemy(w, ENEMY_TURRET,    vec3_make(108.0f, 12.0f, 64.0f));
    world_add_enemy(w, ENEMY_PIGGUARD,  vec3_make(114.0f, 12.0f, 50.0f));
    world_add_enemy(w, ENEMY_BOSS_MECH, vec3_make(50.0f, 12.0f, 17.0f));

    world_add_pickup(w, PICKUP_SHOTGUN,     vec3_make(12.5f, 1.2f, 96.0f));
    world_add_pickup(w, PICKUP_MEDKIT,      vec3_make(12.5f, 1.2f, 92.0f));
    world_add_pickup(w, PICKUP_KEY_BLUE,    vec3_make(45.5f, 2.2f, 96.0f));
    world_add_pickup(w, PICKUP_SHRINK_AMMO, vec3_make(24.0f, 1.2f, 68.0f));
    world_add_pickup(w, PICKUP_PIPEBOMB,    vec3_make(42.0f, 1.2f, 54.0f));
    world_add_pickup(w, PICKUP_ARMOR,       vec3_make(53.5f, 1.2f, 52.0f));
    world_add_pickup(w, PICKUP_PIPEBOMB,    vec3_make(53.5f, 1.2f, 48.0f));
    world_add_pickup(w, PICKUP_JETPACK,     vec3_make(70.0f, 8.2f, 102.0f));
    world_add_pickup(w, PICKUP_TRIPMINE,    vec3_make(74.0f, 8.2f, 92.0f));
    world_add_pickup(w, PICKUP_SHELLS,      vec3_make(73.0f, 6.2f, 57.0f));
    world_add_pickup(w, PICKUP_ARMOR,       vec3_make(71.5f, 6.2f, 57.0f));
    world_add_pickup(w, PICKUP_MEDKIT,      vec3_make(88.0f, 10.2f, 68.0f));
    world_add_pickup(w, PICKUP_KEY_RED,     vec3_make(108.0f, 12.2f, 42.5f));
    world_add_pickup(w, PICKUP_TRIPMINE,    vec3_make(118.5f, 12.2f, 42.0f));
    world_add_pickup(w, PICKUP_MEDKIT,      vec3_make(76.0f, 12.2f, 22.0f));
    world_add_pickup(w, PICKUP_SHELLS,      vec3_make(76.0f, 12.2f, 12.0f));
    world_add_pickup(w, PICKUP_SHRINK_AMMO, vec3_make(54.0f, 12.2f, 10.0f));
    world_add_pickup(w, PICKUP_ARMOR,       vec3_make(54.0f, 12.2f, 24.0f));

    world_spawn_players(w, vec3_make(24.0f, 1.0f, 114.0f), 0.0f, init_owned, WEAP_PISTOL);
    world_set_objective(w, vec3_make(45.5f, 1.5f, 96.0f), "GET BLUE KEYCARD IN KIOSK - GLASS PAWNSHOP HAS SHOTGUN");
    world_set_banner(w, "DAY 1: NEON STRIP 96 - GET BLUE KEYCARD, BREACH CINEMA!", 6.0f);
}

static void build_preview_chamber(GameWorld *w, int day) {
    w->scene = SCENE_PREVIEW;
    w->active_day = day;
    world_clear_entities(w);
    voxel_world_clear(&w->voxels);

    static const WeaponId kPrevLoadout[1] = { WEAP_PISTOL };
    world_set_loadout(w, kPrevLoadout, 1, 1u << WEAP_PISTOL);

    voxel_fill_box(&w->voxels, 48, 0, 48, 80, 0, 80, VOX_STONE);
    voxel_fill_box(&w->voxels, 48, 1, 48, 80, 9, 48, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 1, 80, 80, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 1, 48, 48, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 80, 1, 48, 80, 9, 80, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 48, 10, 48, 80, 10, 80, VOX_WOOD);
    voxel_fill_box(&w->voxels, 63, 1, 58, 65, 2, 60, VOX_GOLD);

    w->return_portal_pos = vec3_make(64.0f, 1.0f, 75.0f);
    world_add_pickup(w, PICKUP_SHARD, vec3_make(64.5f, 3.0f, 59.5f));
    world_spawn_players(w, vec3_make(63.0f, 1.0f, 70.0f), 0.0f, 1u << WEAP_PISTOL, WEAP_PISTOL);

    const PortalMeta *meta = swieta_portal_meta(day);
    world_set_objective(w, vec3_make(64.5f, 3.0f, 59.5f), "COLLECT PREVIEW SHARD OR RETURN TO GRAND HALL");
    char msg[128];
    snprintf(msg, sizeof(msg), "DAY %02d PREVIEW: %s (%s) - FULL LEVEL IN PHASES 3-5",
             day, meta ? meta->portal_title : "?", meta ? meta->inspiration : "?");
    world_set_banner(w, msg, 6.0f);
}

void world_load_day(GameWorld *w, int day) {
    if (!w || day < 1 || day > SWIETA_PORTAL_COUNT) return;
    if (!swieta_portal_is_unlocked(day, w->now, w->unlock_all)) {
        world_set_banner(w, "PORTAL FROZEN BY KRAMPUS.EXE! WAIT FOR ADVENT DAY OR ENABLE UNLOCK-ALL", 3.5f);
        return;
    }
    w->sound_events |= SFX_PORTAL;
    switch (day) {
        case 1: build_day01_duke(w);          break;
        case 2: build_day02_tombraider(w);    break;
        case 3: build_day03_counterstrike(w); break;
        case 4: build_day04_quake(w);         break;
        case 5: build_day05_blood(w);         break;
        case 6: build_day06_doom(w);          break;
        default: build_preview_chamber(w, day); break;
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
            reset_player_for_spawn(&w->players[i], vec3_make(base.x + (float)i * 0.9f, base.y, base.z),
                                   w->players[0].yaw, false, w->players[0].owned_weapons, w->players[0].weapon);
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
        world_spawn_explosion(w, rt->pos, 3.2f, 1.0f, 0.35f, 0.25f);
        w->sound_events |= SFX_EXPLODE;
        if (attacker) attacker->score += 300;

        int remaining = 0;
        for (int i = 0; i < w->relay_count; ++i) {
            if (w->relays[i].active && !w->relays[i].destroyed) remaining++;
        }
        if (remaining == 0 && !w->relays_cleared) {
            w->relays_cleared = true;
            if (w->door_count > 3) world_open_door(w, &w->doors[3]);
            w->checkpoint_pos = vec3_make(106.0f, 12.0f, 48.0f);
            w->checkpoint_stage = 3;
            world_set_objective(w, vec3_make(108.0f, 12.2f, 42.5f), "GRAB RED KEYCARD ON ROOF C -> CROSS TO PENTHOUSE");
            world_set_banner(w, "ALL BROADCAST RELAYS DESTROYED! RED KEYCARD VAULT OPENED! [CHECKPOINT 3]", 5.5f);
        } else {
            world_set_banner(w, "BROADCAST RELAY SHATTERED! DESTROY REMAINING RELAY ON ROOF C!", 4.0f);
            if (w->relay_count > 1 && !w->relays[1].destroyed) {
                world_set_objective(w, w->relays[1].pos, "JETPACK TO ROOF C -> DESTROY RELAY #2");
            }
        }
    }
}

static void damage_barrel(GameWorld *w, int bi, int dmg, int owner_player) {
    if (bi < 0 || bi >= w->barrel_count || !w->barrels[bi].active || dmg <= 0) return;
    w->barrels[bi].hp -= dmg;
    if (w->barrels[bi].hp <= 0) {
        Vec3 pos = w->barrels[bi].pos;
        w->barrels[bi].active = false;
        world_explode_at(w, vec3_add(pos, vec3_make(0, 0.7f, 0)), 4.6f, 125, owner_player);
    }
}

void world_damage_enemy(GameWorld *w, Enemy *e, Player *attacker, int attacker_enemy_idx, int dmg) {
    if (!e || !e->active || dmg <= 0) return;
    if (attacker && attacker->quad_timer > 0.0f) {
        dmg *= 4; /* Quake Quad Damage! */
    }
    e->hp -= dmg;
    e->hit_flash = 0.18f;
    if (attacker) attacker->hitmarker_timer = 0.14f;

    /* Doom Monster Infighting: if struck by another enemy, turn and brawl them! */
    if (attacker_enemy_idx >= 0 && attacker_enemy_idx < WORLD_MAX_ENEMIES &&
        &w->enemies[attacker_enemy_idx] != e && w->enemies[attacker_enemy_idx].active) {
        e->infight_target = attacker_enemy_idx;
    }

    /* 3-Phase Boss Transitions across all Act I Bosses */
    if (world_enemy_is_boss(e->kind) && e->hp > 0) {
        if (w->boss_phase == 1 && e->hp <= (e->max_hp * 2) / 3) {
            w->boss_phase = 2;
            if (e->kind == ENEMY_BOSS_MECH) {
                world_add_enemy(w, ENEMY_TROOPER, vec3_make(46.0f, 12.0f, 12.0f));
                world_add_enemy(w, ENEMY_TROOPER, vec3_make(46.0f, 12.0f, 22.0f));
                world_set_banner(w, "KRAMPUS-MECH PHASE 2: ALIEN ESCORT DEPLOYED! SHRINK & STOMP THEM!", 4.5f);
            } else if (e->kind == ENEMY_BOSS_CYBERTITAN) {
                world_add_enemy(w, ENEMY_IMP, vec3_make(54.0f, 1.0f, 18.0f));
                world_add_enemy(w, ENEMY_IMP, vec3_make(70.0f, 1.0f, 18.0f));
                world_set_banner(w, "CYBER-TITAN PHASE 2: IMP ESCORT SUMMONED! TRIGGER INFIGHTING!", 4.5f);
            } else {
                world_set_banner(w, "BOSS PHASE 2: ENRAGED ASSAULT!", 4.0f);
            }
        } else if (w->boss_phase == 2 && e->hp <= e->max_hp / 3) {
            w->boss_phase = 3;
            world_set_banner(w, "BOSS PHASE 3: OVERDRIVE BARRAGE! FINISH IT!", 4.5f);
        }
    }

    if (e->hp <= 0) {
        e->active = false;
        if (attacker) {
            attacker->score += world_enemy_is_boss(e->kind) ? 1000 : 120;
            if (w->scene == SCENE_DAY03) {
                attacker->cash += 300;
            }
        }
        world_spawn_explosion(w, vec3_add(e->pos, vec3_make(0, 0.9f, 0)), 1.8f, 1.0f, 0.55f, 0.15f);
        if (world_enemy_is_boss(e->kind)) {
            w->boss_defeated = true;
            w->sound_events |= SFX_EXPLODE;
            if (w->scene == SCENE_DAY01) {
                world_set_objective(w, w->nuke_switch_pos, "PRESS USE (E) ON RED NUKE-EXIT BUTTON!");
                world_set_banner(w, "KRAMPUS-MECH MK.IV DESTROYED! PUNCH THE RED NUKE-EXIT BUTTON!", 6.0f);
            } else {
                for (int si = 0; si < w->switch_count; ++si) {
                    if (w->switches[si].kind == SW_EXIT_SHARD) {
                        world_set_objective(w, w->switches[si].pos, "BOSS DEFEATED! CLAIM MEMORY SHARD AT ALTAR (E)!");
                        break;
                    }
                }
                world_set_banner(w, "GUARDIAN BOSS DEFEATED! CLAIM THE MEMORY SHARD (USE E)!", 6.0f);
            }
        }
    }
}

void world_explode_at(GameWorld *w, Vec3 pos, float radius, int max_dmg, int owner_player) {
    world_spawn_explosion(w, pos, radius, 1.0f, 0.48f, 0.12f);
    w->sound_events |= SFX_EXPLODE;

    int destroyed = voxel_explode_cracked(&w->voxels, pos, radius * 0.9f);
    if (destroyed > 0 && w->scene == SCENE_DAY01 && !w->secret_wall_blasted &&
        vec3_dist(pos, vec3_make(48.0f, 2.0f, 52.5f)) < 6.0f) {
        w->secret_wall_blasted = true;
        if (owner_player >= 0) w->players[owner_player].score += 250;
        world_set_banner(w, "CRACKED WALL BLASTED! SECRET RETRO ARCADE UNCOVERED! (+250)", 4.5f);
    }

    Player *att = (owner_player >= 0 && owner_player < WORLD_MAX_PLAYERS) ? &w->players[owner_player] : NULL;
    for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
        Enemy *e = &w->enemies[ei];
        if (!e->active) continue;
        float d = vec3_dist(pos, vec3_add(e->pos, vec3_make(0.0f, 0.9f, 0.0f)));
        if (d < radius * 1.2f) {
            int dmg = (int)((float)max_dmg * (1.0f - d / (radius * 1.3f)));
            if (dmg > 0) world_damage_enemy(w, e, att, -1, dmg);
        }
    }
    for (int ri = 0; ri < w->relay_count; ++ri) {
        if (w->relays[ri].active && !w->relays[ri].destroyed &&
            vec3_dist(pos, w->relays[ri].pos) < radius * 1.25f) {
            damage_relay(w, &w->relays[ri], att, max_dmg);
        }
    }
    /* Chain-reaction explosive barrels! */
    for (int bi = 0; bi < w->barrel_count; ++bi) {
        if (w->barrels[bi].active &&
            vec3_dist(pos, vec3_add(w->barrels[bi].pos, vec3_make(0, 0.7f, 0))) < radius * 1.1f) {
            damage_barrel(w, bi, 50, owner_player);
        }
    }
    /* Quake Rocket Jump self-knockback physics */
    if (att && att->active && att->hp > 0) {
        Vec3 pc = vec3_add(att->pos, vec3_make(0, 0.8f, 0));
        float pd = vec3_dist(pos, pc);
        if (pd < 4.8f && (w->scene == SCENE_DAY04 || w->scene == SCENE_DAY06)) {
            Vec3 push = vec3_sub(pc, pos);
            float scale = (1.0f - pd / 4.8f);
            att->vel.x += push.x * 4.0f * scale;
            att->vel.z += push.z * 4.0f * scale;
            att->vel.y = sw_clampf(att->vel.y + 11.5f * scale, 6.0f, 15.5f);
            att->on_ground = false;
            int self_dmg = (int)(7.0f * scale);
            if (self_dmg > 0 && att->hp > self_dmg + 10) damage_player(w, att, self_dmg);
        }
    }
}

static Projectile *alloc_proj(GameWorld *w, ProjectileKind kind, int owner_p, int owner_e, Vec3 pos, Vec3 vel, float life) {
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        if (!w->projectiles[i].active) {
            Projectile *pr = &w->projectiles[i];
            memset(pr, 0, sizeof(*pr));
            pr->active = true;
            pr->kind = kind;
            pr->owner_player = owner_p;
            pr->owner_enemy = owner_e;
            pr->pos = pos;
            pr->vel = vel;
            pr->life = life;
            return pr;
        }
    }
    return NULL;
}

static void spawn_flare_or_smoke(GameWorld *w, bool is_smoke, Vec3 pos, Vec3 vel, float dur, float r, float g, float b) {
    int slot = 0;
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        if (!w->flares[i].active) { slot = i; break; }
    }
    FlareLight *fl = &w->flares[slot];
    fl->active = true;
    fl->is_smoke = is_smoke;
    fl->pos = pos;
    fl->vel = vel;
    fl->timer = dur;
    fl->r = r; fl->g = g; fl->b = b;
}

static void fire_hitscan_ray(GameWorld *w, Player *p, int pi, Vec3 eye, Vec3 dir, int dmg, float max_dist) {
    VoxelRayHit wh;
    float max_t = max_dist;
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
    int best_barrel = -1;
    float best_t = max_t;
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        float rad = world_enemy_is_boss(e->kind) ? 1.35f : (e->shrunk_timer > 0.0f ? 0.35f : 0.68f);
        float h = world_enemy_is_boss(e->kind) ? 2.8f : (e->shrunk_timer > 0.0f ? 0.6f : 1.78f);
        AABB box = {
            vec3_make(e->pos.x - rad, e->pos.y, e->pos.z - rad),
            vec3_make(e->pos.x + rad, e->pos.y + h, e->pos.z + rad)
        };
        float t = 0.0f;
        if (ray_aabb_hit(eye, dir, box, best_t, &t)) {
            best_t = t;
            best_enemy = i;
            best_barrel = -1;
        }
    }
    for (int bi = 0; bi < w->barrel_count; ++bi) {
        if (!w->barrels[bi].active) continue;
        Vec3 bp = w->barrels[bi].pos;
        AABB box = { vec3_make(bp.x - 0.55f, bp.y, bp.z - 0.55f), vec3_make(bp.x + 0.55f, bp.y + 1.35f, bp.z + 0.55f) };
        float t = 0.0f;
        if (ray_aabb_hit(eye, dir, box, best_t, &t)) {
            best_t = t;
            best_barrel = bi;
            best_enemy = -1;
        }
    }
    if (best_enemy >= 0) {
        world_damage_enemy(w, &w->enemies[best_enemy], p, -1, dmg);
    } else if (best_barrel >= 0) {
        damage_barrel(w, best_barrel, dmg, pi);
    }
}

static void fire_player_weapon(GameWorld *w, int pi, Player *p) {
    Vec3 eye = player_eye_pos(p);
    Vec3 fwd = player_forward(p);
    Vec3 right = vec3_norm(vec3_cross(fwd, vec3_make(0, 1, 0)));
    Vec3 up = vec3_cross(right, fwd);

    switch (p->weapon) {
        case WEAP_PISTOL:
            p->fire_cooldown = 0.20f;
            p->muzzle_flash = 0.10f;
            w->sound_events |= SFX_PISTOL;
            fire_hitscan_ray(w, p, pi, eye, fwd, 24, 48.0f);
            break;

        case WEAP_SHOTGUN:
            if (!(p->owned_weapons & (1u << WEAP_SHOTGUN)) || p->ammo_shells <= 0) {
                p->weapon = world_slot_weapon(w, 0);
                return;
            }
            p->ammo_shells--;
            p->fire_cooldown = 0.56f;
            p->muzzle_flash = 0.16f;
            w->sound_events |= SFX_SHOTGUN;
            {
                static const float sp[7][2] = {
                    {0,0},{-0.045f,0.01f},{0.045f,0.01f},{-0.025f,-0.03f},{0.025f,-0.03f},{-0.06f,0.03f},{0.06f,0.03f}
                };
                for (int k = 0; k < 7; ++k) {
                    Vec3 d = vec3_norm(vec3_add(fwd, vec3_add(vec3_scale(right, sp[k][0]), vec3_scale(up, sp[k][1]))));
                    fire_hitscan_ray(w, p, pi, eye, d, 14, 42.0f);
                }
            }
            break;

        case WEAP_SHRINKER:
            if (p->ammo_shrink <= 0) return;
            p->ammo_shrink--;
            p->fire_cooldown = 0.42f;
            p->muzzle_flash = 0.14f;
            w->sound_events |= SFX_SHRINK;
            alloc_proj(w, PROJ_SHRINK_ORB, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.7f)), vec3_scale(fwd, 19.0f), 3.0f);
            break;

        case WEAP_PIPEBOMB:
            if (p->ammo_pipebomb <= 0) return;
            p->ammo_pipebomb--;
            p->fire_cooldown = 0.45f;
            w->sound_events |= SFX_THROW;
            alloc_proj(w, PROJ_PIPEBOMB, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.6f)),
                       vec3_add(vec3_scale(fwd, 11.5f), vec3_make(0.0f, 3.6f, 0.0f)), 35.0f);
            break;

        case WEAP_TRIPMINE:
            if (p->ammo_tripmine <= 0) return;
            {
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
                            world_set_banner(w, "LASER TRIPMINE PLANTED! BEAM ARMED!", 2.5f);
                            break;
                        }
                    }
                }
            }
            break;

        /* ---- DAY 2: TOMB RAIDER ---- */
        case WEAP_DUAL_PISTOLS:
            p->fire_cooldown = 0.16f;
            p->muzzle_flash = 0.09f;
            w->sound_events |= SFX_PISTOL;
            {
                /* Acrobatic Soft Lock-On onto nearest predator inside aim cone */
                Vec3 aim_dir = fwd;
                float best_dot = 0.88f;
                for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
                    if (!w->enemies[i].active) continue;
                    Vec3 tc = vec3_add(w->enemies[i].pos, vec3_make(0, 1.0f, 0));
                    Vec3 to = vec3_sub(tc, eye);
                    float dist = vec3_len(to);
                    if (dist < 26.0f && dist > 0.4f) {
                        Vec3 nd = vec3_scale(to, 1.0f / dist);
                        float dt = vec3_dot(fwd, nd);
                        if (dt > best_dot) {
                            best_dot = dt;
                            aim_dir = nd;
                        }
                    }
                }
                fire_hitscan_ray(w, p, pi, eye, aim_dir, 22, 42.0f);
            }
            break;

        case WEAP_FLARE:
            if (p->ammo_utility <= 0) return;
            p->ammo_utility--;
            p->fire_cooldown = 0.50f;
            w->sound_events |= SFX_THROW;
            spawn_flare_or_smoke(w, false, vec3_add(eye, vec3_scale(fwd, 0.6f)),
                                 vec3_add(vec3_scale(fwd, 10.5f), vec3_make(0, 2.8f, 0)),
                                 25.0f, 0.25f, 1.0f, 0.35f);
            world_set_banner(w, "MAGNESIUM FLARE LIT! DARK TOMB ILLUMINATED!", 2.5f);
            break;

        case WEAP_MAGNUM:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_DUAL_PISTOLS; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.28f;
            p->muzzle_flash = 0.13f;
            w->sound_events |= SFX_SHOTGUN;
            fire_hitscan_ray(w, p, pi, eye, fwd, 48, 52.0f);
            break;

        /* ---- DAY 3: COUNTER-STRIKE ---- */
        case WEAP_USP:
            p->fire_cooldown = 0.20f;
            p->muzzle_flash = 0.08f;
            w->sound_events |= SFX_PISTOL;
            fire_hitscan_ray(w, p, pi, eye, fwd, 28, 54.0f);
            break;

        case WEAP_DEAGLE:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_USP; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.34f;
            p->muzzle_flash = 0.15f;
            w->sound_events |= SFX_SHOTGUN;
            fire_hitscan_ray(w, p, pi, eye, fwd, 68, 60.0f);
            break;

        case WEAP_CARBINE:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_USP; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.11f;
            p->muzzle_flash = 0.09f;
            w->sound_events |= SFX_PISTOL;
            {
                /* Movement-accuracy & deterministic spray pattern */
                float spd = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
                float spread = (spd > 1.5f) ? 0.042f : 0.003f;
                float pat_x = sinf((float)w->tick * 1.7f) * spread;
                float pat_y = cosf((float)w->tick * 1.3f) * spread * 0.7f;
                Vec3 d = vec3_norm(vec3_add(fwd, vec3_add(vec3_scale(right, pat_x), vec3_scale(up, pat_y))));
                fire_hitscan_ray(w, p, pi, eye, d, 34, 62.0f);
            }
            break;

        case WEAP_SNIPER:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_USP; p->zoomed = false; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.85f;
            p->muzzle_flash = 0.18f;
            w->sound_events |= SFX_SHOTGUN;
            fire_hitscan_ray(w, p, pi, eye, fwd, 140, 90.0f);
            break;

        case WEAP_FLASH_SMOKE:
            if (p->ammo_utility <= 0) return;
            p->ammo_utility--;
            p->fire_cooldown = 0.65f;
            w->sound_events |= SFX_THROW;
            alloc_proj(w, PROJ_FLASHBANG, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.7f)),
                       vec3_add(vec3_scale(fwd, 13.5f), vec3_make(0, 3.2f, 0)), 0.75f);
            break;

        /* ---- DAY 4: QUAKE ---- */
        case WEAP_NAILGUN:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_SHOTGUN; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.10f;
            p->muzzle_flash = 0.07f;
            w->sound_events |= SFX_PISTOL;
            alloc_proj(w, PROJ_NAIL, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.6f)), vec3_scale(fwd, 30.0f), 2.5f);
            break;

        case WEAP_ROCKET:
            if (p->ammo_rockets <= 0) { p->weapon = world_slot_weapon(w, 0); return; }
            p->ammo_rockets--;
            p->fire_cooldown = 0.62f;
            p->muzzle_flash = 0.18f;
            w->sound_events |= SFX_ROCKET;
            alloc_proj(w, PROJ_PLAYER_ROCKET, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.65f)), vec3_scale(fwd, 23.0f), 4.0f);
            break;

        case WEAP_THUNDERBOLT:
            if (p->ammo_cells <= 0) { p->weapon = WEAP_NAILGUN; return; }
            p->ammo_cells--;
            p->fire_cooldown = 0.08f;
            p->muzzle_flash = 0.08f;
            p->lightning_timer = 0.12f;
            w->sound_events |= SFX_LIGHTNING;
            if (p->in_water) {
                /* Classic Quake Underwater Discharge! */
                p->lightning_end = vec3_add(eye, vec3_scale(fwd, 6.0f));
                world_spawn_explosion(w, eye, 4.5f, 0.35f, 0.85f, 1.0f);
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    Enemy *e = &w->enemies[ei];
                    if (!e->active) continue;
                    uint8_t ev = voxel_get(&w->voxels, (int)floorf(e->pos.x), (int)floorf(e->pos.y + 0.3f), (int)floorf(e->pos.z));
                    if (ev == VOX_WATER && vec3_dist(p->pos, e->pos) < 26.0f) {
                        world_damage_enemy(w, e, p, -1, 180);
                    }
                }
                if (p->hp > 25) damage_player(w, p, 12);
                world_set_banner(w, "THUNDERBOLT UNDERWATER DISCHARGE! TRENCH ELECTRIFIED!", 3.5f);
            } else {
                VoxelRayHit wh;
                float rdist = 24.0f;
                if (voxel_raycast_ext(&w->voxels, eye, fwd, rdist, &wh)) {
                    p->lightning_end = wh.point;
                } else {
                    p->lightning_end = vec3_add(eye, vec3_scale(fwd, rdist));
                }
                fire_hitscan_ray(w, p, pi, eye, fwd, 18, 24.0f);
            }
            break;

        /* ---- DAY 5: BLOOD ---- */
        case WEAP_PITCHFORK:
            p->fire_cooldown = 0.32f;
            w->sound_events |= SFX_MELEE;
            fire_hitscan_ray(w, p, pi, eye, fwd, 46, 3.0f);
            break;

        case WEAP_FLAREGUN:
            if (p->ammo_rockets <= 0) { p->weapon = WEAP_PITCHFORK; return; }
            p->ammo_rockets--;
            p->fire_cooldown = 0.42f;
            p->muzzle_flash = 0.14f;
            w->sound_events |= SFX_FLAME;
            alloc_proj(w, PROJ_FLARE_BOLT, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.6f)), vec3_scale(fwd, 24.0f), 3.0f);
            break;

        case WEAP_AEROSOL:
            if (p->ammo_rockets <= 0) { p->weapon = WEAP_PITCHFORK; return; }
            p->ammo_rockets--;
            p->fire_cooldown = 0.12f;
            p->muzzle_flash = 0.12f;
            w->sound_events |= SFX_FLAME;
            alloc_proj(w, PROJ_FLAME_PUFF, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.7f)), vec3_scale(fwd, 15.0f), 0.55f);
            break;

        case WEAP_TNT:
            if (p->ammo_rockets <= 0) return;
            p->ammo_rockets--;
            p->fire_cooldown = 0.55f;
            w->sound_events |= SFX_THROW;
            alloc_proj(w, PROJ_TNT_BUNDLE, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.6f)),
                       vec3_add(vec3_scale(fwd, 13.0f), vec3_make(0, 3.8f, 0)), 1.05f);
            break;

        case WEAP_VOODOO:
            if (p->ammo_cells <= 0) { p->weapon = WEAP_PITCHFORK; return; }
            p->ammo_cells--;
            p->fire_cooldown = 0.35f;
            p->muzzle_flash = 0.10f;
            w->sound_events |= SFX_SHRINK;
            fire_hitscan_ray(w, p, pi, eye, fwd, 58, 36.0f);
            break;

        /* ---- DAY 6: DOOM ---- */
        case WEAP_CHAINSAW:
            p->fire_cooldown = 0.09f;
            w->sound_events |= SFX_MELEE;
            fire_hitscan_ray(w, p, pi, eye, fwd, p->berserk_timer > 0.0f ? 85 : 28, 2.8f);
            break;

        case WEAP_SUPER_SHOTGUN:
            if (p->ammo_shells < 2) { p->weapon = WEAP_CHAINSAW; return; }
            p->ammo_shells -= 2;
            p->fire_cooldown = 0.72f;
            p->muzzle_flash = 0.20f;
            w->sound_events |= SFX_SHOTGUN;
            {
                static const float ssp[14][2] = {
                    {0,0},{-0.03f,0.01f},{0.03f,0.01f},{-0.06f,-0.01f},{0.06f,-0.01f},
                    {-0.02f,0.04f},{0.02f,0.04f},{-0.04f,-0.04f},{0.04f,-0.04f},
                    {-0.08f,0.02f},{0.08f,0.02f},{-0.07f,-0.03f},{0.07f,-0.03f},{0,-0.02f}
                };
                for (int k = 0; k < 14; ++k) {
                    Vec3 d = vec3_norm(vec3_add(fwd, vec3_add(vec3_scale(right, ssp[k][0]), vec3_scale(up, ssp[k][1]))));
                    fire_hitscan_ray(w, p, pi, eye, d, 13, 38.0f);
                }
            }
            break;

        case WEAP_CHAINGUN:
            if (p->ammo_bullets <= 0) { p->weapon = WEAP_SUPER_SHOTGUN; return; }
            p->ammo_bullets--;
            p->fire_cooldown = 0.08f;
            p->muzzle_flash = 0.08f;
            w->sound_events |= SFX_PISTOL;
            fire_hitscan_ray(w, p, pi, eye, fwd, 22, 54.0f);
            break;

        case WEAP_BFG:
            if (p->ammo_cells < 20) { p->weapon = WEAP_SUPER_SHOTGUN; return; }
            p->ammo_cells -= 20;
            p->fire_cooldown = 0.95f;
            p->muzzle_flash = 0.28f;
            w->sound_events |= SFX_QUAD;
            alloc_proj(w, PROJ_BFG_ORB, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.8f)), vec3_scale(fwd, 17.5f), 4.0f);
            break;

        default:
            break;
    }
}

static void handle_alt_fire(GameWorld *w, int pi, Player *p, const PlayerInput *inp) {
    Vec3 eye = player_eye_pos(p);
    Vec3 fwd = player_forward(p);
    Vec3 right = vec3_norm(vec3_cross(fwd, vec3_make(0, 1, 0)));

    /* Day 1: Detonate Pipebombs */
    if (w->scene == SCENE_DAY01) {
        for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
            Projectile *pr = &w->projectiles[i];
            if (!pr->active || pr->kind != PROJ_PIPEBOMB || pr->owner_player != pi) continue;
            pr->active = false;
            world_explode_at(w, pr->pos, 3.8f, 110, pi);
        }
        return;
    }

    /* Day 2: Tomb Raider Acrobatic Side/Back-Flip Dodge */
    if (w->scene == SCENE_DAY02 && p->dodge_cooldown <= 0.0f && p->on_ground) {
        p->dodge_cooldown = 0.75f;
        Vec3 fwd_xz = vec3_make(sinf(p->yaw), 0.0f, -cosf(p->yaw));
        Vec3 right_xz = vec3_make(cosf(p->yaw), 0.0f, sinf(p->yaw));
        Vec3 burst = (fabsf(inp->move_x) > 0.1f)
            ? vec3_scale(right_xz, inp->move_x > 0.0f ? 11.0f : -11.0f)
            : vec3_scale(fwd_xz, -10.0f);
        p->vel.x = burst.x;
        p->vel.z = burst.z;
        p->vel.y = 6.4f;
        p->on_ground = false;
        w->sound_events |= SFX_THROW;
        return;
    }

    /* Day 3: Sniper Scope Zoom OR Tactical Smoke Grenade */
    if (p->weapon == WEAP_SNIPER) {
        p->zoomed = !p->zoomed;
        w->sound_events |= SFX_SWITCH;
        return;
    }
    if (p->weapon == WEAP_FLASH_SMOKE && p->ammo_utility > 0) {
        p->ammo_utility--;
        w->sound_events |= SFX_THROW;
        spawn_flare_or_smoke(w, true, vec3_add(eye, vec3_scale(fwd, 1.5f)),
                             vec3_add(vec3_scale(fwd, 9.0f), vec3_make(0, 2.2f, 0)),
                             14.0f, 0.80f, 0.84f, 0.88f);
        world_set_banner(w, "SMOKE SCREEN DEPLOYED! SNIPER SIGHTLINES BLOCKED!", 3.0f);
        return;
    }

    /* Day 5 Blood: Signature Alt-Fire Modes! */
    if (p->weapon == WEAP_FLAREGUN && p->ammo_rockets >= 3) {
        p->ammo_rockets -= 3;
        p->fire_cooldown = 0.65f;
        p->muzzle_flash = 0.22f;
        w->sound_events |= SFX_FLAME;
        for (int k = -2; k <= 2; ++k) {
            Vec3 d = vec3_norm(vec3_add(fwd, vec3_scale(right, (float)k * 0.09f)));
            alloc_proj(w, PROJ_FLARE_BOLT, pi, -1, vec3_add(eye, vec3_scale(d, 0.7f)), vec3_scale(d, 22.0f), 2.5f);
        }
        world_set_banner(w, "FLARE STARBURST ALT-FIRE!", 2.0f);
    } else if (p->weapon == WEAP_AEROSOL && p->ammo_rockets >= 4) {
        p->ammo_rockets -= 4;
        p->fire_cooldown = 0.75f;
        w->sound_events |= SFX_THROW;
        alloc_proj(w, PROJ_MOLOTOV, pi, -1, vec3_add(eye, vec3_scale(fwd, 0.7f)),
                   vec3_add(vec3_scale(fwd, 13.0f), vec3_make(0, 3.8f, 0)), 1.2f);
        world_set_banner(w, "AEROSOL MOLOTOV THROWN!", 2.0f);
    } else if (p->weapon == WEAP_TNT && p->ammo_rockets > 0) {
        p->ammo_rockets--;
        p->fire_cooldown = 0.60f;
        w->sound_events |= SFX_SWITCH;
        alloc_proj(w, PROJ_TNT_PROX, pi, -1, vec3_add(p->pos, vec3_add(vec3_scale(fwd, 1.2f), vec3_make(0, 0.4f, 0))),
                   vec3_make(0, 0, 0), 40.0f);
        world_set_banner(w, "PROXIMITY TNT ARMED!", 2.2f);
    } else if (p->weapon == WEAP_VOODOO && p->ammo_cells >= 10) {
        p->ammo_cells -= 10;
        p->fire_cooldown = 0.85f;
        w->sound_events |= SFX_QUAD;
        world_spawn_explosion(w, p->pos, 4.5f, 0.75f, 0.15f, 0.95f);
        for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
            if (w->enemies[ei].active && vec3_dist(p->pos, w->enemies[ei].pos) < 12.0f) {
                world_damage_enemy(w, &w->enemies[ei], p, -1, 80);
            }
        }
        world_set_banner(w, "VOODOO CURSE SHOCKWAVE!", 2.2f);
    }
}

static void try_open_nearby_doors(GameWorld *w, Player *p) {
    for (int i = 0; i < w->door_count; ++i) {
        DoorGate *d = &w->doors[i];
        if (!d->active || d->open || d->required_key == 0u) continue;
        if (vec3_dist(p->pos, d->center) < 3.4f) {
            if ((p->keys & d->required_key) != 0u) {
                world_open_door(w, d);
                if (w->scene == SCENE_DAY01) {
                    if (d->required_key == KEY_BLUE) {
                        world_set_objective(w, vec3_make(13.0f, 1.5f, 65.0f), "ENTER MIRROR ROOM -> SELF-SHRINK -> CRAWL VENT TO PROJECTOR");
                        world_set_banner(w, "BLUE KEYCARD ACCEPTED! WELCOME TO YULE CINEMA!", 4.5f);
                    } else if (d->required_key == KEY_RED) {
                        w->checkpoint_pos = vec3_make(78.0f, 12.0f, 22.0f);
                        w->checkpoint_stage = 4;
                        world_set_objective(w, vec3_make(50.0f, 12.5f, 17.0f), "DEFEAT KRAMPUS-MECH MK.IV ON THE PENTHOUSE!");
                        world_set_banner(w, "RED KEYCARD ACCEPTED! PENTHOUSE BOSS ARENA! [CHECKPOINT 4]", 5.0f);
                    }
                } else {
                    const char *kname = (d->required_key == KEY_BLUE) ? "BLUE" :
                                        (d->required_key == KEY_YELLOW ? "YELLOW" : "RED");
                    char mb[96];
                    snprintf(mb, sizeof(mb), "%s KEY ACCEPTED! GATE OPENED!", kname);
                    world_set_banner(w, mb, 3.8f);
                    if (w->scene == SCENE_DAY06 && d->required_key == KEY_RED) {
                        w->checkpoint_pos = vec3_make(62.0f, 1.0f, 36.0f);
                        w->checkpoint_stage = 4;
                        world_set_objective(w, vec3_make(62.0f, 1.5f, 18.0f), "DEFEAT HORNED CYBER-TITAN IN BRIMSTONE REACTOR!");
                    }
                }
            } else {
                const char *req = (d->required_key == KEY_BLUE) ? "LOCKED! REQUIRES BLUE KEYCARD!" :
                                  (d->required_key == KEY_YELLOW ? "LOCKED! REQUIRES YELLOW KEYCARD!" :
                                   "LOCKED! REQUIRES RED KEYCARD!");
                world_set_banner(w, req, 2.5f);
            }
        }
    }
}

static void try_push_nearby_block(GameWorld *w, Player *p) {
    for (int i = 0; i < w->push_block_count; ++i) {
        PushBlock *pb = &w->push_blocks[i];
        if (!pb->active || pb->settled) continue;
        if (vec3_dist(p->pos, pb->center) < 3.6f) {
            /* Slide 2 voxels toward the golden pressure plate (or player look direction) */
            int dx = pb->target_x - pb->vx;
            int dz = pb->target_z - pb->vz;
            int step_x = (dx > 0) ? 2 : (dx < 0 ? -2 : 0);
            int step_z = (step_x == 0) ? ((dz > 0) ? 2 : (dz < 0 ? -2 : 0)) : 0;
            if (step_x == 0 && step_z == 0) continue;

            voxel_fill_box(&w->voxels, pb->vx, pb->vy, pb->vz, pb->vx + 1, pb->vy + 1, pb->vz + 1, VOX_AIR);
            pb->vx += step_x;
            pb->vz += step_z;
            pb->center = vec3_make((float)pb->vx + 1.0f, (float)pb->vy + 1.0f, (float)pb->vz + 1.0f);
            voxel_fill_box(&w->voxels, pb->vx, pb->vy, pb->vz, pb->vx + 1, pb->vy + 1, pb->vz + 1, VOX_PUSH_BLOCK);
            w->sound_events |= SFX_DOOR;

            if (pb->vx == pb->target_x && pb->vz == pb->target_z) {
                pb->settled = true;
                if (pb->door_idx >= 0 && pb->door_idx < w->door_count) {
                    world_open_door(w, &w->doors[pb->door_idx]);
                }
                w->checkpoint_pos = vec3_make(44.0f, 1.0f, 88.0f);
                w->checkpoint_stage = 1;
                p->score += 250;
                world_set_objective(w, vec3_make(28.5f, 4.5f, 66.0f), "ENTER CISTERN -> CLIMB WEST LEDGE -> FLIP WATER VALVE (E)");
                world_set_banner(w, "PRESSURE PLATE LOCKED! CISTERN GATE OPENED! [CHECKPOINT 1]", 5.5f);
            }
            return;
        }
    }
}

static void handle_player_use(GameWorld *w, Player *p) {
    if (w->scene == SCENE_HUB) {
        if (vec3_dist(p->pos, w->santa_bios_pos) < 3.8f) {
            w->sound_events |= SFX_SWITCH;
            world_set_banner(w, kSantaLogs[w->santa_log_index % 6], 5.0f);
            w->santa_log_index = (w->santa_log_index + 1) % 6;
        }
        return;
    }

    try_open_nearby_doors(w, p);
    try_push_nearby_block(w, p);

    for (int si = 0; si < w->switch_count; ++si) {
        SwitchTrigger *sw = &w->switches[si];
        if (sw->active && vec3_dist(p->pos, sw->pos) < 3.6f) {
            act1_handle_switch_use(w, p, sw);
            return;
        }
    }

    if (w->scene == SCENE_DAY01) {
        if (!w->cinema_restored && vec3_dist(p->pos, w->cinema_switch_pos) < 3.2f) {
            w->cinema_restored = true;
            voxel_replace_all(&w->voxels, VOX_CINEMA_STATIC, VOX_CINEMA_LIT);
            if (w->door_count > 1) world_open_door(w, &w->doors[1]);
            if (w->door_count > 2) world_open_door(w, &w->doors[2]);
            w->checkpoint_pos = vec3_make(44.0f, 1.0f, 66.0f);
            w->checkpoint_stage = 2;
            p->score += 350;
            w->sound_events |= SFX_SWITCH;
            world_set_objective(w, w->relays[0].pos, "TAKE EAST FREIGHT RAMP TO ROOFTOPS -> DESTROY 2 RELAYS");
            world_set_banner(w, "YULE CINEMA RESTORED! EAST FREIGHT EXIT OPEN! [CHECKPOINT 2]", 5.5f);
        } else if (vec3_dist(p->pos, w->nuke_switch_pos) < 3.4f) {
            if (!w->cinema_restored || !w->boss_defeated) {
                world_set_banner(w, "RESTORE CINEMA PROJECTOR & DEFEAT KRAMPUS-MECH FIRST!", 3.5f);
            } else {
                w->nuke_exit_ready = true;
                world_award_day_shard(w, 1, p, "DAY 1 COMPLETE! GOLDEN SHADES SHARD LIT ON THE GRAND TREE!");
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
            WeaponId start_w = world_slot_weapon(w, 0);
            reset_player_for_spawn(p, w->checkpoint_pos, w->checkpoint_yaw, true, p->owned_weapons, start_w);
            world_set_banner(w, "RESPAWNED AT LAST CHECKPOINT! GET BACK IN THE FIGHT!", 3.0f);
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
    if (p->quad_timer > 0.0f) p->quad_timer -= dt;
    if (p->berserk_timer > 0.0f) p->berserk_timer -= dt;
    if (p->rad_suit_timer > 0.0f) p->rad_suit_timer -= dt;
    if (p->flash_blind_timer > 0.0f) p->flash_blind_timer -= dt;
    if (p->dodge_cooldown > 0.0f) p->dodge_cooldown -= dt;
    if (p->lightning_timer > 0.0f) p->lightning_timer -= dt;
    if (p->hazard_tick > 0.0f) p->hazard_tick -= dt;

    /* Safe un-shrink: never stand up inside a 1-voxel miniature air duct */
    if (p->self_shrunk_timer > 0.0f) {
        p->self_shrunk_timer -= dt;
        if (p->self_shrunk_timer <= 0.0f) {
            if (player_collides_h(&w->voxels, p->pos, kPlayerTallHeight)) {
                p->self_shrunk_timer = 0.25f;
            } else {
                p->self_shrunk_timer = 0.0f;
                world_set_banner(w, "RETURNED TO FULL SIZE!", 2.0f);
            }
        }
    }

    uint16_t pressed = (uint16_t)(inp->buttons & (uint16_t)(~p->prev_buttons));
    p->prev_buttons = inp->buttons;

    /* Keep has_shotgun in sync with owned_weapons */
    if (p->has_shotgun) p->owned_weapons |= (1u << WEAP_SHOTGUN);
    else p->owned_weapons &= ~(1u << WEAP_SHOTGUN);

    if (inp->select_weapon >= 0 && inp->select_weapon < WEAP_COUNT) {
        if ((p->owned_weapons & (1u << (unsigned)inp->select_weapon)) != 0u) {
            p->weapon = (WeaponId)inp->select_weapon;
            if (p->weapon != WEAP_SNIPER) p->zoomed = false;
        }
    } else if ((pressed & INP_NEXT_WEAP) && w->level_weapon_count > 1) {
        int cur_slot = 0;
        for (int s = 0; s < w->level_weapon_count; ++s) {
            if (w->level_weapons[s] == p->weapon) { cur_slot = s; break; }
        }
        for (int step = 1; step <= w->level_weapon_count; ++step) {
            WeaponId cand = w->level_weapons[(cur_slot + step) % w->level_weapon_count];
            if ((p->owned_weapons & (1u << (unsigned)cand)) != 0u) {
                p->weapon = cand;
                if (p->weapon != WEAP_SNIPER) p->zoomed = false;
                break;
            }
        }
    }

    /* Check liquid / hazard cell at player feet & waist */
    uint8_t foot_vox = voxel_get(&w->voxels, (int)floorf(p->pos.x), (int)floorf(p->pos.y + 0.2f), (int)floorf(p->pos.z));
    uint8_t ground_vox = voxel_get(&w->voxels, (int)floorf(p->pos.x), (int)floorf(p->pos.y - 0.1f), (int)floorf(p->pos.z));
    p->in_water = (foot_vox == VOX_WATER);
    if (p->hazard_tick <= 0.0f) {
        if (foot_vox == VOX_LAVA || ground_vox == VOX_LAVA) {
            damage_player(w, p, 10);
            p->hazard_tick = 0.45f;
        } else if ((foot_vox == VOX_SLIME || ground_vox == VOX_SLIME) && p->rad_suit_timer <= 0.0f) {
            damage_player(w, p, 6);
            p->hazard_tick = 0.55f;
        } else if (foot_vox == VOX_SPIKES) {
            damage_player(w, p, 12);
            p->hazard_tick = 0.45f;
        }
    }

    /* Movement in XZ (Quake strafe air-control + Doom high base pace!) */
    Vec3 fwd_xz = vec3_make(sinf(p->yaw), 0.0f, -cosf(p->yaw));
    Vec3 right_xz = vec3_make(cosf(p->yaw), 0.0f, sinf(p->yaw));
    float base_spd = (w->scene == SCENE_DAY04 || w->scene == SCENE_DAY06) ? 7.8f : 6.6f;
    float speed = (inp->buttons & INP_SPRINT) ? (base_spd + 2.9f) : base_spd;
    if (p->self_shrunk_timer > 0.0f) speed *= 0.8f;
    if (p->in_water) speed *= 0.72f;
    Vec3 wish = vec3_add(vec3_scale(fwd_xz, sw_clampf(inp->move_y, -1.0f, 1.0f)),
                         vec3_scale(right_xz, sw_clampf(inp->move_x, -1.0f, 1.0f)));
    if (vec3_len_sq(wish) > 1.0f) wish = vec3_norm(wish);

    /* Preserve airborne rocket-jump / jump-pad horizontal momentum when not overriding */
    if (!p->on_ground && (w->scene == SCENE_DAY04 || p->dodge_cooldown > 0.25f)) {
        p->vel.x = sw_lerpf(p->vel.x, wish.x * speed, 0.14f);
        p->vel.z = sw_lerpf(p->vel.z, wish.z * speed, 0.14f);
    } else {
        p->vel.x = wish.x * speed;
        p->vel.z = wish.z * speed;
    }

    if (p->on_ground && (fabsf(p->vel.x) + fabsf(p->vel.z)) > 0.5f) {
        p->walk_bob += dt * 10.5f;
    }

    /* Jump, Swimming & Day 1 Jetpack */
    p->jetpack_active = false;
    if (p->in_water) {
        if (inp->buttons & INP_JUMP) {
            p->vel.y = 4.6f;
        } else {
            p->vel.y = sw_lerpf(p->vel.y, -0.6f, 0.15f);
        }
        p->on_ground = false;
    } else if ((pressed & INP_JUMP) && p->on_ground) {
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

    /* Axis-separated voxel collision + auto step-up + Tomb Raider Ledge Mantle! */
    if (fabsf(p->vel.x) > 1e-5f) {
        Vec3 try_x = vec3_make(p->pos.x + p->vel.x * dt, p->pos.y, p->pos.z);
        if (!player_collides_h(&w->voxels, try_x, ph)) {
            p->pos.x = try_x.x;
        } else if ((p->on_ground || p->in_water) &&
                   !player_collides_h(&w->voxels, vec3_make(try_x.x, p->pos.y + 1.01f, p->pos.z), ph)) {
            p->pos.x = try_x.x;
            p->pos.y = floorf(p->pos.y + 1.01f);
        } else if ((inp->buttons & INP_JUMP) && w->scene == SCENE_DAY02 &&
                   !player_collides_h(&w->voxels, vec3_make(try_x.x, p->pos.y + 2.02f, p->pos.z), ph)) {
            /* First-person Ledge Grab & Mantle! */
            p->pos.x = try_x.x;
            p->pos.y = floorf(p->pos.y + 2.02f);
            p->vel.y = 0.0f;
            p->on_ground = true;
            w->sound_events |= SFX_MELEE;
        }
    }

    if (fabsf(p->vel.z) > 1e-5f) {
        Vec3 try_z = vec3_make(p->pos.x, p->pos.y, p->pos.z + p->vel.z * dt);
        if (!player_collides_h(&w->voxels, try_z, ph)) {
            p->pos.z = try_z.z;
        } else if ((p->on_ground || p->in_water) &&
                   !player_collides_h(&w->voxels, vec3_make(p->pos.x, p->pos.y + 1.01f, try_z.z), ph)) {
            p->pos.z = try_z.z;
            p->pos.y = floorf(p->pos.y + 1.01f);
        } else if ((inp->buttons & INP_JUMP) && w->scene == SCENE_DAY02 &&
                   !player_collides_h(&w->voxels, vec3_make(p->pos.x, p->pos.y + 2.02f, try_z.z), ph)) {
            p->pos.z = try_z.z;
            p->pos.y = floorf(p->pos.y + 2.02f);
            p->vel.y = 0.0f;
            p->on_ground = true;
            w->sound_events |= SFX_MELEE;
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

    /* Jump Pads (Quake) */
    for (int j = 0; j < w->jump_pad_count; ++j) {
        if (w->jump_pads[j].active && vec3_dist(p->pos, w->jump_pads[j].pos) < 1.8f) {
            p->vel = w->jump_pads[j].boost_vel;
            p->on_ground = false;
            w->sound_events |= SFX_TELEPORT;
        }
    }

    /* Slipgate Teleporters + Instant Telefragging! */
    for (int t = 0; t < w->teleporter_count; ++t) {
        Teleporter *tp = &w->teleporters[t];
        if (tp->active && vec3_dist(p->pos, tp->src) < 1.65f) {
            p->pos = tp->dst;
            p->yaw = tp->dst_yaw;
            p->vel = vec3_make(0, 0, 0);
            w->sound_events |= SFX_TELEPORT;
            world_spawn_explosion(w, tp->dst, 2.5f, 0.72f, 0.30f, 1.0f);
            bool telefragged = false;
            for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                if (w->enemies[ei].active && vec3_dist(w->enemies[ei].pos, tp->dst) < 2.8f) {
                    world_damage_enemy(w, &w->enemies[ei], p, -1, 9999);
                    telefragged = true;
                }
            }
            if (telefragged) {
                world_set_banner(w, "SLIPGATE TELEFRAG! GATEKEEPER OBLITERATED!", 4.0f);
            }
        }
    }

    /* Auto-open keycard doors when walking up with matching key */
    if (w->scene != SCENE_HUB) {
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
        handle_alt_fire(w, pi, p, inp);
    }
    if ((pressed & INP_HOLO) && p->holo_cooldown <= 0.0f && w->scene == SCENE_DAY01) {
        w->decoy.active = true;
        w->decoy.owner_player = pi;
        w->decoy.pos = vec3_add(p->pos, vec3_scale(fwd_xz, 1.8f));
        w->decoy.timer = 9.0f;
        p->holo_cooldown = 10.0f;
        w->sound_events |= SFX_SWITCH;
        world_set_banner(w, "HOLO-DECOY DEPLOYED! TURRETS & ALIENS DISTRACTED!", 2.5f);
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
            world_set_banner(w, "MIGHTY BOOT STOMP! SQUISHED! (+200, +12 HP)", 2.2f);
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
            world_set_banner(w, sb, 4.0f);
        }
    }

    /* Pickups */
    for (int ki = 0; ki < WORLD_MAX_PICKUPS; ++ki) {
        Pickup *pk = &w->pickups[ki];
        if (!pk->active) continue;
        if (vec3_dist(vec3_add(p->pos, vec3_make(0, ph * 0.5f, 0)), pk->pos) < 1.65f) {
            pk->active = false;
            w->sound_events |= SFX_PICKUP;
            switch (pk->kind) {
                case PICKUP_MEDKIT:      p->hp = (p->hp + 35 > p->max_hp) ? p->max_hp : p->hp + 35; break;
                case PICKUP_ARMOR:       p->armor = (p->armor + 50 > 100) ? 100 : p->armor + 50; break;
                case PICKUP_SHOTGUN:
                    p->has_shotgun = true;
                    p->owned_weapons |= (1u << WEAP_SHOTGUN);
                    p->ammo_shells += 16;
                    p->weapon = WEAP_SHOTGUN;
                    world_set_banner(w, "SHOTGUN ACQUIRED! (KEY 2)", 3.0f);
                    break;
                case PICKUP_WEAPON_GRANT:
                    p->owned_weapons |= (1u << (unsigned)pk->weapon_grant);
                    if (pk->weapon_grant == WEAP_SHOTGUN) p->has_shotgun = true;
                    p->ammo_shells += 12;
                    p->ammo_bullets += 45;
                    p->ammo_rockets += 12;
                    p->ammo_cells += 50;
                    p->weapon = pk->weapon_grant;
                    {
                        char wb[96];
                        snprintf(wb, sizeof(wb), "ACQUIRED %s!", world_weapon_short_name(pk->weapon_grant));
                        world_set_banner(w, wb, 3.5f);
                    }
                    break;
                case PICKUP_SHELLS:      p->ammo_shells += 16; break;
                case PICKUP_SHRINK_AMMO: p->ammo_shrink += 8; break;
                case PICKUP_PIPEBOMB:    p->ammo_pipebomb += 4; break;
                case PICKUP_TRIPMINE:    p->ammo_tripmine += 4; break;
                case PICKUP_JETPACK:     p->jetpack_fuel = 100.0f; break;
                case PICKUP_BULLETS:     p->ammo_bullets += 50; break;
                case PICKUP_ROCKETS:     p->ammo_rockets += 10; break;
                case PICKUP_CELLS:       p->ammo_cells += 60; break;
                case PICKUP_UTILITY:     p->ammo_utility += 6; break;
                case PICKUP_CASH:        p->cash += 800; world_set_banner(w, "FOUND CASH STASH (+$800)!", 3.0f); break;
                case PICKUP_QUAD:
                    p->quad_timer = 20.0f;
                    w->sound_events |= SFX_QUAD;
                    world_set_banner(w, "QUAD DAMAGE! 4X WEAPON POWER FOR 20 SECONDS!", 4.5f);
                    break;
                case PICKUP_BERSERK:
                    p->hp = p->max_hp;
                    p->berserk_timer = 25.0f;
                    w->sound_events |= SFX_QUAD;
                    world_set_banner(w, "BERSERK PACK! FULL HEALTH + SUPER MELEE FRENZY!", 4.5f);
                    break;
                case PICKUP_RADSUIT:
                    p->rad_suit_timer = 35.0f;
                    world_set_banner(w, "RADIATION SUIT ON! SAFE TO WADE THROUGH RADIOACTIVE SLIME!", 4.5f);
                    break;
                case PICKUP_KEY_BLUE:
                    for (int k = 0; k < w->player_count; ++k) w->players[k].keys |= KEY_BLUE;
                    if (w->scene == SCENE_DAY01) {
                        w->checkpoint_pos = vec3_make(30.0f, 1.0f, 82.0f);
                        w->checkpoint_stage = 1;
                        world_set_objective(w, vec3_make(28.0f, 1.5f, 72.0f), "UNLOCK YULE CINEMA BLUE DOOR (NORTH)");
                        world_set_banner(w, "BLUE ACCESS CARD COLLECTED! [CHECKPOINT 1] BREACH YULE CINEMA!", 5.0f);
                    } else if (w->scene == SCENE_DAY02) {
                        w->checkpoint_pos = vec3_make(44.0f, 5.0f, 34.0f);
                        w->checkpoint_stage = 3;
                        world_set_objective(w, vec3_make(46.0f, 5.5f, 16.0f), "UNLOCK VALLEY GATE -> DEFEAT THE T-REX!");
                        world_set_banner(w, "BLUE RELIC KEY ACQUIRED! [CHECKPOINT 3] ENTER THE LOST VALLEY!", 5.0f);
                    } else if (w->scene == SCENE_DAY04) {
                        w->checkpoint_pos = vec3_make(30.0f, 3.0f, 54.0f);
                        w->checkpoint_stage = 2;
                        world_set_objective(w, vec3_make(60.0f, 3.5f, 52.0f), "UNLOCK BLUE RUNE GATE -> ENTER SLIPGATE #0!");
                        world_set_banner(w, "BLUE RUNE ACQUIRED! [CHECKPOINT 2] ENTER SLIPGATE!", 4.5f);
                    } else if (w->scene == SCENE_DAY06) {
                        w->checkpoint_pos = vec3_make(46.0f, 1.0f, 64.0f);
                        w->checkpoint_stage = 1;
                        world_set_objective(w, vec3_make(20.0f, 1.5f, 46.0f), "UNLOCK BLUE DOOR (WEST) -> GRAB YELLOW KEYCARD");
                        world_set_banner(w, "BLUE KEYCARD ACQUIRED! [CHECKPOINT 1] OPEN WEST TECH WING!", 4.5f);
                    }
                    break;
                case PICKUP_KEY_YELLOW:
                    for (int k = 0; k < w->player_count; ++k) w->players[k].keys |= KEY_YELLOW;
                    w->checkpoint_pos = vec3_make(62.0f, 1.0f, 64.0f);
                    w->checkpoint_stage = 2;
                    world_set_objective(w, vec3_make(106.0f, 1.5f, 46.0f), "UNLOCK YELLOW DOOR (EAST) -> CLAIM YULE-BFG & RED KEY!");
                    world_set_banner(w, "YELLOW KEYCARD ACQUIRED! [CHECKPOINT 2] OPEN EAST BFG VAULT!", 4.5f);
                    break;
                case PICKUP_KEY_RED:
                    for (int k = 0; k < w->player_count; ++k) w->players[k].keys |= KEY_RED;
                    if (w->scene == SCENE_DAY01) {
                        world_set_objective(w, vec3_make(87.0f, 12.5f, 28.0f), "CROSS NEON SKYBRIDGE -> UNLOCK RED PENTHOUSE GATE");
                        world_set_banner(w, "RED ACCESS CARD COLLECTED! CROSS THE SKYBRIDGE TO THE PENTHOUSE!", 5.0f);
                    } else if (w->scene == SCENE_DAY04) {
                        w->checkpoint_pos = vec3_make(90.0f, 3.0f, 56.0f);
                        w->checkpoint_stage = 3;
                        world_set_objective(w, vec3_make(90.0f, 3.5f, 49.0f), "OPEN RED RUNE GATE -> SLIPGATE TO SHAMBLER CATHEDRAL!");
                        world_set_banner(w, "RED RUNE ACQUIRED! [CHECKPOINT 3] ENTER SHAMBLER SLIPGATE!", 4.5f);
                    } else if (w->scene == SCENE_DAY05) {
                        w->checkpoint_pos = vec3_make(60.0f, 1.0f, 36.0f);
                        w->checkpoint_stage = 3;
                        world_set_objective(w, vec3_make(60.0f, 1.5f, 16.0f), "UNLOCK BIG TOP -> BANISH GARGOYLE MATRIARCH!");
                        world_set_banner(w, "RED SKULL KEY ACQUIRED! [CHECKPOINT 3] ENTER THE BIG TOP!", 4.5f);
                    } else if (w->scene == SCENE_DAY06) {
                        w->checkpoint_pos = vec3_make(62.0f, 1.0f, 44.0f);
                        w->checkpoint_stage = 3;
                        world_set_objective(w, vec3_make(62.0f, 1.5f, 18.0f), "OPEN RED REACTOR GATE -> DEFEAT CYBER-TITAN!");
                        world_set_banner(w, "RED KEYCARD ACQUIRED! [CHECKPOINT 3] BREACH BRIMSTONE REACTOR!", 5.0f);
                    }
                    break;
                case PICKUP_SHARD:
                    if (w->active_day >= 1 && w->active_day <= SWIETA_PORTAL_COUNT) {
                        w->shard_mask |= (1u << (unsigned)(w->active_day - 1));
                        w->sound_events |= SFX_SHARD;
                        world_set_banner(w, "PREVIEW MEMORY SHARD COLLECTED!", 3.0f);
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

static void step_flares(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        FlareLight *fl = &w->flares[i];
        if (!fl->active) continue;
        fl->timer -= dt;
        if (fl->timer <= 0.0f) { fl->active = false; continue; }
        fl->vel.y -= kGravity * 0.65f * dt;
        Vec3 next = vec3_add(fl->pos, vec3_scale(fl->vel, dt));
        if (voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y), (int)floorf(next.z))) {
            fl->vel = vec3_make(0, 0, 0);
        } else {
            fl->pos = next;
        }
    }
}

static void step_tripmines(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_TRIPMINES; ++i) {
        Tripmine *tm = &w->tripmines[i];
        if (!tm->active) continue;
        if (tm->arm_timer > 0.0f) { tm->arm_timer -= dt; continue; }
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
                world_explode_at(w, closest, 3.6f, 120, tm->owner_player);
                world_set_banner(w, "LASER TRIPMINE TRIGGERED!", 2.0f);
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

        /* Timed explosions for TNT, Molotov, Flashbang, Enemy Grenades */
        if (pr->life <= 0.0f) {
            pr->active = false;
            if (pr->kind == PROJ_TNT_BUNDLE || pr->kind == PROJ_ENEMY_GRENADE) {
                world_explode_at(w, pr->pos, 4.4f, 125, pr->owner_player);
            } else if (pr->kind == PROJ_MOLOTOV) {
                world_explode_at(w, pr->pos, 4.8f, 95, pr->owner_player);
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    if (w->enemies[ei].active && vec3_dist(pr->pos, w->enemies[ei].pos) < 5.2f) {
                        w->enemies[ei].burn_timer = 5.5f;
                        w->enemies[ei].burn_owner = pr->owner_player;
                    }
                }
            } else if (pr->kind == PROJ_FLASHBANG) {
                w->sound_events |= SFX_FLASH;
                world_spawn_explosion(w, pr->pos, 3.5f, 1.0f, 1.0f, 1.0f);
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    if (w->enemies[ei].active && vec3_dist(pr->pos, w->enemies[ei].pos) < 15.0f) {
                        w->enemies[ei].blind_timer = 5.0f;
                        w->enemies[ei].hit_flash = 0.4f;
                    }
                }
                for (int pi = 0; pi < w->player_count; ++pi) {
                    if (w->players[pi].active && vec3_dist(pr->pos, w->players[pi].pos) < 6.0f) {
                        w->players[pi].flash_blind_timer = 0.7f;
                    }
                }
                world_set_banner(w, "FLASHBANG! SYNDICATE TROOPERS BLINDED FOR 5 SECONDS!", 3.0f);
            }
            continue;
        }

        /* Proximity TNT check */
        if (pr->kind == PROJ_TNT_PROX) {
            if (pr->life < 39.5f) {
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    if (w->enemies[ei].active && vec3_dist(pr->pos, w->enemies[ei].pos) < 2.6f) {
                        pr->active = false;
                        world_explode_at(w, pr->pos, 4.5f, 130, pr->owner_player);
                        break;
                    }
                }
            }
            continue;
        }

        /* Bouncing ballistic projectiles */
        if (pr->kind == PROJ_PIPEBOMB || pr->kind == PROJ_TNT_BUNDLE ||
            pr->kind == PROJ_MOLOTOV || pr->kind == PROJ_FLASHBANG || pr->kind == PROJ_ENEMY_GRENADE) {
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
        uint8_t hit_type = voxel_get(&w->voxels, (int)floorf(next.x), (int)floorf(next.y), (int)floorf(next.z));

        if (voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y), (int)floorf(next.z))) {
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
            if (pr->kind == PROJ_PLAYER_ROCKET) {
                world_explode_at(w, pr->pos, 4.4f, 115, pr->owner_player);
            } else if (pr->kind == PROJ_BFG_ORB) {
                world_explode_at(w, pr->pos, 6.5f, 180, pr->owner_player);
                Player *att = (pr->owner_player >= 0) ? &w->players[pr->owner_player] : NULL;
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    if (w->enemies[ei].active && vec3_dist(pr->pos, w->enemies[ei].pos) < 22.0f) {
                        world_damage_enemy(w, &w->enemies[ei], att, -1, 120);
                    }
                }
            } else if (pr->kind == PROJ_BOSS_ROCKET) {
                world_spawn_explosion(w, pr->pos, 2.2f, 1.0f, 0.3f, 0.1f);
            }
            pr->active = false;
            continue;
        }
        pr->pos = next;

        /* Player-owned projectiles */
        if (pr->owner_player >= 0) {
            Player *att = &w->players[pr->owner_player];
            if (pr->kind == PROJ_SHRINK_ORB && pr->reflected) {
                for (int pi = 0; pi < w->player_count; ++pi) {
                    Player *p = &w->players[pi];
                    if (!p->active || p->hp <= 0) continue;
                    if (vec3_dist(pr->pos, vec3_add(p->pos, vec3_make(0, 1.0f, 0))) < 1.15f) {
                        pr->active = false;
                        p->self_shrunk_timer = 15.0f;
                        p->stomp_flash = 0.25f;
                        w->sound_events |= SFX_SHRINK;
                        world_set_banner(w, "MIRROR REFLECTION! YOU SHRUNK! CRAWL THROUGH THE VENT TO THE PROJECTOR!", 5.0f);
                        world_set_objective(w, w->cinema_switch_pos, "CRAWL THROUGH 1-BLOCK AIR VENT -> FLIP CINEMA PROJECTOR");
                        break;
                    }
                }
                continue;
            }
            for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                Enemy *e = &w->enemies[ei];
                if (!e->active) continue;
                float hr = world_enemy_is_boss(e->kind) ? 1.55f : 1.15f;
                if (vec3_dist(pr->pos, vec3_add(e->pos, vec3_make(0, 0.9f, 0))) < hr) {
                    pr->active = false;
                    if (pr->kind == PROJ_SHRINK_ORB) {
                        if (world_enemy_is_boss(e->kind) || e->kind == ENEMY_TURRET) {
                            world_damage_enemy(w, e, att, -1, 30);
                        } else {
                            e->shrunk_timer = 8.0f;
                            e->hit_flash = 0.25f;
                            att->hitmarker_timer = 0.16f;
                            world_set_banner(w, "ENEMY SHRUNK! WALK OVER THEM TO STOMP!", 2.5f);
                        }
                    } else if (pr->kind == PROJ_NAIL) {
                        world_damage_enemy(w, e, att, -1, 24);
                    } else if (pr->kind == PROJ_PLAYER_ROCKET) {
                        world_explode_at(w, pr->pos, 4.4f, 115, pr->owner_player);
                    } else if (pr->kind == PROJ_FLARE_BOLT) {
                        world_damage_enemy(w, e, att, -1, 26);
                        e->burn_timer = 5.0f;
                        e->burn_tick = 0.4f;
                        e->burn_owner = pr->owner_player;
                    } else if (pr->kind == PROJ_FLAME_PUFF) {
                        world_damage_enemy(w, e, att, -1, 16);
                        e->burn_timer = 3.5f;
                        e->burn_tick = 0.4f;
                        e->burn_owner = pr->owner_player;
                    } else if (pr->kind == PROJ_BFG_ORB) {
                        world_explode_at(w, pr->pos, 6.5f, 180, pr->owner_player);
                        for (int k = 0; k < WORLD_MAX_ENEMIES; ++k) {
                            if (w->enemies[k].active && vec3_dist(pr->pos, w->enemies[k].pos) < 22.0f) {
                                world_damage_enemy(w, &w->enemies[k], att, -1, 120);
                            }
                        }
                    }
                    break;
                }
            }
        } else {
            /* Enemy-owned projectiles: hit players OR hit other enemies (Doom Monster Infighting!) */
            bool hit = false;
            for (int pi = 0; pi < w->player_count; ++pi) {
                Player *p = &w->players[pi];
                if (!p->active || p->hp <= 0) continue;
                if (vec3_dist(pr->pos, vec3_add(p->pos, vec3_make(0, player_height(p) * 0.55f, 0))) < 0.85f) {
                    int dmg = (pr->kind == PROJ_BOSS_ROCKET) ? 16 : (pr->kind == PROJ_OCTA_WAVE ? 12 : 8);
                    damage_player(w, p, dmg);
                    pr->active = false;
                    hit = true;
                    break;
                }
            }
            if (!hit && pr->owner_enemy >= 0) {
                for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                    if (ei == pr->owner_enemy || !w->enemies[ei].active) continue;
                    if (vec3_dist(pr->pos, vec3_add(w->enemies[ei].pos, vec3_make(0, 0.9f, 0))) < 1.15f) {
                        world_damage_enemy(w, &w->enemies[ei], NULL, pr->owner_enemy, 18);
                        pr->active = false;
                        break;
                    }
                }
            }
        }
    }
}

static bool line_blocked_by_smoke(const GameWorld *w, Vec3 a, Vec3 b) {
    Vec3 ab = vec3_sub(b, a);
    float len_sq = vec3_len_sq(ab);
    if (len_sq < 1e-4f) return false;
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        if (!w->flares[i].active || !w->flares[i].is_smoke) continue;
        float t = sw_clampf(vec3_dot(vec3_sub(w->flares[i].pos, a), ab) / len_sq, 0.0f, 1.0f);
        Vec3 closest = vec3_add(a, vec3_scale(ab, t));
        if (vec3_dist(closest, w->flares[i].pos) < 3.6f) return true;
    }
    return false;
}

static void step_enemies(GameWorld *w, float dt) {
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        if (!e->active) continue;
        e->anim_time += dt;
        if (e->hit_flash > 0.0f) e->hit_flash -= dt;

        /* Day 5 Blood: Burn DoT damage! */
        if (e->burn_timer > 0.0f) {
            e->burn_timer -= dt;
            e->burn_tick -= dt;
            if (e->burn_tick <= 0.0f) {
                e->burn_tick = 0.50f;
                Player *bo = (e->burn_owner >= 0 && e->burn_owner < WORLD_MAX_PLAYERS) ? &w->players[e->burn_owner] : NULL;
                world_damage_enemy(w, e, bo, -1, 14);
                if (!e->active) continue;
            }
        }

        /* Day 3 CS: Flashbang blindness stun */
        if (e->blind_timer > 0.0f) {
            e->blind_timer -= dt;
            continue;
        }
        if (e->shrunk_timer > 0.0f) {
            e->shrunk_timer -= dt;
            continue;
        }

        bool is_flyer = (e->kind == ENEMY_OCTABRAIN || e->kind == ENEMY_BAT ||
                         e->kind == ENEMY_GARGOYLE || e->kind == ENEMY_CACODEMON);
        if (is_flyer) {
            e->pos.y = e->base_y + 0.45f * sinf(e->anim_time * 2.6f);
        }

        Vec3 target = e->pos;
        bool has_target = false;
        bool target_is_enemy = false;

        /* Doom Monster Infighting target takes priority! */
        if (e->infight_target >= 0 && e->infight_target < WORLD_MAX_ENEMIES &&
            w->enemies[e->infight_target].active) {
            target = w->enemies[e->infight_target].pos;
            has_target = true;
            target_is_enemy = true;
        } else {
            e->infight_target = -1;
            if (w->decoy.active && vec3_dist(e->pos, w->decoy.pos) < 22.0f) {
                target = w->decoy.pos;
                has_target = true;
            } else {
                float best_d = world_enemy_is_boss(e->kind) ? 34.0f : (e->kind == ENEMY_SYNDICATE_SNIPER ? 36.0f : 23.0f);
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
        }
        if (!has_target) continue;
        if (!target_is_enemy && line_blocked_by_smoke(w, e->pos, target)) continue;

        Vec3 delta = vec3_sub(target, e->pos);
        float dist = vec3_len(delta);
        e->yaw = atan2f(delta.x, -delta.z);

        bool is_melee = (e->kind == ENEMY_WOLF || e->kind == ENEMY_FIEND ||
                         e->kind == ENEMY_GHOUL || e->kind == ENEMY_PINKY);
        float stop_dist = is_melee ? 1.5f : 4.5f;

        if (dist > stop_dist && !world_enemy_is_boss(e->kind) && e->kind != ENEMY_TURRET && e->kind != ENEMY_SYNDICATE_SNIPER) {
            Vec3 dir = vec3_norm(vec3_make(delta.x, 0.0f, delta.z));
            float spd = is_melee ? 3.8f : (e->kind == ENEMY_TROOPER ? 2.6f : 2.1f);
            Vec3 next = vec3_add(e->pos, vec3_scale(dir, spd * dt));
            bool blocked = voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y + 0.5f), (int)floorf(next.z));
            bool has_floor = is_flyer ||
                             voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y - 0.2f), (int)floorf(next.z));
            if (!blocked && has_floor) {
                e->pos.x = next.x;
                e->pos.z = next.z;
            }
        }

        e->fire_cooldown -= dt;
        if (is_melee) {
            if (e->fire_cooldown <= 0.0f && dist < 2.2f) {
                e->fire_cooldown = 0.95f;
                if (target_is_enemy && e->infight_target >= 0) {
                    world_damage_enemy(w, &w->enemies[e->infight_target], NULL, i, 22);
                } else {
                    for (int pi = 0; pi < w->player_count; ++pi) {
                        if (w->players[pi].active && vec3_dist(e->pos, w->players[pi].pos) < 2.2f) {
                            damage_player(w, &w->players[pi], 10);
                        }
                    }
                }
            }
            continue;
        }

        if (e->fire_cooldown <= 0.0f && dist < 28.0f) {
            if (world_enemy_is_boss(e->kind)) {
                e->fire_cooldown = (w->boss_phase == 3) ? 0.65f : 1.10f;
            } else if (e->kind == ENEMY_TURRET) {
                e->fire_cooldown = 0.85f;
            } else {
                e->fire_cooldown = 1.55f;
            }
            Vec3 origin = vec3_add(e->pos, vec3_make(0.0f, 1.3f, 0.0f));
            Vec3 pdir = vec3_norm(vec3_sub(vec3_add(target, vec3_make(0, 1.0f, 0)), origin));
            if (e->kind == ENEMY_OGRE || e->kind == ENEMY_CULTIST) {
                alloc_proj(w, PROJ_ENEMY_GRENADE, -1, i, origin,
                           vec3_add(vec3_scale(pdir, 10.5f), vec3_make(0, 3.2f, 0)), 1.4f);
            } else {
                ProjectileKind pk = world_enemy_is_boss(e->kind) ? PROJ_BOSS_ROCKET :
                                    (e->kind == ENEMY_OCTABRAIN ? PROJ_OCTA_WAVE :
                                    ((e->kind == ENEMY_IMP || e->kind == ENEMY_CACODEMON) ? PROJ_ENEMY_FIREBALL : PROJ_ENEMY_PLASMA));
                alloc_proj(w, pk, -1, i, origin, vec3_scale(pdir, 13.0f), 2.8f);
            }
        }
    }
}

static void rebuild_dynamic_lights(GameWorld *w) {
    w->light_count = 0;
    float flick = 0.88f + 0.12f * sinf(w->time_sec * 13.0f) * cosf(w->time_sec * 7.0f);

    if (w->scene == SCENE_HUB) {
        world_add_light(w, vec3_make(64.0f, 3.5f, 31.0f), 1.0f, 0.48f, 0.14f, 3.0f * flick, 28.0f);
        int shards = swieta_progress_count_shards(w->shard_mask);
        world_add_light(w, vec3_make(64.5f, 15.5f, 64.5f), 1.0f, 0.86f, 0.40f, 1.8f + 0.08f * (float)shards, 34.0f);
        world_add_light(w, vec3_make(31.0f, 3.5f, 84.5f), 1.0f, 0.25f, 0.78f, 2.0f, 16.0f);
        world_add_light(w, vec3_make(64.5f, 3.2f, 52.0f), 0.2f, 0.92f, 1.0f, 1.4f, 14.0f);
        world_add_light(w, vec3_make(45.0f, 9.0f, 64.0f), 1.0f, 0.76f, 0.46f, 1.4f, 24.0f);
        world_add_light(w, vec3_make(83.0f, 9.0f, 64.0f), 1.0f, 0.76f, 0.46f, 1.4f, 24.0f);
    } else if (w->scene == SCENE_DAY01) {
        world_add_light(w, vec3_make(28.0f, 6.5f, 75.0f), 1.0f, 0.24f, 0.78f, 2.8f, 26.0f);
        world_add_light(w, vec3_make(19.5f, 5.5f, 96.0f), 0.18f, 0.92f, 1.0f, 2.0f, 18.0f);
        world_add_light(w, vec3_make(45.0f, 4.5f, 96.0f), 0.25f, 0.65f, 1.0f, 1.8f, 16.0f);
        world_add_light(w, vec3_make(32.0f, 5.5f, 46.0f),
                        w->cinema_restored ? 1.0f : 0.45f, w->cinema_restored ? 0.88f : 0.55f,
                        w->cinema_restored ? 0.35f : 0.78f, w->cinema_restored ? 2.5f : 1.3f * flick, 24.0f);
        world_add_light(w, vec3_make(15.5f, 3.2f, 65.0f), 0.35f, 0.96f, 1.0f, 1.7f, 14.0f);
        world_add_light(w, vec3_make(53.5f, 3.2f, 52.0f), 1.0f, 0.28f, 0.85f, 1.8f, 14.0f);
        world_add_light(w, vec3_make(78.0f, 12.0f, 58.0f), 1.0f, 0.25f, 0.55f, 2.2f, 24.0f);
        world_add_light(w, vec3_make(110.0f, 14.5f, 54.0f), 1.0f, 0.22f, 0.22f, 2.2f * flick, 24.0f);
        world_add_light(w, vec3_make(50.0f, 16.5f, 17.0f), 1.0f, 0.32f, 0.20f, 2.6f * flick, 32.0f);
    } else if (w->scene >= SCENE_DAY02 && w->scene <= SCENE_DAY06) {
        act1_rebuild_lights(w, flick);
    } else {
        const PortalMeta *meta = swieta_portal_meta(w->active_day);
        world_add_light(w, vec3_make(64.0f, 6.5f, 62.0f), meta ? meta->color_r : 0.8f,
                        meta ? meta->color_g : 0.8f, meta ? meta->color_b : 1.0f, 2.5f * flick, 26.0f);
    }

    /* Dynamic player lights (Quad Damage blue aura + muzzle flash) */
    for (int i = 0; i < w->player_count; ++i) {
        if (!w->players[i].active) continue;
        if (w->players[i].quad_timer > 0.0f) {
            world_add_light(w, player_eye_pos(&w->players[i]), 0.22f, 0.55f, 1.0f, 2.6f, 16.0f);
        } else if (w->players[i].muzzle_flash > 0.0f) {
            world_add_light(w, player_eye_pos(&w->players[i]), 1.0f, 0.85f, 0.4f, 2.4f, 10.0f);
        }
    }
    /* Thrown green Magnesium Flares! */
    for (int i = 0; i < WORLD_MAX_FLARES; ++i) {
        if (w->flares[i].active && !w->flares[i].is_smoke) {
            world_add_light(w, w->flares[i].pos, w->flares[i].r, w->flares[i].g, w->flares[i].b, 2.8f * flick, 16.0f);
        }
    }
    /* Burning enemies (Blood Flare Gun / Molotov) cast live firelight! */
    for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
        if (w->enemies[i].active && w->enemies[i].burn_timer > 0.0f) {
            world_add_light(w, vec3_add(w->enemies[i].pos, vec3_make(0, 1.1f, 0)), 1.0f, 0.45f, 0.12f, 2.2f * flick, 12.0f);
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (w->explosions[i].active) {
            world_add_light(w, w->explosions[i].pos, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b,
                            3.4f * (w->explosions[i].timer / 0.45f), 14.0f);
        }
    }
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        if (w->projectiles[i].active && (w->projectiles[i].kind == PROJ_SHRINK_ORB || w->projectiles[i].kind == PROJ_BFG_ORB)) {
            world_add_light(w, w->projectiles[i].pos, 0.2f, 1.0f, 0.35f, 2.2f, 12.0f);
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
        step_flares(w, dt);
        step_tripmines(w, dt);
        step_projectiles(w, dt);
        step_enemies(w, dt);
        act1_step_level(w, dt);
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
