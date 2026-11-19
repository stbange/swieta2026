#include "sim/world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float kPlayerRadius = 0.38f;
static const float kPlayerHeight = 1.72f;
static const float kPlayerEyeHeight = 1.52f;
static const float kGravity = 22.0f;

static void set_banner(GameWorld *w, const char *msg, float duration) {
    if (!w || !msg) return;
    snprintf(w->banner, sizeof(w->banner), "%s", msg);
    w->banner_timer = duration;
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

Vec3 player_eye_pos(const Player *p) {
    return vec3_make(p->pos.x, p->pos.y + kPlayerEyeHeight, p->pos.z);
}

Vec3 player_forward(const Player *p) {
    float cp = cosf(p->pitch);
    return vec3_make(sinf(p->yaw) * cp, sinf(p->pitch), -cosf(p->yaw) * cp);
}

static void reset_player_for_spawn(Player *p, Vec3 pos, float yaw, bool keep_score) {
    int prev_score = keep_score ? p->score : 0;
    int prev_stomps = keep_score ? p->stomps : 0;
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->pos = pos;
    p->yaw = yaw;
    p->pitch = 0.0f;
    p->hp = 100;
    p->max_hp = 100;
    p->armor = 25;
    p->weapon = WEAP_PISTOL;
    p->ammo_shrink = 12;
    p->ammo_pipebomb = 6;
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
            if (kind == ENEMY_TROOPER) {
                e->max_hp = 40;
            } else if (kind == ENEMY_PIGGUARD) {
                e->max_hp = 80;
            } else {
                e->max_hp = 180;
            }
            e->hp = e->max_hp;
            e->fire_cooldown = 1.0f + (float)(i % 3) * 0.35f;
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
            w->pickups[i].bob_phase = (float)i * 0.7f;
            return;
        }
    }
}

static void clear_entities(GameWorld *w) {
    memset(w->enemies, 0, sizeof(w->enemies));
    memset(w->projectiles, 0, sizeof(w->projectiles));
    memset(w->pickups, 0, sizeof(w->pickups));
    memset(&w->decoy, 0, sizeof(w->decoy));
    memset(w->explosions, 0, sizeof(w->explosions));
}

static void setup_hub_portals(GameWorld *w) {
    /* Act I (Days 1..6): West Wall (x=4.5), Day 1 closest to spawn */
    static const float west_z[6] = {36.5f, 30.5f, 24.5f, 18.5f, 12.5f, 6.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[i].day = i + 1;
        w->portals[i].pos = vec3_make(4.5f, 1.0f, west_z[i]);
        w->portals[i].facing_yaw = SW_PI * 0.5f;
    }
    /* Act II (Days 7..12): East Wall (x=43.5) */
    static const float east_z[6] = {36.5f, 30.5f, 24.5f, 18.5f, 12.5f, 6.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[6 + i].day = 7 + i;
        w->portals[6 + i].pos = vec3_make(43.5f, 1.0f, east_z[i]);
        w->portals[6 + i].facing_yaw = -SW_PI * 0.5f;
    }
    /* Act III (Days 13..18): South Wall (z=43.5) */
    static const float south_x[6] = {9.5f, 15.5f, 21.5f, 27.5f, 33.5f, 39.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[12 + i].day = 13 + i;
        w->portals[12 + i].pos = vec3_make(south_x[i], 1.0f, 43.5f);
        w->portals[12 + i].facing_yaw = 0.0f;
    }
    /* Act IV (Days 19..24): North Wall flanking Fireplace (z=4.5) */
    static const float north_x[6] = {8.5f, 13.5f, 18.5f, 30.5f, 35.5f, 40.5f};
    for (int i = 0; i < 6; ++i) {
        w->portals[18 + i].day = 19 + i;
        w->portals[18 + i].pos = vec3_make(north_x[i], 1.0f, 4.5f);
        w->portals[18 + i].facing_yaw = SW_PI;
    }
}

void world_load_hub(GameWorld *w) {
    if (!w) return;
    w->scene = SCENE_HUB;
    w->active_day = 0;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    /* Floor & ceiling */
    voxel_fill_box(&w->voxels, 2, 0, 2, 45, 0, 45, VOX_STONE);
    voxel_fill_box(&w->voxels, 22, 0, 5, 26, 0, 42, VOX_CARPET);
    voxel_fill_box(&w->voxels, 5, 0, 22, 42, 0, 26, VOX_CARPET);

    /* Outer festive timber/stone hall walls */
    voxel_fill_box(&w->voxels, 2, 1, 2, 45, 11, 2, VOX_STONE);
    voxel_fill_box(&w->voxels, 2, 1, 45, 45, 11, 45, VOX_STONE);
    voxel_fill_box(&w->voxels, 2, 1, 2, 2, 11, 45, VOX_STONE);
    voxel_fill_box(&w->voxels, 45, 1, 2, 45, 11, 45, VOX_STONE);
    voxel_fill_box(&w->voxels, 2, 12, 2, 45, 12, 45, VOX_WOOD);

    /* Decorative timber ribs & pillars */
    for (int p = 10; p <= 38; p += 14) {
        voxel_fill_box(&w->voxels, 12, 1, p, 13, 11, p + 1, VOX_WOOD);
        voxel_fill_box(&w->voxels, 34, 1, p, 35, 11, p + 1, VOX_WOOD);
    }

    /* Grand Fireplace on North wall (x=22..26, z=3..4) */
    voxel_fill_box(&w->voxels, 21, 1, 3, 27, 6, 4, VOX_BRICK);
    voxel_fill_box(&w->voxels, 22, 1, 4, 26, 3, 4, VOX_AIR);
    voxel_fill_box(&w->voxels, 23, 1, 4, 25, 2, 4, VOX_FIRE);
    voxel_fill_box(&w->voxels, 21, 4, 5, 27, 4, 5, VOX_WOOD); /* Mantle */

    /* Central 24-Ornament Voxel Christmas Tree at (24, y, 24) */
    voxel_fill_box(&w->voxels, 23, 1, 23, 24, 2, 24, VOX_WOOD);
    voxel_fill_box(&w->voxels, 21, 3, 21, 26, 4, 26, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 22, 5, 22, 25, 6, 25, VOX_LEAVES);
    voxel_fill_box(&w->voxels, 23, 7, 23, 24, 8, 24, VOX_LEAVES);
    voxel_set(&w->voxels, 24, 9, 24, VOX_GOLD);

    /* Light up golden ornaments on the tree for collected shards */
    int shards = swieta_progress_count_shards(w->shard_mask);
    if (shards > 0) {
        voxel_set(&w->voxels, 21, 3, 23, VOX_GOLD);
        voxel_set(&w->voxels, 26, 3, 24, VOX_NEON_PINK);
        voxel_set(&w->voxels, 23, 5, 22, VOX_NEON_CYAN);
        voxel_set(&w->voxels, 24, 7, 23, VOX_GOLD);
    }

    /* SANTA-BIOS Terminal desk */
    w->santa_bios_pos = vec3_make(24.0f, 1.0f, 17.0f);
    voxel_fill_box(&w->voxels, 23, 1, 16, 25, 1, 17, VOX_METAL);
    voxel_set(&w->voxels, 24, 2, 16, VOX_NEON_CYAN);

    /* Portal stone arch frames for all 24 doors (generous 2-voxel wide openings) */
    setup_hub_portals(w);
    for (int i = 0; i < SWIETA_PORTAL_COUNT; ++i) {
        int px = (int)floorf(w->portals[i].pos.x);
        int pz = (int)floorf(w->portals[i].pos.z);
        if (px <= 5 || px >= 42) {
            voxel_fill_box(&w->voxels, px, 1, pz - 1, px, 4, pz - 1, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px, 1, pz + 2, px, 4, pz + 2, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px, 4, pz - 1, px, 4, pz + 2, VOX_PORTAL_FRAME);
        } else {
            voxel_fill_box(&w->voxels, px - 1, 1, pz, px - 1, 4, pz, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px + 2, 1, pz, px + 2, 4, pz, VOX_PORTAL_FRAME);
            voxel_fill_box(&w->voxels, px - 1, 4, pz, px + 2, 4, pz, VOX_PORTAL_FRAME);
        }
    }

    /* Spawn players near South-Center carpet facing tree & portals */
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(22.5f + (float)i * 1.3f, 1.01f, 34.0f);
            reset_player_for_spawn(&w->players[i], sp, 0.0f, true);
        } else {
            w->players[i].active = false;
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "GRAND FESTIVE HALL | SHARDS: %d/24 | WALK INTO DAY 1 PORTAL (WEST)", shards);
    set_banner(w, msg, 5.0f);
}

static void build_day01_duke(GameWorld *w) {
    w->scene = SCENE_DAY01;
    w->active_day = 1;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    w->cinema_restored = false;
    w->secret_wall_blasted = false;
    w->boss_defeated = false;
    w->nuke_exit_ready = false;

    /* Snowy asphalt boulevard */
    voxel_fill_box(&w->voxels, 2, 0, 2, 45, 0, 45, VOX_ASPHALT);
    voxel_fill_box(&w->voxels, 2, 0, 32, 45, 0, 44, VOX_SNOW);
    voxel_fill_box(&w->voxels, 2, 0, 2, 45, 0, 8, VOX_SNOW);

    /* Outer city perimeter walls */
    voxel_fill_box(&w->voxels, 2, 1, 2, 45, 10, 2, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 45, 45, 10, 45, VOX_BRICK);
    voxel_fill_box(&w->voxels, 2, 1, 2, 2, 10, 45, VOX_BRICK);
    voxel_fill_box(&w->voxels, 45, 1, 2, 45, 10, 45, VOX_BRICK);

    /* Yule Cinema Building (x=12..26, z=14..26, y=1..6) */
    voxel_fill_box(&w->voxels, 12, 1, 14, 26, 6, 26, VOX_BRICK);
    voxel_fill_box(&w->voxels, 13, 1, 15, 25, 5, 25, VOX_AIR); /* Interior */
    voxel_fill_box(&w->voxels, 12, 6, 14, 38, 6, 26, VOX_STONE); /* Roof terrace extending east */
    voxel_fill_box(&w->voxels, 17, 1, 26, 21, 3, 26, VOX_AIR); /* Front entrance */

    /* Neon marquee signs */
    voxel_fill_box(&w->voxels, 15, 5, 27, 23, 5, 27, VOX_NEON_PINK);
    voxel_fill_box(&w->voxels, 16, 4, 27, 22, 4, 27, VOX_NEON_CYAN);

    /* Cinema screen showing KRAMPUS grey static until projector restored */
    voxel_fill_box(&w->voxels, 15, 2, 15, 23, 4, 15, VOX_CINEMA_STATIC);
    w->cinema_switch_pos = vec3_make(19.0f, 1.5f, 19.0f);
    voxel_set(&w->voxels, 19, 1, 19, VOX_GOLD);

    /* Cracked Backstage Voxel Wall (x=26, y=1..3, z=19..21) -> blast with Pipebomb! */
    voxel_fill_box(&w->voxels, 26, 1, 19, 26, 3, 21, VOX_CRACKED);

    /* Secret Retro Arcade behind cracked wall (x=27..34, z=16..24) */
    voxel_fill_box(&w->voxels, 27, 1, 16, 35, 5, 24, VOX_STONE);
    voxel_fill_box(&w->voxels, 27, 1, 17, 34, 5, 23, VOX_AIR);
    voxel_set(&w->voxels, 33, 2, 18, VOX_NEON_CYAN);
    voxel_set(&w->voxels, 33, 2, 22, VOX_NEON_PINK);

    /* Street-side crates & stair ramp up to Roof Arena (x=36..41) */
    voxel_fill_box(&w->voxels, 9, 1, 30, 10, 2, 31, VOX_WOOD);
    for (int step = 0; step < 6; ++step) {
        voxel_fill_box(&w->voxels, 39, 1, 29 - step, 41, 1 + step, 29 - step, VOX_METAL);
    }

    /* Rooftop Arena guardrails & Nuke-Exit pedestal */
    voxel_fill_box(&w->voxels, 12, 7, 14, 38, 7, 14, VOX_METAL);
    w->nuke_switch_pos = vec3_make(36.0f, 7.5f, 18.0f);
    voxel_set(&w->voxels, 36, 7, 18, VOX_NEON_PINK);

    /* Return portal at spawn alley */
    w->return_portal_pos = vec3_make(4.5f, 1.0f, 40.0f);

    /* Enemies */
    add_enemy(w, ENEMY_TROOPER,   vec3_make(14.0f, 1.0f, 34.0f));
    add_enemy(w, ENEMY_TROOPER,   vec3_make(24.0f, 1.0f, 32.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(19.0f, 1.0f, 22.0f));
    add_enemy(w, ENEMY_TROOPER,   vec3_make(32.0f, 1.0f, 28.0f));
    add_enemy(w, ENEMY_PIGGUARD,  vec3_make(22.0f, 7.0f, 22.0f));
    add_enemy(w, ENEMY_BOSS_MECH, vec3_make(30.0f, 7.0f, 19.0f));

    /* Pickups: Street + Secret Arcade + Roof */
    add_pickup(w, PICKUP_SHRINK_AMMO, vec3_make(11.0f, 1.2f, 35.0f));
    add_pickup(w, PICKUP_PIPEBOMB,    vec3_make(16.0f, 1.2f, 24.0f));
    add_pickup(w, PICKUP_MEDKIT,      vec3_make(24.0f, 1.2f, 17.0f));
    add_pickup(w, PICKUP_ARMOR,       vec3_make(31.0f, 1.2f, 20.0f)); /* Behind cracked wall */
    add_pickup(w, PICKUP_JETPACK,     vec3_make(36.0f, 1.2f, 34.0f));

    /* Spawn players in snowy alley facing Neon Strip */
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(6.0f + (float)i * 1.1f, 1.01f, 38.0f);
            reset_player_for_spawn(&w->players[i], sp, SW_PI * 0.35f, true);
        } else {
            w->players[i].active = false;
        }
    }

    set_banner(w, "DAY 1: NEON STRIP '96 | RESTORE CINEMA, BLAST CRACKED WALL, DEFEAT ROOF BOSS!", 6.0f);
}

static void build_preview_chamber(GameWorld *w, int day) {
    w->scene = SCENE_PREVIEW;
    w->active_day = day;
    clear_entities(w);
    voxel_world_clear(&w->voxels);

    voxel_fill_box(&w->voxels, 14, 0, 14, 34, 0, 34, VOX_STONE);
    voxel_fill_box(&w->voxels, 14, 1, 14, 34, 7, 14, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 14, 1, 34, 34, 7, 34, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 14, 1, 14, 14, 7, 34, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 34, 1, 14, 34, 7, 34, VOX_PORTAL_FRAME);
    voxel_fill_box(&w->voxels, 14, 8, 14, 34, 8, 34, VOX_WOOD);
    voxel_fill_box(&w->voxels, 23, 1, 20, 25, 2, 22, VOX_GOLD);

    w->return_portal_pos = vec3_make(24.0f, 1.0f, 31.0f);
    add_pickup(w, PICKUP_SHARD, vec3_make(24.0f, 3.0f, 21.0f));

    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) {
        if (i < w->player_count) {
            Vec3 sp = vec3_make(23.0f + (float)i * 1.1f, 1.01f, 28.0f);
            reset_player_for_spawn(&w->players[i], sp, 0.0f, true);
        } else {
            w->players[i].active = false;
        }
    }

    const PortalMeta *meta = swieta_portal_meta(day);
    char msg[128];
    snprintf(msg, sizeof(msg), "DAY %02d PREVIEW: %s (%s) | RETURN PORTAL BEHIND YOU",
             day, meta ? meta->portal_title : "?", meta ? meta->inspiration : "?");
    set_banner(w, msg, 6.0f);
}

void world_load_day(GameWorld *w, int day) {
    if (!w || day < 1 || day > SWIETA_PORTAL_COUNT) return;
    if (!swieta_portal_is_unlocked(day, w->now, w->unlock_all)) {
        set_banner(w, "PORTAL FROZEN BY KRAMPUS.EXE! WAIT FOR ADVENT DAY OR ENABLE PLAYTEST MODE", 3.5f);
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

static bool player_collides(const VoxelWorld *vw, Vec3 pos) {
    int x0 = (int)floorf(pos.x - kPlayerRadius);
    int x1 = (int)floorf(pos.x + kPlayerRadius);
    int y0 = (int)floorf(pos.y + 0.05f);
    int y1 = (int)floorf(pos.y + kPlayerHeight);
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

static void damage_enemy(GameWorld *w, Enemy *e, Player *attacker, int dmg) {
    if (!e->active || dmg <= 0) return;
    e->hp -= dmg;
    e->hit_flash = 0.18f;
    if (e->hp <= 0) {
        e->active = false;
        if (attacker) {
            attacker->score += (e->kind == ENEMY_BOSS_MECH) ? 500 : 100;
        }
        spawn_explosion(w, vec3_add(e->pos, vec3_make(0, 0.9f, 0)), 1.6f, 1.0f, 0.55f, 0.15f);
        if (e->kind == ENEMY_BOSS_MECH) {
            w->boss_defeated = true;
            w->sound_events |= SFX_EXPLODE;
            set_banner(w, "ROOFTOP BATTLE-MECH DESTROYED! HIT THE RED NUKE-EXIT SWITCH!", 5.0f);
        }
    }
}

static void fire_player_weapon(GameWorld *w, int pi, Player *p) {
    Vec3 eye = player_eye_pos(p);
    Vec3 fwd = player_forward(p);

    if (p->weapon == WEAP_PISTOL) {
        p->fire_cooldown = 0.22f;
        p->muzzle_flash = 0.10f;
        w->sound_events |= SFX_PISTOL;
        float wall_dist = 40.0f;
        voxel_raycast(&w->voxels, eye, fwd, 40.0f, &wall_dist, NULL);

        int best_enemy = -1;
        float best_t = wall_dist;
        for (int i = 0; i < WORLD_MAX_ENEMIES; ++i) {
            Enemy *e = &w->enemies[i];
            if (!e->active) continue;
            float rad = (e->kind == ENEMY_BOSS_MECH) ? 1.15f : (e->shrunk_timer > 0.0f ? 0.35f : 0.65f);
            float h = (e->kind == ENEMY_BOSS_MECH) ? 2.5f : (e->shrunk_timer > 0.0f ? 0.6f : 1.75f);
            AABB box = {
                vec3_make(e->pos.x - rad, e->pos.y, e->pos.z - rad),
                vec3_make(e->pos.x + rad, e->pos.y + h, e->pos.z + rad)
            };
            float t = 0.0f;
            if (ray_aabb_hit(eye, fwd, box, best_t, &t)) {
                best_t = t;
                best_enemy = i;
            }
        }
        if (best_enemy >= 0) {
            damage_enemy(w, &w->enemies[best_enemy], p, 22);
        }
    } else if (p->weapon == WEAP_SHRINKER && p->ammo_shrink > 0) {
        p->ammo_shrink--;
        p->fire_cooldown = 0.45f;
        p->muzzle_flash = 0.14f;
        w->sound_events |= SFX_SHRINK;
        for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
            if (!w->projectiles[i].active) {
                Projectile *pr = &w->projectiles[i];
                pr->active = true;
                pr->kind = PROJ_SHRINK_ORB;
                pr->owner_player = pi;
                pr->pos = vec3_add(eye, vec3_scale(fwd, 0.7f));
                pr->vel = vec3_scale(fwd, 18.0f);
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
                pr->active = true;
                pr->kind = PROJ_PIPEBOMB;
                pr->owner_player = pi;
                pr->pos = vec3_add(eye, vec3_scale(fwd, 0.6f));
                pr->vel = vec3_add(vec3_scale(fwd, 11.0f), vec3_make(0.0f, 3.5f, 0.0f));
                pr->life = 30.0f;
                break;
            }
        }
    }
}

static void detonate_pipebombs(GameWorld *w, int owner_player) {
    bool any = false;
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        Projectile *pr = &w->projectiles[i];
        if (!pr->active || pr->kind != PROJ_PIPEBOMB || pr->owner_player != owner_player) {
            continue;
        }
        any = true;
        pr->active = false;
        spawn_explosion(w, pr->pos, 3.8f, 1.0f, 0.45f, 0.12f);

        int destroyed = voxel_explode_cracked(&w->voxels, pr->pos, 3.4f);
        if (destroyed > 0 && !w->secret_wall_blasted) {
            w->secret_wall_blasted = true;
            w->players[owner_player].score += 250;
            set_banner(w, "SECRET RETRO ARCADE BLASTED OPEN! (+250)", 4.0f);
        }

        for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
            Enemy *e = &w->enemies[ei];
            if (!e->active) continue;
            float d = vec3_dist(pr->pos, vec3_add(e->pos, vec3_make(0.0f, 0.9f, 0.0f)));
            if (d < 4.5f) {
                int dmg = (int)(95.0f * (1.0f - d / 4.8f));
                damage_enemy(w, e, &w->players[owner_player], dmg);
            }
        }
    }
    if (any) {
        w->sound_events |= SFX_EXPLODE;
    }
}

static void handle_player_use(GameWorld *w, Player *p) {
    if (w->scene == SCENE_HUB) {
        if (vec3_dist(p->pos, w->santa_bios_pos) < 3.0f) {
            w->sound_events |= SFX_SWITCH;
            int shards = swieta_progress_count_shards(w->shard_mask);
            char msg[128];
            snprintf(msg, sizeof(msg), "SANTA-BIOS: NODE-24 ONLINE! %d/24 SHARDS RESTORED. STOP KRAMPUS.EXE!", shards);
            set_banner(w, msg, 4.5f);
        }
    } else if (w->scene == SCENE_DAY01) {
        if (!w->cinema_restored && vec3_dist(p->pos, w->cinema_switch_pos) < 3.0f) {
            w->cinema_restored = true;
            voxel_replace_all(&w->voxels, VOX_CINEMA_STATIC, VOX_CINEMA_LIT);
            p->score += 200;
            w->sound_events |= SFX_SWITCH;
            set_banner(w, "YULE CINEMA PROJECTOR RESTORED! HEAD TO THE ROOFTOP ARENA!", 4.5f);
        } else if (vec3_dist(p->pos, w->nuke_switch_pos) < 3.0f) {
            if (!w->cinema_restored || !w->boss_defeated) {
                set_banner(w, "RESTORE THE CINEMA PROJECTOR & DEFEAT THE ROOFTOP BOSS FIRST!", 3.5f);
            } else {
                w->nuke_exit_ready = true;
                w->shard_mask |= (1u << 0);
                p->score += 1000;
                w->sound_events |= SFX_SHARD;
                world_load_hub(w);
                set_banner(w, "DAY 1 COMPLETE! GOLDEN SHADES SHARD SOCKETED INTO THE GRAND TREE!", 6.0f);
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
            Vec3 sp = (w->scene == SCENE_DAY01) ? vec3_make(6.0f, 1.01f, 38.0f) : vec3_make(24.0f, 1.01f, 34.0f);
            reset_player_for_spawn(p, sp, p->yaw, true);
            set_banner(w, "RESPAWNED! GET BACK IN THE FIGHT!", 2.5f);
        }
        return;
    }

    p->yaw = sw_wrap_angle(p->yaw + inp->look_dx);
    p->pitch = sw_clampf(p->pitch - inp->look_dy, -1.35f, 1.35f);

    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;
    if (p->holo_cooldown > 0.0f) p->holo_cooldown -= dt;
    if (p->stomp_flash > 0.0f) p->stomp_flash -= dt;
    if (p->damage_flash > 0.0f) p->damage_flash -= dt;
    if (p->muzzle_flash > 0.0f) p->muzzle_flash -= dt;

    uint16_t pressed = (uint16_t)(inp->buttons & (uint16_t)(~p->prev_buttons));
    p->prev_buttons = inp->buttons;

    if (inp->select_weapon >= 0 && inp->select_weapon < WEAP_COUNT) {
        p->weapon = (WeaponId)inp->select_weapon;
    } else if (pressed & INP_NEXT_WEAP) {
        p->weapon = (WeaponId)((p->weapon + 1) % WEAP_COUNT);
    }

    /* Movement in XZ */
    Vec3 fwd_xz = vec3_make(sinf(p->yaw), 0.0f, -cosf(p->yaw));
    Vec3 right_xz = vec3_make(cosf(p->yaw), 0.0f, sinf(p->yaw));
    float speed = (inp->buttons & INP_SPRINT) ? 9.2f : 6.4f;
    Vec3 wish = vec3_add(vec3_scale(fwd_xz, sw_clampf(inp->move_y, -1.0f, 1.0f)),
                         vec3_scale(right_xz, sw_clampf(inp->move_x, -1.0f, 1.0f)));
    if (vec3_len_sq(wish) > 1.0f) {
        wish = vec3_norm(wish);
    }
    p->vel.x = wish.x * speed;
    p->vel.z = wish.z * speed;

    /* Jump & Jetpack */
    p->jetpack_active = false;
    if ((pressed & INP_JUMP) && p->on_ground) {
        p->vel.y = 8.4f;
        p->on_ground = false;
    } else if (((inp->buttons & INP_JETPACK) || ((inp->buttons & INP_JUMP) && !p->on_ground && p->vel.y < 1.0f)) &&
               p->jetpack_fuel > 1.0f && w->scene == SCENE_DAY01) {
        p->vel.y = sw_lerpf(p->vel.y, 6.2f, 0.25f);
        p->jetpack_fuel = sw_clampf(p->jetpack_fuel - 26.0f * dt, 0.0f, 100.0f);
        p->jetpack_active = true;
        w->sound_events |= SFX_JETPACK;
    } else {
        p->vel.y -= kGravity * dt;
        if (p->on_ground) {
            p->jetpack_fuel = sw_clampf(p->jetpack_fuel + 12.0f * dt, 0.0f, 100.0f);
        }
    }

    /* Axis-separated voxel collision + auto step-up */
    Vec3 try_x = vec3_make(p->pos.x + p->vel.x * dt, p->pos.y, p->pos.z);
    if (!player_collides(&w->voxels, try_x)) {
        p->pos.x = try_x.x;
    } else if (p->on_ground && !player_collides(&w->voxels, vec3_make(try_x.x, p->pos.y + 1.05f, p->pos.z))) {
        p->pos.x = try_x.x;
        p->pos.y += 1.02f;
    }

    Vec3 try_z = vec3_make(p->pos.x, p->pos.y, p->pos.z + p->vel.z * dt);
    if (!player_collides(&w->voxels, try_z)) {
        p->pos.z = try_z.z;
    } else if (p->on_ground && !player_collides(&w->voxels, vec3_make(p->pos.x, p->pos.y + 1.05f, try_z.z))) {
        p->pos.z = try_z.z;
        p->pos.y += 1.02f;
    }

    Vec3 try_y = vec3_make(p->pos.x, p->pos.y + p->vel.y * dt, p->pos.z);
    if (!player_collides(&w->voxels, try_y)) {
        p->pos.y = try_y.y;
        p->on_ground = false;
    } else {
        if (p->vel.y < 0.0f) {
            p->pos.y = floorf(p->pos.y) + 0.005f;
            p->on_ground = true;
        }
        p->vel.y = 0.0f;
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
        w->decoy.pos = vec3_add(p->pos, vec3_scale(fwd_xz, 1.5f));
        w->decoy.timer = 8.0f;
        p->holo_cooldown = 10.0f;
        w->sound_events |= SFX_SWITCH;
        set_banner(w, "HOLO-DECOY DEPLOYED! ALIENS DISTRACTED!", 2.5f);
    }
    if (pressed & INP_USE) {
        handle_player_use(w, p);
    }

    /* Mighty Boot Stomp on shrunk enemies */
    for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
        Enemy *e = &w->enemies[ei];
        if (!e->active || e->shrunk_timer <= 0.0f) continue;
        if (vec3_dist(p->pos, e->pos) < 1.25f) {
            e->active = false;
            p->stomps++;
            p->score += 175;
            p->stomp_flash = 0.3f;
            w->sound_events |= SFX_STOMP;
            set_banner(w, "MIGHTY BOOT STOMP! SQUISHED! (+175)", 2.0f);
        }
    }

    /* Pickups */
    for (int ki = 0; ki < WORLD_MAX_PICKUPS; ++ki) {
        Pickup *pk = &w->pickups[ki];
        if (!pk->active) continue;
        if (vec3_dist(vec3_add(p->pos, vec3_make(0, 0.8f, 0)), pk->pos) < 1.45f) {
            pk->active = false;
            w->sound_events |= SFX_PICKUP;
            switch (pk->kind) {
                case PICKUP_MEDKIT:      p->hp = (p->hp + 35 > p->max_hp) ? p->max_hp : p->hp + 35; break;
                case PICKUP_ARMOR:       p->armor = (p->armor + 50 > 100) ? 100 : p->armor + 50; break;
                case PICKUP_SHRINK_AMMO: p->ammo_shrink += 8; break;
                case PICKUP_PIPEBOMB:    p->ammo_pipebomb += 4; break;
                case PICKUP_JETPACK:     p->jetpack_fuel = 100.0f; break;
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
        const PortalGate *gate = world_find_nearby_portal(w, p->pos, 1.35f);
        if (gate) {
            world_load_day(w, gate->day);
        }
    } else if (vec3_dist(p->pos, w->return_portal_pos) < 1.45f) {
        w->sound_events |= SFX_PORTAL;
        world_load_hub(w);
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
                if (vec3_len_sq(pr->vel) < 0.25f) {
                    pr->vel = vec3_make(0, 0, 0);
                }
            } else {
                pr->pos = next;
            }
            continue;
        }

        Vec3 next = vec3_add(pr->pos, vec3_scale(pr->vel, dt));
        if (voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y), (int)floorf(next.z))) {
            if (pr->kind == PROJ_BOSS_ROCKET) {
                spawn_explosion(w, pr->pos, 2.2f, 1.0f, 0.3f, 0.1f);
            }
            pr->active = false;
            continue;
        }
        pr->pos = next;

        if (pr->kind == PROJ_SHRINK_ORB) {
            for (int ei = 0; ei < WORLD_MAX_ENEMIES; ++ei) {
                Enemy *e = &w->enemies[ei];
                if (!e->active) continue;
                if (vec3_dist(pr->pos, vec3_add(e->pos, vec3_make(0, 0.9f, 0))) < 1.1f) {
                    pr->active = false;
                    if (e->kind == ENEMY_BOSS_MECH) {
                        damage_enemy(w, e, &w->players[pr->owner_player], 28);
                    } else {
                        e->shrunk_timer = 8.0f;
                        e->hit_flash = 0.25f;
                        set_banner(w, "ENEMY SHRUNK! WALK OVER THEM TO STOMP!", 2.5f);
                    }
                    break;
                }
            }
        } else {
            /* Enemy plasma / boss rocket */
            for (int pi = 0; pi < w->player_count; ++pi) {
                Player *p = &w->players[pi];
                if (!p->active || p->hp <= 0) continue;
                if (vec3_dist(pr->pos, vec3_add(p->pos, vec3_make(0, 1.0f, 0))) < 0.85f) {
                    damage_player(w, p, (pr->kind == PROJ_BOSS_ROCKET) ? 16 : 8);
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
            continue; /* Shrunk enemies scurry harmlessly */
        }

        /* Choose target: Holo-Decoy takes priority if within 16m */
        Vec3 target = e->pos;
        bool has_target = false;
        if (w->decoy.active && vec3_dist(e->pos, w->decoy.pos) < 16.0f) {
            target = w->decoy.pos;
            has_target = true;
        } else {
            float best_d = 18.0f;
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

        /* Gentle strafe/advance */
        if (dist > 4.2f && e->kind != ENEMY_BOSS_MECH) {
            Vec3 dir = vec3_norm(vec3_make(delta.x, 0.0f, delta.z));
            float spd = (e->kind == ENEMY_TROOPER) ? 2.4f : 1.8f;
            Vec3 next = vec3_add(e->pos, vec3_scale(dir, spd * dt));
            if (!voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y + 0.5f), (int)floorf(next.z)) &&
                voxel_is_solid(&w->voxels, (int)floorf(next.x), (int)floorf(next.y - 0.2f), (int)floorf(next.z))) {
                e->pos = next;
            }
        }

        e->fire_cooldown -= dt;
        if (e->fire_cooldown <= 0.0f && dist < 16.0f) {
            e->fire_cooldown = (e->kind == ENEMY_BOSS_MECH) ? 1.15f : 1.75f;
            for (int pi = 0; pi < WORLD_MAX_PROJECTILES; ++pi) {
                if (!w->projectiles[pi].active) {
                    Projectile *pr = &w->projectiles[pi];
                    pr->active = true;
                    pr->kind = (e->kind == ENEMY_BOSS_MECH) ? PROJ_BOSS_ROCKET : PROJ_ENEMY_PLASMA;
                    pr->owner_player = -1;
                    pr->pos = vec3_add(e->pos, vec3_make(0.0f, 1.3f, 0.0f));
                    pr->vel = vec3_scale(vec3_norm(vec3_sub(vec3_add(target, vec3_make(0, 1.0f, 0)), pr->pos)), 11.5f);
                    pr->life = 2.6f;
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
        /* Warm crackling fireplace */
        add_light(w, vec3_make(24.0f, 2.5f, 6.0f), 1.0f, 0.48f, 0.14f, 2.6f * flick, 18.0f);
        /* Golden Christmas tree star & glow */
        int shards = swieta_progress_count_shards(w->shard_mask);
        add_light(w, vec3_make(24.0f, 9.5f, 24.0f), 1.0f, 0.85f, 0.38f, 1.5f + 0.08f * (float)shards, 20.0f);
        /* Day 1 Neon Portal Beacon */
        add_light(w, vec3_make(5.5f, 2.5f, 36.0f), 1.0f, 0.25f, 0.72f, 1.6f, 10.0f);
        /* SANTA-BIOS cyan terminal */
        add_light(w, vec3_make(24.0f, 2.8f, 17.5f), 0.2f, 0.9f, 1.0f, 1.1f, 8.0f);
        /* Wing warm sconces */
        add_light(w, vec3_make(12.0f, 6.0f, 24.0f), 1.0f, 0.75f, 0.45f, 1.1f, 14.0f);
        add_light(w, vec3_make(36.0f, 6.0f, 24.0f), 1.0f, 0.75f, 0.45f, 1.1f, 14.0f);
    } else if (w->scene == SCENE_DAY01) {
        /* Neon marquee & cinema interior */
        add_light(w, vec3_make(19.0f, 5.2f, 28.2f), 1.0f, 0.22f, 0.75f, 2.4f, 16.0f);
        add_light(w, vec3_make(19.0f, 3.8f, 17.0f),
                  w->cinema_restored ? 1.0f : 0.45f,
                  w->cinema_restored ? 0.85f : 0.55f,
                  w->cinema_restored ? 0.35f : 0.75f,
                  w->cinema_restored ? 2.2f : 1.1f * flick, 14.0f);
        /* Secret Arcade cyan light */
        add_light(w, vec3_make(31.0f, 3.0f, 20.0f), 0.15f, 0.92f, 1.0f, 1.5f, 10.0f);
        /* Rooftop Nuke-Exit & Boss spotlight */
        add_light(w, vec3_make(33.0f, 9.5f, 18.0f), 1.0f, 0.25f, 0.2f, 2.0f * flick, 15.0f);
        /* Streetlamp */
        add_light(w, vec3_make(9.0f, 5.0f, 36.0f), 0.7f, 0.85f, 1.0f, 1.4f, 14.0f);
    } else {
        const PortalMeta *meta = swieta_portal_meta(w->active_day);
        float r = meta ? meta->color_r : 0.8f;
        float g = meta ? meta->color_g : 0.8f;
        float b = meta ? meta->color_b : 1.0f;
        add_light(w, vec3_make(24.0f, 5.5f, 23.0f), r, g, b, 2.4f * flick, 18.0f);
    }

    /* Dynamic muzzle flashes, explosions & shrink orbs */
    for (int i = 0; i < w->player_count; ++i) {
        if (w->players[i].active && w->players[i].muzzle_flash > 0.0f) {
            add_light(w, player_eye_pos(&w->players[i]), 1.0f, 0.85f, 0.4f, 2.2f, 8.0f);
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (w->explosions[i].active) {
            add_light(w, w->explosions[i].pos, w->explosions[i].r, w->explosions[i].g, w->explosions[i].b,
                      3.2f * (w->explosions[i].timer / 0.45f), 12.0f);
        }
    }
    for (int i = 0; i < WORLD_MAX_PROJECTILES; ++i) {
        if (w->projectiles[i].active && w->projectiles[i].kind == PROJ_SHRINK_ORB) {
            add_light(w, w->projectiles[i].pos, 0.2f, 1.0f, 0.35f, 1.6f, 6.0f);
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
        if (w->scene != start_scene) {
            break; /* Scene transitioned; stop processing old scene entities */
        }
    }

    if (w->scene == start_scene) {
        step_projectiles(w, dt);
        step_enemies(w, dt);
    }

    for (int i = 0; i < WORLD_MAX_PICKUPS; ++i) {
        if (w->pickups[i].active) {
            w->pickups[i].bob_phase += dt * 3.0f;
        }
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
            /* Gentle reconciliation for local player */
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
        if (ps->weapon < WEAP_COUNT) {
            p->weapon = (WeaponId)ps->weapon;
        }
    }
}
