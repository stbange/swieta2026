#include "../unit/test_harness.h"

#include "net/net_proto.h"
#include "sim/world.h"

#include <math.h>
#include <string.h>

/* Static worlds keep stack usage minimal across Windows/Linux/macOS */
static GameWorld g_play_world;
static GameWorld g_host_world;
static GameWorld g_client_world;

static void zero_inputs(PlayerInput in[WORLD_MAX_PLAYERS]) {
    memset(in, 0, sizeof(PlayerInput) * WORLD_MAX_PLAYERS);
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) in[i].select_weapon = -1;
}

static void aim_player_at(Player *p, Vec3 target) {
    Vec3 eye = player_eye_pos(p);
    Vec3 d = vec3_sub(target, eye);
    float horiz = sqrtf(d.x * d.x + d.z * d.z);
    p->yaw = atan2f(d.x, -d.z);
    p->pitch = atan2f(d.y, horiz > 1e-4f ? horiz : 1e-4f);
}

static void tap_use(GameWorld *w, PlayerInput in[WORLD_MAX_PLAYERS]) {
    zero_inputs(in);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    world_step(w, in, WORLD_FIXED_DT);
}

/* ============================================================================
 * DAY 1 AUTOMATED BOT: Duke Nukem 3D ("Neon Strip '96")
 * ============================================================================ */
static void test_automated_day01_full_playthrough(void) {
    SwietaDateTime dec1 = {2026, 12, 1, 8, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec1, false, 1);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Walk west from Hub spawn into Day 1 Portal */
    for (int tick = 0; tick < 450 && w->scene == SCENE_HUB; ++tick) {
        aim_player_at(&w->players[0], vec3_make(28.5f, 2.0f, 84.5f));
        in[0].move_y = 1.0f;
        in[0].buttons = INP_SPRINT;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_EQ_INT(w->scene, SCENE_DAY01);

    /* Beat 1: Break Pawnshop glass -> Shotgun -> kill Trooper -> Blue Keycard */
    zero_inputs(in);
    w->players[0].pos = vec3_make(20.5f, 1.0f, 96.0f);
    aim_player_at(&w->players[0], vec3_make(18.5f, 2.5f, 96.0f));
    in[0].select_weapon = WEAP_PISTOL;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->sound_events & SFX_GLASS) != 0u);

    zero_inputs(in);
    w->players[0].pos = vec3_make(12.5f, 1.0f, 96.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].has_shotgun);

    w->players[0].pos = vec3_make(24.0f, 1.0f, 106.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0, 1.0f, 0)));
    in[0].select_weapon = WEAP_SHOTGUN;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->enemies[0].active);

    zero_inputs(in);
    w->players[0].pos = vec3_make(45.5f, 2.0f, 96.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);

    /* Beat 2: Unlock Cinema -> Shrink+Stomp -> Pipebomb Secret #1 -> Mirror Self-Shrink -> Vent -> Projector */
    w->players[0].pos = vec3_make(28.0f, 1.0f, 73.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[0].open);

    w->players[0].pos = vec3_make(30.0f, 1.0f, 68.5f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_add(w->enemies[4].pos, vec3_make(0, 0.9f, 0)));
    in[0].select_weapon = WEAP_SHRINKER;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 35; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[4].shrunk_timer > 0.0f);
    for (int t = 0; t < 90 && w->enemies[4].active; ++t) {
        aim_player_at(&w->players[0], w->enemies[4].pos);
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_EQ_INT(w->players[0].stomps, 1);

    zero_inputs(in);
    w->players[0].pos = vec3_make(45.5f, 1.0f, 52.5f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(48.0f, 2.0f, 52.5f));
    in[0].select_weapon = WEAP_PIPEBOMB;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 16; ++t) world_step(w, in, WORLD_FIXED_DT);
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->secret_wall_blasted);

    zero_inputs(in);
    w->players[0].pos = vec3_make(53.5f, 1.0f, 52.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->secrets_found, 1);

    w->players[0].pos = vec3_make(15.5f, 1.0f, 65.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(11.5f, 2.0f, 65.0f));
    in[0].select_weapon = WEAP_SHRINKER;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 45 && w->players[0].self_shrunk_timer <= 0.0f; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].self_shrunk_timer > 10.0f);

    for (int t = 0; t < 140 && w->players[0].pos.z > 54.0f; ++t) {
        aim_player_at(&w->players[0], vec3_make(15.5f, 1.3f, 52.5f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    w->players[0].pos = vec3_make(15.5f, 1.0f, 53.5f);
    tap_use(w, in);
    ASSERT_TRUE(w->cinema_restored);
    ASSERT_TRUE(w->doors[2].open);

    /* Beat 3: Walk Ramp/Catwalk/Bridge + Tripmine + Holo + Jetpack + Skylight + 2 Relays */
    zero_inputs(in);
    w->players[0].pos = vec3_make(61.5f, 1.0f, 84.5f);
    for (int t = 0; t < 180 && w->players[0].pos.y < 7.9f; ++t) {
        aim_player_at(&w->players[0], vec3_make(61.5f, w->players[0].pos.y + 1.5f, 94.0f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_NEAR(w->players[0].pos.y, 8.0f, 0.05f);

    zero_inputs(in);
    w->players[0].pos = vec3_make(76.0f, 8.0f, 111.5f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(76.0f, 8.5f, 114.5f));
    in[0].select_weapon = WEAP_TRIPMINE;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->tripmines[0].active);
    zero_inputs(in);
    for (int t = 0; t < 30; ++t) world_step(w, in, WORLD_FIXED_DT);
    w->enemies[8].pos = vec3_add(w->tripmines[0].pos, vec3_scale(w->tripmines[0].normal, 1.2f));
    w->enemies[8].pos.y = 8.0f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->enemies[8].active);

    zero_inputs(in);
    in[0].buttons = INP_HOLO;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->decoy.active);

    w->players[0].pos = vec3_make(70.0f, 8.0f, 102.0f);
    for (int t = 0; t < 60; ++t) {
        zero_inputs(in);
        in[0].buttons = INP_JETPACK;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->players[0].pos.y > 10.5f);

    w->players[0].pos = vec3_make(84.0f, 10.0f, 65.0f);
    for (int t = 0; t < 80 && !w->relays[0].destroyed; ++t) {
        zero_inputs(in);
        aim_player_at(&w->players[0], w->relays[0].pos);
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    w->players[0].pos = vec3_make(111.0f, 12.0f, 63.0f);
    for (int t = 0; t < 80 && !w->relays[1].destroyed; ++t) {
        zero_inputs(in);
        aim_player_at(&w->players[0], w->relays[1].pos);
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->relays_cleared);

    zero_inputs(in);
    w->players[0].pos = vec3_make(108.0f, 12.0f, 42.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_RED) != 0u);

    /* Beat 4: Skybridge -> Red Gate -> 3-Phase Boss -> Nuke-Exit -> Hub Shard #1 */
    w->players[0].pos = vec3_make(87.0f, 12.0f, 30.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[4].open);

    w->players[0].pos = vec3_make(68.0f, 12.0f, 17.0f);
    for (int t = 0; t < 600 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100;
        aim_player_at(&w->players[0], vec3_add(w->enemies[14].pos, vec3_make(0, 1.4f, 0)));
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->boss_defeated);
    w->players[0].pos = vec3_make(31.5f, 12.0f, 17.0f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 0)) != 0u);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 59, 4, 64), VOX_GOLD);
}

/* ============================================================================
 * DAY 2 AUTOMATED BOT: Tomb Raider ("Vault of the Pixel Sun")
 * ============================================================================ */
static void test_automated_day02_tombraider_playthrough(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 9, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec6, false, 1);
    world_load_day(w, 2);
    ASSERT_EQ_INT(w->scene, SCENE_DAY02);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Beat 1: Dual Pistols Soft Lock-On + Acrobatic Dodge + Magnesium Flare + Push-Block */
    w->players[0].pos = vec3_make(38.0f, 1.0f, 108.0f);
    aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0.25f, 1.0f, 0)));
    for (int t = 0; t < 80 && w->enemies[0].active; ++t) {
        zero_inputs(in);
        in[0].select_weapon = WEAP_DUAL_PISTOLS;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_FALSE(w->enemies[0].active);

    /* Acrobatic side-flip dodge (RMB) */
    zero_inputs(in);
    in[0].move_x = 1.0f;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].dodge_cooldown > 0.5f);
    ASSERT_FALSE(w->players[0].on_ground);
    for (int t = 0; t < 40; ++t) { zero_inputs(in); world_step(w, in, WORLD_FIXED_DT); }

    /* Toss Green Magnesium Flare -> illuminates dark tomb */
    zero_inputs(in);
    in[0].select_weapon = WEAP_FLARE;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->flares[0].active && !w->flares[0].is_smoke);

    /* Push Golden Stone Block onto pressure plate -> Door #0 opens + Checkpoint 1 */
    w->players[0].pos = vec3_make(44.0f, 1.0f, 99.0f);
    tap_use(w, in);
    ASSERT_TRUE(w->push_blocks[0].settled);
    ASSERT_TRUE(w->doors[0].open);
    ASSERT_EQ_INT(w->checkpoint_stage, 1);

    /* Secret #1: Spike Crypt -> Dual Magnums */
    w->players[0].pos = vec3_make(70.0f, 1.0f, 99.0f);
    zero_inputs(in);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->secrets_found, 1);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_MAGNUM)) != 0u);

    /* Beat 2: Flooded Cistern Valve -> Water fills to y=4 + Door #1 opens + Swim across! */
    w->players[0].pos = vec3_make(28.5f, 4.0f, 66.0f);
    tap_use(w, in);
    ASSERT_TRUE(w->cistern_flooded);
    ASSERT_TRUE(w->doors[1].open);
    ASSERT_EQ_INT(w->checkpoint_stage, 2);

    /* Swim across flooded cistern (in_water == true + Jump swims upward) */
    w->players[0].pos = vec3_make(44.0f, 3.0f, 66.0f);
    in[0].buttons = INP_JUMP;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].in_water);
    ASSERT_TRUE(w->players[0].vel.y > 2.0f);
    zero_inputs(in);

    /* Beat 3: Trigger Rolling Boulder + Grab Blue Relic Key + Unlock Valley Gate */
    w->players[0].pos = vec3_make(44.0f, 5.0f, 43.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->boulders[0].rolling);
    for (int t = 0; t < 20; ++t) world_step(w, in, WORLD_FIXED_DT); /* Roll forward */

    w->players[0].pos = vec3_make(51.5f, 5.0f, 38.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);

    w->players[0].pos = vec3_make(44.0f, 5.0f, 29.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[2].open);

    /* Beat 4: Defeat Valley T-Rex with Dual Magnums -> Claim Scion Prism Shard #2! */
    w->players[0].pos = vec3_make(46.0f, 5.0f, 24.0f);
    for (int t = 0; t < 500 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100;
        aim_player_at(&w->players[0], vec3_add(w->enemies[7].pos, vec3_make(0, 1.5f, 0)));
        in[0].select_weapon = WEAP_MAGNUM;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->boss_defeated);
    w->players[0].pos = vec3_make(44.5f, 5.0f, 10.5f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 1)) != 0u);
}

/* ============================================================================
 * DAY 3 AUTOMATED BOT: Counter-Strike ("de_yule_dust")
 * ============================================================================ */
static void test_automated_day03_counterstrike_playthrough(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 9, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec6, false, 1);
    world_load_day(w, 3);
    ASSERT_EQ_INT(w->scene, SCENE_DAY03);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Beat 1: Buy Defuse Kit ($400) -> Kill Rifleman (+$300) -> Buy Deagle ($700) */
    w->players[0].pos = vec3_make(60.5f, 1.0f, 108.5f);
    tap_use(w, in);
    ASSERT_TRUE(w->players[0].has_defuse_kit);
    ASSERT_EQ_INT(w->players[0].cash, 600);

    /* Flashbang blinds Mid Syndicate Trooper #0! */
    w->players[0].pos = vec3_make(58.0f, 1.0f, 94.0f);
    aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0, 1.0f, 0)));
    in[0].select_weapon = WEAP_FLASH_SMOKE;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 55; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[0].blind_timer > 0.0f);

    /* Smoke Grenade (RMB) blocks Sniper sightlines */
    in[0].select_weapon = WEAP_FLASH_SMOKE;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->flares[0].active && w->flares[0].is_smoke);

    /* Eliminate Mid Rifleman with USP (+$300 cash) */
    for (int t = 0; t < 80 && w->enemies[0].active; ++t) {
        zero_inputs(in);
        aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0, 1.0f, 0)));
        in[0].select_weapon = WEAP_USP;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_FALSE(w->enemies[0].active);
    ASSERT_TRUE(w->players[0].cash >= 900);

    /* Buy Heavy Deagle at Buy Crate */
    w->players[0].pos = vec3_make(60.5f, 1.0f, 108.5f);
    tap_use(w, in);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_DEAGLE)) != 0u);

    /* Beat 2: Grab Carbine & Defuse C4 Bombsite A (2 taps with Defuse Kit!) -> Door #0 opens */
    w->players[0].pos = vec3_make(34.0f, 1.0f, 68.0f);
    zero_inputs(in);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_CARBINE)) != 0u);

    /* Test Carbine fire & Deagle fire */
    in[0].select_weapon = WEAP_CARBINE;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    in[0].select_weapon = WEAP_DEAGLE;
    w->players[0].fire_cooldown = 0.0f;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);

    w->players[0].pos = vec3_make(28.5f, 1.0f, 66.5f);
    tap_use(w, in);
    tap_use(w, in);
    ASSERT_EQ_INT(w->bombs_defused, 1);
    ASSERT_TRUE(w->doors[0].open);

    /* Beat 3: Walk Catwalk, spend cash on Kevlar/Sniper/Resupply, toggle Sniper Scope (RMB) & shoot! */
    w->players[0].cash = 5000;
    w->players[0].pos = vec3_make(60.5f, 5.0f, 59.5f);
    for (int b = 0; b < 5; ++b) tap_use(w, in); /* Buys Kevlar, Sniper, Resupply */
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_SNIPER)) != 0u);
    zero_inputs(in);
    in[0].select_weapon = WEAP_SNIPER;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].zoomed);
    zero_inputs(in);
    w->players[0].fire_cooldown = 0.0f;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);

    /* Beat 4: Defuse C4 Bombsite B -> Claim Defusal Pliers Shard #3! */
    w->players[0].pos = vec3_make(92.5f, 1.0f, 25.5f);
    tap_use(w, in);
    tap_use(w, in);
    ASSERT_EQ_INT(w->bombs_defused, 2);

    w->players[0].pos = vec3_make(92.5f, 1.0f, 17.5f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 2)) != 0u);
}

/* ============================================================================
 * DAY 4 AUTOMATED BOT: Quake ("Slipgate of Brass & Rust")
 * ============================================================================ */
static void test_automated_day04_quake_playthrough(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 9, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec6, false, 1);
    world_load_day(w, 4);
    ASSERT_EQ_INT(w->scene, SCENE_DAY04);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Beat 1: Grab Rocket Launcher -> Execute Genuine Rocket Jump! */
    w->players[0].pos = vec3_make(40.5f, 1.0f, 98.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_ROCKET)) != 0u);

    w->players[0].pos = vec3_make(40.5f, 1.0f, 108.0f);
    w->players[0].pitch = -1.30f; /* Look straight down at feet */
    w->players[0].fire_cooldown = 0.0f;
    in[0].select_weapon = WEAP_ROCKET;
    in[0].buttons = INP_FIRE | INP_JUMP;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 20; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].pos.y > 3.2f); /* Rocket-jump altitude achieved! */

    /* Jump Pad #0 launch test */
    w->players[0].pos = vec3_make(52.5f, 1.0f, 105.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].vel.y > 10.0f);

    /* Flip Brass Portcullis Lever -> Door #0 opens */
    w->players[0].pos = vec3_make(40.5f, 6.0f, 90.0f);
    tap_use(w, in);
    ASSERT_TRUE(w->doors[0].open);

    /* Beat 2: Grab Thunderbolt -> Dry fire + Underwater AoE Discharge! */
    w->players[0].pos = vec3_make(28.0f, 3.0f, 70.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_THUNDERBOLT)) != 0u);
    w->players[0].fire_cooldown = 0.0f;
    in[0].select_weapon = WEAP_THUNDERBOLT;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].lightning_timer > 0.0f);

    /* Step into flooded trench water & trigger Underwater Discharge frying water Fiend #2! */
    w->players[0].pos = vec3_make(40.0f, 1.5f, 68.0f);
    w->players[0].fire_cooldown = 0.0f;
    zero_inputs(in);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].in_water);
    in[0].select_weapon = WEAP_THUNDERBOLT;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->enemies[2].active); /* Electrocuted underwater! */

    /* Grab Blue Rune -> Open Door #1 -> Enter Slipgate #0 -> TELEFRAG Death-Knight #4! */
    zero_inputs(in);
    w->players[0].pos = vec3_make(28.0f, 3.0f, 54.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);
    w->players[0].pos = vec3_make(54.0f, 3.0f, 52.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[1].open);

    ASSERT_TRUE(w->enemies[4].active);
    w->players[0].pos = vec3_make(60.0f, 3.0f, 52.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->enemies[4].active); /* Telefragged on arrival! */

    /* Beat 3: Grab Quad Damage Rune (4x!) & Red Rune -> Slipgate #1 */
    w->players[0].pos = vec3_make(90.0f, 3.0f, 64.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].quad_timer > 15.0f);

    w->players[0].pos = vec3_make(107.0f, 8.0f, 54.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_RED) != 0u);
    w->players[0].pos = vec3_make(90.0f, 3.0f, 53.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[2].open);
    w->players[0].pos = vec3_make(90.0f, 3.0f, 49.0f);
    world_step(w, in, WORLD_FIXED_DT);

    /* Beat 4: Melt Shambler Boss with Quad Super Nailgun -> Claim Slipgate Rune Shard #4! */
    w->players[0].pos = vec3_make(64.0f, 5.0f, 26.0f);
    for (int t = 0; t < 300 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100;
        aim_player_at(&w->players[0], vec3_add(w->enemies[7].pos, vec3_make(0, 1.5f, 0)));
        in[0].select_weapon = WEAP_NAILGUN;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->boss_defeated);
    w->players[0].pos = vec3_make(64.5f, 5.0f, 10.5f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 3)) != 0u);
}

/* ============================================================================
 * DAY 5 AUTOMATED BOT: Blood ("Carnival of the Damned")
 * ============================================================================ */
static void test_automated_day05_blood_playthrough(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 9, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec6, false, 1);
    world_load_day(w, 5);
    ASSERT_EQ_INT(w->scene, SCENE_DAY05);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Beat 1: Pitchfork Melee + Flare Gun Burn DoT + Flare Starburst Alt-Fire! */
    w->players[0].pos = vec3_make(34.0f, 1.0f, 104.0f);
    aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0, 1.0f, 0)));
    in[0].select_weapon = WEAP_PITCHFORK;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[0].hp < w->enemies[0].max_hp);

    /* Ignite Ghoul #1 with Flare Gun -> Burn DoT ticks damage over time! */
    zero_inputs(in);
    w->players[0].pos = vec3_make(44.0f, 1.0f, 101.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_add(w->enemies[1].pos, vec3_make(0, 1.0f, 0)));
    in[0].select_weapon = WEAP_FLAREGUN;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 25 && w->enemies[1].burn_timer <= 0.0f; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[1].burn_timer > 0.0f);
    int hp_burn = w->enemies[1].hp;
    for (int t = 0; t < 40; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[1].hp < hp_burn);

    /* Flare Starburst Alt-Fire (RMB) */
    w->players[0].fire_cooldown = 0.0f;
    in[0].select_weapon = WEAP_FLAREGUN;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);

    /* Grab Aerosol Flamethrower -> Fire Puffs + Thrown Molotov Alt-Fire */
    zero_inputs(in);
    w->players[0].pos = vec3_make(25.5f, 1.0f, 98.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_AEROSOL)) != 0u);
    in[0].select_weapon = WEAP_AEROSOL;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    in[0].select_weapon = WEAP_AEROSOL;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 80; ++t) world_step(w, in, WORLD_FIXED_DT); /* Molotov explodes */

    /* Blast Cracked Crypt Wall with TNT Bundle (LMB) -> Checkpoint 1 */
    w->players[0].pos = vec3_make(38.0f, 1.0f, 90.5f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(38.0f, 2.0f, 88.0f));
    in[0].select_weapon = WEAP_TNT;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 75; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 38, 2, 88), VOX_AIR);
    ASSERT_EQ_INT(w->checkpoint_stage, 1);

    /* Proximity TNT Alt-Fire (RMB) triggers on Ghost Train Cultist #2 */
    w->players[0].pos = vec3_make(46.0f, 2.0f, 70.0f);
    w->players[0].fire_cooldown = 0.0f;
    in[0].select_weapon = WEAP_TNT;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 45; ++t) world_step(w, in, WORLD_FIXED_DT);

    /* Beat 2: Flip Ghost Train Throttle -> Door #0 blows open + Checkpoint 2 */
    w->players[0].pos = vec3_make(67.5f, 2.0f, 70.0f);
    tap_use(w, in);
    ASSERT_TRUE(w->train_activated);
    ASSERT_TRUE(w->doors[0].open);

    /* Beat 3: Grab Voodoo Doll (stab + AoE Curse Alt-Fire) + Red Skull Key -> Unlock Big Top */
    w->players[0].pos = vec3_make(36.0f, 1.0f, 44.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_VOODOO)) != 0u);
    in[0].select_weapon = WEAP_VOODOO;
    in[0].buttons = INP_ALT;
    world_step(w, in, WORLD_FIXED_DT);

    zero_inputs(in);
    w->players[0].pos = vec3_make(86.0f, 1.0f, 44.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_RED) != 0u);
    w->players[0].pos = vec3_make(60.0f, 1.0f, 33.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[1].open);

    /* Beat 4: Banish Gargoyle Matriarch with Voodoo Doll -> Claim Crimson Chalice Shard #5! */
    w->players[0].pos = vec3_make(60.0f, 1.0f, 24.0f);
    w->players[0].ammo_cells = 120;
    for (int t = 0; t < 400 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100;
        aim_player_at(&w->players[0], vec3_add(w->enemies[7].pos, vec3_make(0, 1.5f, 0)));
        in[0].select_weapon = WEAP_VOODOO;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->boss_defeated);
    w->players[0].pos = vec3_make(60.5f, 1.0f, 10.5f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 4)) != 0u);
}

/* ============================================================================
 * DAY 6 AUTOMATED BOT: Doom Act I Climax ("E1M24: Brimstone Hangar")
 * ============================================================================ */
static void test_automated_day06_doom_playthrough(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 9, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec6, false, 1);
    world_load_day(w, 6);
    ASSERT_EQ_INT(w->scene, SCENE_DAY06);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* Beat 1: Shoot Explosive Barrel #0 -> Chain Reaction detonates adjacent barrels #1 & #2! */
    w->players[0].pos = vec3_make(60.0f, 1.0f, 104.0f);
    aim_player_at(&w->players[0], vec3_add(w->barrels[0].pos, vec3_make(0, 0.7f, 0)));
    in[0].select_weapon = WEAP_SUPER_SHOTGUN;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->barrels[0].active);
    ASSERT_FALSE(w->barrels[1].active); /* Chained! */

    /* Grab Radiation Suit -> Wade safely through radioactive VOX_SLIME with ZERO damage! */
    zero_inputs(in);
    w->players[0].pos = vec3_make(55.0f, 1.0f, 92.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].rad_suit_timer > 25.0f);
    int hp_before_slime = w->players[0].hp;
    w->players[0].pos = vec3_make(46.0f, 1.0f, 82.0f);
    for (int t = 0; t < 35; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->players[0].hp, hp_before_slime);

    /* Grab Blue Keycard + Secret #1 Berserk Pack + Berserk Chainsaw! */
    w->players[0].pos = vec3_make(46.0f, 1.0f, 74.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);

    w->players[0].pos = vec3_make(36.0f, 1.0f, 80.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].berserk_timer > 15.0f);

    /* Grab Chaingun & test Chainsaw */
    w->players[0].pos = vec3_make(76.0f, 1.0f, 98.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_CHAINGUN)) != 0u);
    in[0].select_weapon = WEAP_CHAINSAW;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    in[0].select_weapon = WEAP_CHAINGUN;
    w->players[0].fire_cooldown = 0.0f;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);

    /* Beat 2: Open Blue Door -> Verify live Doom Monster Infighting! -> Grab Yellow Keycard */
    zero_inputs(in);
    w->players[0].pos = vec3_make(41.5f, 1.0f, 63.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[0].open);

    /* Imp #3's fireball hits Pinky #4 -> Pinky turns and brawls Imp #3! */
    w->projectiles[0].active = true;
    w->projectiles[0].kind = PROJ_ENEMY_FIREBALL;
    w->projectiles[0].owner_player = -1;
    w->projectiles[0].owner_enemy = 3;
    w->projectiles[0].pos = vec3_add(w->enemies[4].pos, vec3_make(0, 0.9f, 0));
    w->projectiles[0].vel = vec3_make(0, 0, 0);
    w->projectiles[0].life = 1.0f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->enemies[4].infight_target, 3);

    w->players[0].pos = vec3_make(20.0f, 1.0f, 46.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_YELLOW) != 0u);

    /* Beat 3: Open Yellow Door -> Grab Yule-BFG 9000 & Red Keycard */
    w->players[0].pos = vec3_make(82.5f, 1.0f, 63.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[1].open);
    w->players[0].pos = vec3_make(102.0f, 1.0f, 56.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].owned_weapons & (1u << WEAP_BFG)) != 0u);
    w->players[0].pos = vec3_make(106.0f, 1.0f, 46.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_RED) != 0u);

    /* Beat 4: Open Red Reactor Gate -> Unleash Yule-BFG 9000 on Cyber-Titan -> Claim Hellfire Key Shard #6! */
    w->players[0].pos = vec3_make(62.0f, 1.0f, 41.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[2].open);

    w->players[0].pos = vec3_make(62.0f, 1.0f, 28.0f);
    w->players[0].ammo_cells = 200;
    for (int t = 0; t < 400 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100;
        aim_player_at(&w->players[0], vec3_add(w->enemies[9].pos, vec3_make(0, 1.5f, 0)));
        in[0].select_weapon = WEAP_BFG;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->boss_defeated);

    w->players[0].pos = vec3_make(62.5f, 1.0f, 10.5f);
    tap_use(w, in);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & (1u << 5)) != 0u);
}

/* ============================================================================
 * MULTIPLAYER SOAK TEST: 4P Split-Screen + 10% Packet-Loss WebRTC Snapshot Sync
 * ============================================================================ */
static void test_automated_splitscreen_and_network_soak(void) {
    SwietaDateTime dec6 = {2026, 12, 6, 10, 0, 0};
    GameWorld *host = &g_host_world;
    GameWorld *client = &g_client_world;
    world_init(host, dec6, true, 4);
    world_init(client, dec6, true, 2);

    PlayerInput hin[WORLD_MAX_PLAYERS];
    PlayerInput cin[WORLD_MAX_PLAYERS];
    zero_inputs(hin);
    zero_inputs(cin);

    hin[0].move_y = 1.0f;
    hin[1].move_x = 1.0f;
    hin[2].move_y = -1.0f;
    hin[3].move_x = -1.0f;

    uint8_t wire[NET_MAX_PACKET_SIZE];
    for (int tick = 0; tick < 120; ++tick) {
        world_step(host, hin, WORLD_FIXED_DT);
        if (tick % 10 != 0) {
            NetSnapshotPacket snap, recv_snap;
            world_build_snapshot(host, &snap);
            size_t len = net_encode_snapshot(&snap, wire, sizeof(wire));
            ASSERT_TRUE(len > 0);
            ASSERT_TRUE(net_decode_snapshot(wire, len, &recv_snap));
            world_apply_snapshot(client, &recv_snap, 1);
        }
        world_step(client, cin, WORLD_FIXED_DT);
    }

    ASSERT_EQ_INT(client->player_count, 4);
    ASSERT_TRUE(vec3_dist(host->players[0].pos, client->players[0].pos) < 0.75f);
    ASSERT_TRUE(vec3_dist(host->players[1].pos, client->players[1].pos) < 1.0f);

    /* Host warps to Day 6 -> snapshot seamlessly transitions Client to Day 6 */
    world_load_day(host, 6);
    NetSnapshotPacket warp_snap, recv_warp;
    world_build_snapshot(host, &warp_snap);
    size_t wlen = net_encode_snapshot(&warp_snap, wire, sizeof(wire));
    ASSERT_TRUE(net_decode_snapshot(wire, wlen, &recv_warp));
    world_apply_snapshot(client, &recv_warp, 1);
    ASSERT_EQ_INT(client->scene, SCENE_DAY06);
}

int main(void) {
    RUN_TEST(test_automated_day01_full_playthrough);
    RUN_TEST(test_automated_day02_tombraider_playthrough);
    RUN_TEST(test_automated_day03_counterstrike_playthrough);
    RUN_TEST(test_automated_day04_quake_playthrough);
    RUN_TEST(test_automated_day05_blood_playthrough);
    RUN_TEST(test_automated_day06_doom_playthrough);
    RUN_TEST(test_automated_splitscreen_and_network_soak);
    printf("\nAutomated game tests run: %d | Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
