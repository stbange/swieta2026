#include "test_harness.h"

#include "audio/retro_audio.h"
#include "core/calendar.h"
#include "core/math3d.h"
#include "net/net_proto.h"
#include "platform/touch.h"
#include "render/voxel_mesher.h"
#include "sim/world.h"

static void test_math3d_vectors_and_angles(void) {
    ASSERT_NEAR(sw_clampf(-5.0f, 0.0f, 10.0f), 0.0f, 1e-5f);
    ASSERT_NEAR(sw_clampf(15.0f, 0.0f, 10.0f), 10.0f, 1e-5f);
    ASSERT_NEAR(sw_clampf(4.0f, 0.0f, 10.0f), 4.0f, 1e-5f);
    ASSERT_NEAR(sw_lerpf(2.0f, 6.0f, 0.25f), 3.0f, 1e-5f);
    ASSERT_NEAR(sw_wrap_angle(2.5f * SW_PI), 0.5f * SW_PI, 1e-4f);
    ASSERT_NEAR(sw_wrap_angle(-2.5f * SW_PI), -0.5f * SW_PI, 1e-4f);

    Vec3 a = vec3_make(1.0f, 2.0f, 3.0f);
    Vec3 b = vec3_make(4.0f, 5.0f, 6.0f);
    Vec3 sum = vec3_add(a, b);
    ASSERT_NEAR(sum.x, 5.0f, 1e-5f);
    ASSERT_NEAR(sum.y, 7.0f, 1e-5f);
    ASSERT_NEAR(sum.z, 9.0f, 1e-5f);

    Vec3 diff = vec3_sub(b, a);
    ASSERT_NEAR(vec3_len(diff), sqrtf(27.0f), 1e-4f);
    ASSERT_NEAR(vec3_dist(a, b), sqrtf(27.0f), 1e-4f);
    ASSERT_NEAR(vec3_dot(a, b), 32.0f, 1e-5f);

    Vec3 c = vec3_cross(vec3_make(1, 0, 0), vec3_make(0, 1, 0));
    ASSERT_NEAR(c.z, 1.0f, 1e-5f);

    Vec3 zero_n = vec3_norm(vec3_make(0, 0, 0));
    ASSERT_NEAR(vec3_len(zero_n), 0.0f, 1e-5f);
    Vec3 n = vec3_norm(vec3_make(3, 4, 0));
    ASSERT_NEAR(n.x, 0.6f, 1e-5f);
    ASSERT_NEAR(n.y, 0.8f, 1e-5f);
    Vec3 mid = vec3_lerp(a, b, 0.5f);
    ASSERT_NEAR(mid.x, 2.5f, 1e-5f);
}

static void test_math3d_matrices_and_collisions(void) {
    Mat4 id = mat4_identity();
    Mat4 tr = mat4_translate(vec3_make(2.0f, 3.0f, 4.0f));
    Mat4 sc = mat4_scale(vec3_make(2.0f, 2.0f, 2.0f));
    Mat4 m = mat4_mul(tr, sc);
    Vec4 p = {1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 r = mat4_mul_vec4(m, p);
    ASSERT_NEAR(r.x, 4.0f, 1e-5f);
    ASSERT_NEAR(r.y, 5.0f, 1e-5f);
    ASSERT_NEAR(r.z, 6.0f, 1e-5f);

    Mat4 rx = mat4_rotate_x(SW_PI * 0.5f);
    Vec4 py = {0.0f, 1.0f, 0.0f, 1.0f};
    Vec4 ry = mat4_mul_vec4(rx, py);
    ASSERT_NEAR(ry.z, 1.0f, 1e-4f);

    Mat4 rym = mat4_rotate_y(SW_PI * 0.5f);
    Vec4 px = {1.0f, 0.0f, 0.0f, 1.0f};
    Vec4 rpx = mat4_mul_vec4(rym, px);
    ASSERT_NEAR(rpx.z, -1.0f, 1e-4f);

    Mat4 persp = mat4_perspective(SW_PI * 0.5f, 16.0f / 9.0f, 0.1f, 100.0f);
    ASSERT_NEAR(persp.m[11], -1.0f, 1e-5f);
    Mat4 ortho = mat4_ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 50.0f);
    ASSERT_NEAR(ortho.m[0], 0.1f, 1e-5f);
    Mat4 view = mat4_look_at(vec3_make(0, 0, 5), vec3_make(0, 0, 0), vec3_make(0, 1, 0));
    Vec4 origin_v = mat4_mul_vec4(view, (Vec4){0, 0, 0, 1});
    ASSERT_NEAR(origin_v.z, -5.0f, 1e-4f);
    (void)id;

    AABB b1 = {vec3_make(0, 0, 0), vec3_make(2, 2, 2)};
    AABB b2 = {vec3_make(1, 1, 1), vec3_make(3, 3, 3)};
    AABB b3 = {vec3_make(5, 5, 5), vec3_make(6, 6, 6)};
    ASSERT_TRUE(aabb_overlap(b1, b2));
    ASSERT_FALSE(aabb_overlap(b1, b3));
    ASSERT_TRUE(sphere_aabb_overlap(vec3_make(2.5f, 1.0f, 1.0f), 0.6f, b1));
    ASSERT_FALSE(sphere_aabb_overlap(vec3_make(5.0f, 1.0f, 1.0f), 0.5f, b1));

    float t = 0.0f;
    ASSERT_TRUE(ray_aabb_hit(vec3_make(-2, 1, 1), vec3_make(1, 0, 0), b1, 10.0f, &t));
    ASSERT_NEAR(t, 2.0f, 1e-4f);
    ASSERT_TRUE(ray_aabb_hit(vec3_make(4, 1, 1), vec3_make(-1, 0, 0), b1, 10.0f, &t));
    ASSERT_FALSE(ray_aabb_hit(vec3_make(-2, 5, 1), vec3_make(1, 0, 0), b1, 10.0f, &t));
    ASSERT_FALSE(ray_aabb_hit(vec3_make(-2, 1, 1), vec3_make(-1, 0, 0), b1, 10.0f, &t));
}

static void test_calendar_gating_and_progress(void) {
    ASSERT_TRUE(swieta_portal_meta(0) == NULL);
    ASSERT_TRUE(swieta_portal_meta(25) == NULL);
    const PortalMeta *m1 = swieta_portal_meta(1);
    const PortalMeta *m24 = swieta_portal_meta(24);
    ASSERT_TRUE(m1 != NULL && m24 != NULL);
    ASSERT_EQ_INT(m1->act, 1);
    ASSERT_EQ_INT(m24->act, 4);

    SwietaDateTime nov = {2026, 11, 19, 12, 0, 0};
    ASSERT_EQ_INT(swieta_calendar_unlocked_count(nov, false), 0);
    ASSERT_EQ_INT(swieta_calendar_unlocked_count(nov, true), 24);
    ASSERT_TRUE(swieta_portal_seconds_until_unlock(1, nov, false) > 0);
    ASSERT_EQ_INT(swieta_portal_seconds_until_unlock(1, nov, true), 0);
    ASSERT_EQ_INT(swieta_portal_seconds_until_unlock(0, nov, false), -1);
    ASSERT_FALSE(swieta_portal_is_unlocked(99, nov, true));

    SwietaDateTime dec6 = {2026, 12, 6, 0, 0, 0};
    ASSERT_EQ_INT(swieta_calendar_unlocked_count(dec6, false), 6);
    ASSERT_TRUE(swieta_portal_is_unlocked(6, dec6, false));
    ASSERT_FALSE(swieta_portal_is_unlocked(7, dec6, false));

    SwietaDateTime dec25 = {2026, 12, 25, 10, 0, 0};
    ASSERT_EQ_INT(swieta_calendar_unlocked_count(dec25, false), 24);
    SwietaDateTime jan = {2027, 1, 2, 0, 0, 0};
    ASSERT_TRUE(swieta_datetime_to_unix(jan) > swieta_datetime_to_unix(dec25));

    char buf[16];
    ASSERT_FALSE(swieta_progress_encode(0, NULL, 16));
    ASSERT_FALSE(swieta_progress_encode(0, buf, 4));
    ASSERT_TRUE(swieta_progress_encode(0x00A5F001u, buf, sizeof(buf)));
    uint32_t decoded = 0;
    ASSERT_TRUE(swieta_progress_decode(buf, &decoded));
    ASSERT_EQ_INT(decoded, 0x00A5F001u);
    ASSERT_TRUE(swieta_progress_decode("SW26:0000ab", &decoded));
    ASSERT_EQ_INT(decoded, 0xABu);
    ASSERT_FALSE(swieta_progress_decode(NULL, &decoded));
    ASSERT_FALSE(swieta_progress_decode("BAD!:000001", &decoded));
    ASSERT_FALSE(swieta_progress_decode("SW26:ZZZZZZ", &decoded));
    ASSERT_EQ_INT(swieta_progress_count_shards(0x00000007u), 3);
}

static void test_voxel_world_and_mesher(void) {
    VoxelWorld vw;
    voxel_world_clear(&vw);
    ASSERT_FALSE(voxel_in_bounds(-1, 0, 0));
    ASSERT_EQ_INT(voxel_get(&vw, -1, 0, 0), VOX_AIR);

    /* 3x3x3 solid cube -> center cell has 0 exposed faces; outer 26 cells have 54 faces */
    voxel_fill_box(&vw, 6, 4, 6, 4, 2, 4, VOX_STONE);
    ASSERT_TRUE(voxel_is_solid(&vw, 5, 3, 5));

    VoxelMesh mesh;
    voxel_mesh_init(&mesh);
    ASSERT_TRUE(voxel_build_mesh(&vw, &mesh));
    ASSERT_EQ_INT((int)mesh.vertex_count, 54 * 4);
    ASSERT_EQ_INT((int)mesh.index_count, 54 * 6);

    /* Cracked destruction + replace_all + raycast */
    voxel_fill_box(&vw, 10, 1, 10, 12, 3, 10, VOX_CRACKED);
    int blown = voxel_explode_cracked(&vw, vec3_make(11.5f, 2.5f, 10.5f), 2.5f);
    ASSERT_TRUE(blown > 0);
    ASSERT_EQ_INT(voxel_explode_cracked(&vw, vec3_make(11.5f, 2.5f, 10.5f), -1.0f), 0);

    voxel_set(&vw, 20, 2, 20, VOX_CINEMA_STATIC);
    ASSERT_EQ_INT(voxel_replace_all(&vw, VOX_CINEMA_STATIC, VOX_CINEMA_LIT), 1);
    ASSERT_EQ_INT(voxel_replace_all(&vw, VOX_CINEMA_LIT, VOX_CINEMA_LIT), 0);

    /* Hit every material color path */
    for (uint8_t t = 0; t <= VOX_CINEMA_LIT + 1; ++t) {
        float rgba[4];
        float em = 0.0f;
        voxel_type_color(t, rgba, &em);
        ASSERT_NEAR(rgba[3], 1.0f, 1e-5f);
    }

    float dist = 0.0f;
    Vec3 hit = vec3_make(0, 0, 0);
    ASSERT_TRUE(voxel_raycast(&vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(1, 0, 0), 10.0f, &dist, &hit));
    ASSERT_TRUE(dist > 1.5f && dist < 3.0f);
    ASSERT_FALSE(voxel_raycast(&vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(0, 0, 0), 10.0f, &dist, &hit));
    ASSERT_FALSE(voxel_raycast(&vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(-1, 0, 0), 5.0f, &dist, &hit));
    ASSERT_FALSE(voxel_build_mesh(NULL, &mesh));

    voxel_mesh_free(&mesh);
}

static void test_net_proto_and_room_codes(void) {
    char code[NET_ROOM_CODE_LEN + 1];
    net_room_code_generate(42u, code);
    ASSERT_TRUE(net_room_code_valid(code));
    ASSERT_FALSE(net_room_code_valid(NULL));
    ASSERT_FALSE(net_room_code_valid("SHORT"));
    ASSERT_FALSE(net_room_code_valid("BAD0OO")); /* 0 and O disallowed */

    uint8_t buf[NET_MAX_PACKET_SIZE];
    NetInputPacket in = {1, 77, 0.5f, -0.75f, 1.2f, -0.3f, INP_FIRE | INP_JUMP, WEAP_SHRINKER};
    ASSERT_EQ_INT((int)net_encode_input(&in, buf, 4), 0);
    size_t n = net_encode_input(&in, buf, sizeof(buf));
    ASSERT_TRUE(n > 6);
    ASSERT_EQ_INT(net_peek_packet_type(buf, n), NET_PKT_INPUT);
    ASSERT_EQ_INT(net_peek_packet_type(buf, 2), 0);

    NetInputPacket out_in;
    ASSERT_TRUE(net_decode_input(buf, n, &out_in));
    ASSERT_EQ_INT(out_in.seq, 77);
    ASSERT_NEAR(out_in.move_y, -0.75f, 1e-5f);
    /* Corrupt CRC */
    buf[n - 1] ^= 0xFFu;
    ASSERT_FALSE(net_decode_input(buf, n, &out_in));

    NetSnapshotPacket snap;
    memset(&snap, 0, sizeof(snap));
    snap.tick = 1234;
    snap.scene_id = SCENE_DAY01;
    snap.active_day = 1;
    snap.shard_mask = 0x5u;
    snap.player_count = 2;
    snap.players[0] = (NetPlayerState){1, vec3_make(1, 2, 3), 0.4f, 0.1f, 90, 250, WEAP_PIPEBOMB};
    snap.players[1] = (NetPlayerState){1, vec3_make(4, 5, 6), -0.4f, -0.1f, 80, 100, WEAP_PISTOL};

    size_t sn = net_encode_snapshot(&snap, buf, sizeof(buf));
    ASSERT_TRUE(sn > 18);
    NetSnapshotPacket out_snap;
    ASSERT_TRUE(net_decode_snapshot(buf, sn, &out_snap));
    ASSERT_EQ_INT(out_snap.tick, 1234);
    ASSERT_EQ_INT(out_snap.player_count, 2);
    ASSERT_NEAR(out_snap.players[1].pos.z, 6.0f, 1e-5f);
    ASSERT_FALSE(net_decode_snapshot(buf, sn - 1, &out_snap));
    snap.player_count = 9;
    ASSERT_EQ_INT((int)net_encode_snapshot(&snap, buf, sizeof(buf)), 0);

    NetEventPacket ev = {9, 3, 1, 500};
    size_t en = net_encode_event(&ev, buf, sizeof(buf));
    NetEventPacket out_ev;
    ASSERT_TRUE(net_decode_event(buf, en, &out_ev));
    ASSERT_EQ_INT(out_ev.arg_b, 500);
    ASSERT_EQ_INT((int)net_encode_event(&ev, buf, 2), 0);

    NetPlayerState lerped = net_lerp_player_state(out_snap.players[0], out_snap.players[1], 0.5f);
    ASSERT_NEAR(lerped.pos.x, 2.5f, 1e-4f);
}

static void test_touch_controller(void) {
    TouchController tc;
    touch_init(&tc);
    ASSERT_FALSE(tc.enabled);

    int btn_count = 0;
    const TouchButtonRect *btns = touch_get_layout(&btn_count);
    ASSERT_TRUE(btn_count >= 6 && btns != NULL);

    /* Finger 1: left virtual stick pushed forward-right into sprint zone */
    touch_on_down(&tc, 10, 0.16f, 0.76f);
    ASSERT_TRUE(tc.enabled);
    ASSERT_TRUE(tc.stick_active);
    touch_on_move(&tc, 10, 0.165f, 0.755f); /* inside deadzone */
    ASSERT_NEAR(tc.move_y, 0.0f, 1e-4f);
    touch_on_move(&tc, 10, 0.30f, 0.50f); /* outer ring sprint */
    ASSERT_TRUE(tc.sprint);
    ASSERT_TRUE(tc.move_y > 0.5f);

    /* Finger 2: right swipe look */
    touch_on_down(&tc, 20, 0.55f, 0.35f);
    touch_on_move(&tc, 20, 0.65f, 0.30f);
    float dx = 0.0f, dy = 0.0f;
    touch_consume_look(&tc, &dx, &dy);
    ASSERT_TRUE(dx > 0.2f);
    ASSERT_TRUE(dy < 0.0f);

    /* Finger 3: Fire button press & release */
    touch_on_down(&tc, 30, btns[0].cx, btns[0].cy);
    ASSERT_TRUE((tc.buttons & TOUCH_BTN_FIRE) != 0u);
    touch_on_up(&tc, 30);
    ASSERT_TRUE((tc.buttons & TOUCH_BTN_FIRE) == 0u);

    /* Re-using existing finger id & disabling resets state */
    touch_on_down(&tc, 10, 0.16f, 0.76f);
    touch_on_up(&tc, 20);
    touch_set_enabled(&tc, false);
    ASSERT_FALSE(tc.enabled);
    ASSERT_FALSE(tc.stick_active);
}

static void test_retro_audio_synth(void) {
    RetroAudioSynth synth;
    retro_audio_init(&synth);
    retro_audio_trigger_events(&synth, SFX_PISTOL | SFX_SHRINK | SFX_STOMP | SFX_THROW |
                                       SFX_EXPLODE | SFX_JETPACK | SFX_PICKUP | SFX_SWITCH |
                                       SFX_PORTAL | SFX_HURT | SFX_SHARD);
    float buf[256];
    retro_audio_render_samples(&synth, SCENE_HUB, 3, buf, 256);
    float energy = 0.0f;
    for (int i = 0; i < 256; ++i) energy += fabsf(buf[i]);
    ASSERT_TRUE(energy > 1.0f);

    retro_audio_render_samples(&synth, SCENE_DAY01, 1, buf, 256);
    synth.muted = true;
    retro_audio_render_samples(&synth, SCENE_HUB, 0, buf, 64);
    ASSERT_NEAR(buf[0], 0.0f, 1e-6f);
}

static void test_world_hub_preview_and_combat_edges(void) {
    SwietaDateTime nov = {2026, 11, 19, 12, 0, 0};
    GameWorld w;
    world_init(&w, nov, false, 1);
    ASSERT_EQ_INT(w.scene, SCENE_HUB);

    /* Locked portal cannot be entered when unlock_all=false */
    world_load_day(&w, 1);
    ASSERT_EQ_INT(w.scene, SCENE_HUB);

    /* Vertical grounding stability: zero bounce while standing still or walking on flat floor */
    PlayerInput in[WORLD_MAX_PLAYERS];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) in[i].select_weapon = -1;
    for (int t = 0; t < 30; ++t) {
        world_step(&w, in, WORLD_FIXED_DT);
        ASSERT_NEAR(w.players[0].pos.y, 1.0f, 0.015f);
        ASSERT_TRUE(w.players[0].on_ground);
    }
    in[0].move_y = 1.0f;
    for (int t = 0; t < 20; ++t) {
        world_step(&w, in, WORLD_FIXED_DT);
        ASSERT_NEAR(w.players[0].pos.y, 1.0f, 0.015f);
        ASSERT_TRUE(w.players[0].on_ground);
    }
    in[0].move_y = 0.0f;

    /* Jump + ceiling clamp test */
    w.players[0].pos = vec3_make(22.5f, 10.2f, 34.0f);
    w.players[0].on_ground = true;
    in[0].buttons = INP_JUMP;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w.players[0].pos.y <= 10.30f);
    in[0].buttons = 0;

    /* Interacting with SANTA-BIOS terminal */
    w.players[0].pos = vec3_make(24.0f, 1.0f, 18.0f);
    in[0].buttons = INP_USE;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w.sound_events & SFX_SWITCH) != 0u);

    /* Unlock-all enters preview Day 15 (Magic Carpet 2) & collects preview shard & returns */
    w.unlock_all = true;
    world_load_day(&w, 15);
    ASSERT_EQ_INT(w.scene, SCENE_PREVIEW);
    w.players[0].pos = vec3_make(24.0f, 2.5f, 21.0f);
    in[0].buttons = 0;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w.shard_mask & (1u << 14)) != 0u);

    /* Walk into return portal -> back to hub */
    w.players[0].pos = w.return_portal_pos;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w.scene, SCENE_HUB);

    /* Player damage, death, and respawn on Day 1 */
    world_load_day(&w, 1);
    w.players[0].hp = 5;
    w.players[0].armor = 0;
    w.projectiles[0].active = true;
    w.projectiles[0].kind = PROJ_BOSS_ROCKET;
    w.projectiles[0].owner_player = -1;
    w.projectiles[0].pos = vec3_add(w.players[0].pos, vec3_make(0, 1.0f, 0));
    w.projectiles[0].vel = vec3_make(0, 0, 0);
    w.projectiles[0].life = 1.0f;
    world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w.players[0].hp, 0);
    for (int t = 0; t < 130; ++t) world_step(&w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w.players[0].hp, 100);
}

int main(void) {
    RUN_TEST(test_math3d_vectors_and_angles);
    RUN_TEST(test_math3d_matrices_and_collisions);
    RUN_TEST(test_calendar_gating_and_progress);
    RUN_TEST(test_voxel_world_and_mesher);
    RUN_TEST(test_net_proto_and_room_codes);
    RUN_TEST(test_touch_controller);
    RUN_TEST(test_retro_audio_synth);
    RUN_TEST(test_world_hub_preview_and_combat_edges);
    printf("\nUnit tests run: %d | Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
