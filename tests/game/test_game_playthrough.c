#include "../unit/test_harness.h"

#include "net/net_proto.h"
#include "sim/world.h"

#include <math.h>
#include <string.h>

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

/* Tier 1 Automated Game Test: Full deterministic headless playthrough of Grand Hall -> Day 1 -> Shard */
static void test_automated_day01_full_playthrough(void) {
    SwietaDateTime dec1 = {2026, 12, 1, 8, 0, 0};
    GameWorld w;
    world_init(&w, dec1, false, 1);
    ASSERT_EQ_INT(w.scene, SCENE_HUB);

    PlayerInput in[WORLD_MAX_PLAYERS];
    zero_inputs(in);

    /* 1. Bot walks from Hub spawn directly into Day 1 Portal at (4.5, 1, 36.5) */
    for (int tick = 0; tick < 300 && w.scene == SCENE_HUB; ++tick) {
        aim_player_at(&w.players[0], vec3_make(4.5f, 2.0f, 36.5f));
        in[0].move_y = 1.0f;
        in[0].buttons = INP_SPRINT;
        world_step(&w, in, WORLD_FIXED_DT);
    }
    ASSERT_EQ_INT(w.scene, SCENE_DAY01);
    ASSERT_EQ_INT(w.active_day, 1);

    /* 2. Shrink-Ray + Mighty Boot Stomp on Alien Trooper #0 at (14, 1, 34) */
    zero_inputs(in);
    w.players[0].pos = vec3_make(10.0f, 1.01f, 34.0f);
    aim_player_at(&w.players[0], vec3_add(w.enemies[0].pos, vec3_make(0, 0.9f, 0)));
    in[0].select_weapon = WEAP_SHRINKER;
    in[0].buttons = INP_FIRE;
    world_step(&w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 45; ++t) world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.enemies[0].shrunk_timer > 0.0f);

    /* Walk into shrunk enemy to execute Mighty Boot Stomp */
    for (int t = 0; t < 90 && w.enemies[0].active; ++t) {
        aim_player_at(&w.players[0], w.enemies[0].pos);
        in[0].move_y = 1.0f;
        world_step(&w, in, WORLD_FIXED_DT);
    }
    ASSERT_FALSE(w.enemies[0].active);
    ASSERT_EQ_INT(w.players[0].stomps, 1);

    /* 3. Deploy Holo-Decoy & confirm active distraction */
    zero_inputs(in);
    in[0].buttons = INP_HOLO;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.decoy.active);

    /* 4. Restore Yule Cinema Projector Switch */
    zero_inputs(in);
    w.players[0].pos = vec3_make(19.0f, 1.01f, 20.5f);
    in[0].buttons = INP_USE;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.cinema_restored);
    ASSERT_EQ_INT(voxel_get(&w.voxels, 18, 3, 15), VOX_CINEMA_LIT);

    /* 5. Throw Remote Pipebomb at Cracked Backstage Voxel Wall (x=26, z=20) and detonate */
    zero_inputs(in);
    w.players[0].pos = vec3_make(23.5f, 1.01f, 20.0f);
    aim_player_at(&w.players[0], vec3_make(26.0f, 1.8f, 20.0f));
    in[0].select_weapon = WEAP_PIPEBOMB;
    in[0].buttons = INP_FIRE;
    world_step(&w, in, WORLD_FIXED_DT);
    zero_inputs(in);
    for (int t = 0; t < 18; ++t) world_step(&w, in, WORLD_FIXED_DT);
    in[0].buttons = INP_ALT;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.secret_wall_blasted);
    ASSERT_EQ_INT(voxel_get(&w.voxels, 26, 2, 20), VOX_AIR);

    /* Collect armor in Secret Retro Arcade + verify pickups */
    zero_inputs(in);
    w.players[0].pos = vec3_make(31.0f, 1.01f, 20.0f);
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.players[0].armor >= 75);

    /* 6. Jetpack upward flight verification */
    w.players[0].pos = vec3_make(36.0f, 1.01f, 32.0f);
    float start_y = w.players[0].pos.y;
    for (int t = 0; t < 75; ++t) {
        zero_inputs(in);
        in[0].buttons = INP_JETPACK;
        world_step(&w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w.players[0].pos.y > start_y + 3.0f);
    ASSERT_TRUE(w.players[0].jetpack_fuel < 95.0f);

    /* 7. Defeat Rooftop Battle-Mech Boss (enemy #5) using Pistol + Shrinker */
    w.players[0].pos = vec3_make(24.0f, 7.01f, 19.0f);
    for (int t = 0; t < 400 && !w.boss_defeated; ++t) {
        zero_inputs(in);
        aim_player_at(&w.players[0], vec3_add(w.enemies[5].pos, vec3_make(0, 1.3f, 0)));
        in[0].select_weapon = WEAP_PISTOL;
        in[0].buttons = INP_FIRE;
        world_step(&w, in, WORLD_FIXED_DT);
    }
    ASSERT_TRUE(w.boss_defeated);

    /* 8. Hit red Nuke-Exit button -> earn Golden Shades Shard #1 & return to Hub */
    zero_inputs(in);
    w.players[0].pos = vec3_make(35.0f, 7.01f, 18.0f);
    in[0].buttons = INP_USE;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w.scene, SCENE_HUB);
    ASSERT_TRUE((w.shard_mask & 1u) != 0u);
    ASSERT_EQ_INT(swieta_progress_count_shards(w.shard_mask), 1);
}

/* Tier 1 Automated Multiplayer Test: 4P Split-Screen + Network Snapshot Sync with Packet Loss */
static void test_automated_splitscreen_and_network_soak(void) {
    SwietaDateTime dec1 = {2026, 12, 1, 10, 0, 0};
    GameWorld host, client;
    world_init(&host, dec1, true, 4);
    world_init(&client, dec1, true, 2);

    PlayerInput hin[WORLD_MAX_PLAYERS];
    PlayerInput cin[WORLD_MAX_PLAYERS];
    zero_inputs(hin);
    zero_inputs(cin);

    /* All 4 split-screen players move in distinct directions */
    hin[0].move_y = 1.0f;
    hin[1].move_x = 1.0f;
    hin[2].move_y = -1.0f;
    hin[3].move_x = -1.0f;

    uint8_t wire[NET_MAX_PACKET_SIZE];
    for (int tick = 0; tick < 120; ++tick) {
        world_step(&host, hin, WORLD_FIXED_DT);
        /* Simulate 10% packet drop over WebRTC unreliable DataChannel */
        if (tick % 10 != 0) {
            NetSnapshotPacket snap, recv_snap;
            world_build_snapshot(&host, &snap);
            size_t len = net_encode_snapshot(&snap, wire, sizeof(wire));
            ASSERT_TRUE(len > 0);
            ASSERT_TRUE(net_decode_snapshot(wire, len, &recv_snap));
            world_apply_snapshot(&client, &recv_snap, 1);
        }
        world_step(&client, cin, WORLD_FIXED_DT);
    }

    ASSERT_EQ_INT(client.player_count, 4);
    ASSERT_TRUE(vec3_dist(host.players[0].pos, client.players[0].pos) < 0.75f);
    ASSERT_TRUE(vec3_dist(host.players[1].pos, client.players[1].pos) < 1.0f);

    /* Host warps to Day 1 -> snapshot seamlessly transitions Client to Day 1 */
    world_load_day(&host, 1);
    NetSnapshotPacket warp_snap, recv_warp;
    world_build_snapshot(&host, &warp_snap);
    size_t wlen = net_encode_snapshot(&warp_snap, wire, sizeof(wire));
    ASSERT_TRUE(net_decode_snapshot(wire, wlen, &recv_warp));
    world_apply_snapshot(&client, &recv_warp, 1);
    ASSERT_EQ_INT(client.scene, SCENE_DAY01);
}

int main(void) {
    RUN_TEST(test_automated_day01_full_playthrough);
    RUN_TEST(test_automated_splitscreen_and_network_soak);
    printf("\nAutomated game tests run: %d | Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
