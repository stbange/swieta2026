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

/* Tier 1 Automated Game Test: Full 4-Beat headless playthrough of Grand Hall -> Day 1 ("Neon Strip '96") -> Grand Tree Shard */
static void test_automated_day01_full_playthrough(void) {
    SwietaDateTime dec1 = {2026, 12, 1, 8, 0, 0};
    GameWorld *w = &g_play_world;
    world_init(w, dec1, false, 1);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* 0. Walk west along the red carpet from Hub spawn (62.5,1,84.5) into Day 1 Portal (28.5,1,84.5) */
    for (int tick = 0; tick < 450 && w->scene == SCENE_HUB; ++tick) {
        aim_player_at(&w->players[0], vec3_make(28.5f, 2.0f, 84.5f));
        in[0].move_y = 1.0f;
        in[0].buttons = INP_SPRINT;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_EQ_INT(w->scene, SCENE_DAY01);
    ASSERT_EQ_INT(w->active_day, 1);

    /* ---- BEAT 1: BREAK PAWNSHOP GLASS, GRAB SHOTGUN & BLUE KEYCARD ---- */
    zero_inputs(in);
    w->players[0].pos = vec3_make(20.5f, 1.0f, 96.0f);
    aim_player_at(&w->players[0], vec3_make(18.5f, 2.5f, 96.0f));
    in[0].select_weapon = WEAP_PISTOL;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->sound_events & SFX_GLASS) != 0u);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 18, 2, 96), VOX_AIR);

    /* Walk through shattered storefront to acquire Shotgun */
    zero_inputs(in);
    w->players[0].pos = vec3_make(12.5f, 1.0f, 96.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].has_shotgun);
    ASSERT_EQ_INT(w->players[0].weapon, WEAP_SHOTGUN);

    /* Blast Beat 1 Trooper #0 with 7-pellet Shotgun */
    w->players[0].pos = vec3_make(24.0f, 1.0f, 106.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_add(w->enemies[0].pos, vec3_make(0, 1.0f, 0)));
    in[0].select_weapon = WEAP_SHOTGUN;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->enemies[0].active);

    /* Grab Blue Keycard inside Alley Security Kiosk -> Checkpoint 1 */
    zero_inputs(in);
    w->players[0].pos = vec3_make(45.5f, 2.0f, 96.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);
    ASSERT_EQ_INT(w->checkpoint_stage, 1);

    /* ---- BEAT 2: YULE CINEMA, SHRINK-STOMP, SECRET ARCADE & MIRROR SELF-SHRINK ---- */
    /* Unlock Blue Cinema Door #0 at (28, 1, 72) */
    zero_inputs(in);
    w->players[0].pos = vec3_make(28.0f, 1.0f, 73.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->doors[0].open);

    /* Shrink-Ray + Mighty Boot Stomp on Cinema Trooper #4 */
    zero_inputs(in);
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
    ASSERT_FALSE(w->enemies[4].active);
    ASSERT_EQ_INT(w->players[0].stomps, 1);

    /* Pipebomb blast Cracked Wall at (48, 2, 52.5) -> Secret #1 Retro Arcade */
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
    ASSERT_EQ_INT(voxel_get(&w->voxels, 48, 2, 52), VOX_AIR);

    zero_inputs(in);
    w->players[0].pos = vec3_make(53.5f, 1.0f, 52.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->secrets_found, 1);
    ASSERT_TRUE(w->players[0].armor >= 75);

    /* Signature Mirror Room Puzzle: shoot Shrink-Ray west at VOX_MIRROR -> self-shrink -> crawl 1-voxel vent! */
    zero_inputs(in);
    w->players[0].pos = vec3_make(15.5f, 1.0f, 65.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(11.5f, 2.0f, 65.0f));
    in[0].select_weapon = WEAP_SHRINKER;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 45 && w->players[0].self_shrunk_timer <= 0.0f; ++t) {
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->players[0].self_shrunk_timer > 10.0f);

    /* Crawl north through 1-voxel-tall air duct (x=15.5, y=1, z=59) into locked Projection Booth! */
    for (int t = 0; t < 140 && w->players[0].pos.z > 54.0f; ++t) {
        aim_player_at(&w->players[0], vec3_make(15.5f, 1.3f, 52.5f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->players[0].pos.z < 56.0f);

    /* Flip Cinema Projector Switch -> Restores Screen + Opens Booth & East Freight Doors + Checkpoint 2 */
    zero_inputs(in);
    w->players[0].pos = vec3_make(15.5f, 1.0f, 53.5f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->cinema_restored);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 26, 3, 37), VOX_CINEMA_LIT);
    ASSERT_TRUE(w->doors[1].open);
    ASSERT_TRUE(w->doors[2].open);
    ASSERT_EQ_INT(w->checkpoint_stage, 2);

    /* ---- BEAT 3: ROOFTOPS, WALKABILITY, TRIPMINE TRAP, HOLO-DECOY, JETPACK, SECRETS #2/#3 & RELAYS ---- */
    /* Physically walk up Stepped Freight Ramp from Street (61.5,1,85) -> Roof A (65,8,92) */
    zero_inputs(in);
    w->players[0].pos = vec3_make(61.5f, 1.0f, 84.5f);
    for (int t = 0; t < 180 && w->players[0].pos.y < 7.9f; ++t) {
        aim_player_at(&w->players[0], vec3_make(61.5f, w->players[0].pos.y + 1.5f, 94.0f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_NEAR(w->players[0].pos.y, 8.0f, 0.05f);

    /* Physically walk across East Catwalk from Roof A (88.5,8,83) -> Roof B (88.5,10,74) */
    zero_inputs(in);
    w->players[0].pos = vec3_make(88.5f, 8.0f, 83.0f);
    for (int t = 0; t < 160 && w->players[0].pos.y < 9.9f; ++t) {
        aim_player_at(&w->players[0], vec3_make(88.5f, w->players[0].pos.y + 1.5f, 72.0f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_NEAR(w->players[0].pos.y, 10.0f, 0.05f);

    /* Physically walk across Step-Bridge from Roof B (95.5,10,63.5) -> Roof C (101,12,63.5) */
    zero_inputs(in);
    w->players[0].pos = vec3_make(95.5f, 10.0f, 63.5f);
    for (int t = 0; t < 140 && w->players[0].pos.y < 11.9f; ++t) {
        aim_player_at(&w->players[0], vec3_make(102.0f, w->players[0].pos.y + 1.5f, 63.5f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_NEAR(w->players[0].pos.y, 12.0f, 0.05f);

    /* Plant Laser Tripmine on Roof A parapet and lure Trooper #8 across the red beam */
    zero_inputs(in);
    w->players[0].pos = vec3_make(76.0f, 8.0f, 111.5f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(76.0f, 8.5f, 114.5f));
    in[0].select_weapon = WEAP_TRIPMINE;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->tripmines[0].active);

    /* Arm tripmine & move enemy #8 across laser beam -> detonates! */
    zero_inputs(in);
    for (int t = 0; t < 30; ++t) world_step(w, in, WORLD_FIXED_DT);
    w->enemies[8].pos = vec3_add(w->tripmines[0].pos, vec3_scale(w->tripmines[0].normal, 1.2f));
    w->enemies[8].pos.y = 8.0f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->tripmines[0].active);
    ASSERT_FALSE(w->enemies[8].active);

    /* Deploy Holo-Decoy to distract Rooftop Turret & Octabrain */
    zero_inputs(in);
    in[0].buttons = INP_HOLO;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->decoy.active);

    /* Jetpack upward flight verification */
    w->players[0].pos = vec3_make(70.0f, 8.0f, 102.0f);
    float start_y = w->players[0].pos.y;
    for (int t = 0; t < 60; ++t) {
        zero_inputs(in);
        in[0].buttons = INP_JETPACK;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->players[0].pos.y > start_y + 2.5f);
    ASSERT_TRUE(w->players[0].jetpack_fuel < 95.0f);

    /* Break Skylight glass on Roof B & collect Secret #2 */
    zero_inputs(in);
    w->players[0].pos = vec3_make(72.5f, 10.0f, 54.0f);
    w->players[0].fire_cooldown = 0.0f;
    aim_player_at(&w->players[0], vec3_make(72.5f, 9.5f, 56.5f));
    in[0].select_weapon = WEAP_PISTOL;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 72, 9, 56), VOX_AIR);

    zero_inputs(in);
    w->players[0].pos = vec3_make(72.5f, 6.0f, 57.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->secrets_found, 2);

    /* Destroy Broadcast Relay #1 (Roof B) & Relay #2 (Roof C) -> Red Keycard Vault Door #3 opens + Checkpoint 3 */
    w->players[0].pos = vec3_make(84.0f, 10.0f, 65.0f);
    for (int t = 0; t < 80 && !w->relays[0].destroyed; ++t) {
        zero_inputs(in);
        aim_player_at(&w->players[0], w->relays[0].pos);
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->relays[0].destroyed);

    w->players[0].pos = vec3_make(111.0f, 12.0f, 63.0f);
    for (int t = 0; t < 80 && !w->relays[1].destroyed; ++t) {
        zero_inputs(in);
        aim_player_at(&w->players[0], w->relays[1].pos);
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->relays[1].destroyed);
    ASSERT_TRUE(w->relays_cleared);
    ASSERT_TRUE(w->doors[3].open);
    ASSERT_EQ_INT(w->checkpoint_stage, 3);

    /* Grab Secret #3 (Broadcast Tower Vault) & Red Keycard on Roof C */
    zero_inputs(in);
    w->players[0].pos = vec3_make(118.5f, 12.0f, 42.0f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->secrets_found, 3);

    w->players[0].pos = vec3_make(108.0f, 12.0f, 42.5f);
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->players[0].keys & KEY_RED) != 0u);

    /* ---- BEAT 4: SKYBRIDGE, RED PENTHOUSE GATE, 3-PHASE BOSS & NUKE-EXIT ---- */
    /* Physically walk across Neon Skybridge from Roof C (103,12,38) -> South Face of Red Gate (87,12,28) */
    zero_inputs(in);
    w->players[0].pos = vec3_make(103.0f, 12.0f, 38.0f);
    for (int t = 0; t < 160 && w->players[0].pos.z > 31.5f; ++t) {
        aim_player_at(&w->players[0], vec3_make(103.0f, 13.5f, 31.0f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    for (int t = 0; t < 240 && !w->doors[4].open; ++t) {
        aim_player_at(&w->players[0], vec3_make(87.0f, 13.5f, 29.0f));
        in[0].move_y = 1.0f;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w->doors[4].open);
    ASSERT_EQ_INT(w->checkpoint_stage, 4);
    ASSERT_NEAR(w->players[0].pos.y, 12.0f, 0.05f);

    /* Fight KRAMPUS-MECH MK.IV (enemy #14) through Phase 1 -> Phase 2 -> Phase 3 -> Defeated */
    w->players[0].pos = vec3_make(68.0f, 12.0f, 17.0f);
    bool saw_phase2 = false;
    bool saw_phase3 = false;
    for (int t = 0; t < 600 && !w->boss_defeated; ++t) {
        zero_inputs(in);
        w->players[0].hp = 100; /* Focus on boss phase state verification */
        aim_player_at(&w->players[0], vec3_add(w->enemies[14].pos, vec3_make(0, 1.4f, 0)));
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
        if (w->boss_phase == 2) saw_phase2 = true;
        if (w->boss_phase == 3) saw_phase3 = true;
    }
    ASSERT_TRUE(saw_phase2);
    ASSERT_TRUE(saw_phase3);
    ASSERT_TRUE(w->boss_defeated);

    /* Press USE on Red Nuke-Exit -> Earn Shard #1 -> Return to Grand Festive Hall with Lit Tree Ornament #0! */
    zero_inputs(in);
    w->players[0].pos = vec3_make(31.5f, 12.0f, 17.0f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);
    ASSERT_TRUE((w->shard_mask & 1u) != 0u);
    ASSERT_EQ_INT(swieta_progress_count_shards(w->shard_mask), 1);
    ASSERT_EQ_INT(voxel_get(&w->voxels, 59, 4, 64), VOX_GOLD);
}

/* Tier 1 Automated Multiplayer Test: 4P Split-Screen + Network Snapshot Sync with Packet Loss */
static void test_automated_splitscreen_and_network_soak(void) {
    SwietaDateTime dec1 = {2026, 12, 1, 10, 0, 0};
    GameWorld *host = &g_host_world;
    GameWorld *client = &g_client_world;
    world_init(host, dec1, true, 4);
    world_init(client, dec1, true, 2);

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

    /* Host warps to Day 1 -> snapshot seamlessly transitions Client to Day 1 */
    world_load_day(host, 1);
    NetSnapshotPacket warp_snap, recv_warp;
    world_build_snapshot(host, &warp_snap);
    size_t wlen = net_encode_snapshot(&warp_snap, wire, sizeof(wire));
    ASSERT_TRUE(net_decode_snapshot(wire, wlen, &recv_warp));
    world_apply_snapshot(client, &recv_warp, 1);
    ASSERT_EQ_INT(client->scene, SCENE_DAY01);
}

int main(void) {
    RUN_TEST(test_automated_day01_full_playthrough);
    RUN_TEST(test_automated_splitscreen_and_network_soak);
    printf("\nAutomated game tests run: %d | Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
