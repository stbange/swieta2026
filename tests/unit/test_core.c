#include "test_harness.h"

#include "audio/retro_audio.h"
#include "core/calendar.h"
#include "core/math3d.h"
#include "net/net_proto.h"
#include "platform/touch.h"
#include "render/voxel_mesher.h"
#include "sim/world.h"

#include <string.h>

/* Static storage keeps test stacks tiny across Windows/Linux/macOS */
static VoxelWorld g_test_vw;
static GameWorld g_test_world;

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

static void test_voxel_world_and_chunked_mesher(void) {
    VoxelWorld *vw = &g_test_vw;
    voxel_world_clear(vw);
    ASSERT_FALSE(voxel_in_bounds(-1, 0, 0));
    ASSERT_FALSE(voxel_in_bounds(VOX_WORLD_X, 0, 0));
    ASSERT_EQ_INT(voxel_get(vw, -1, 0, 0), VOX_AIR);

    int idx = voxel_chunk_index(3, 1, 5);
    int cx = 0, cy = 0, cz = 0;
    voxel_chunk_coords(idx, &cx, &cy, &cz);
    ASSERT_EQ_INT(cx, 3);
    ASSERT_EQ_INT(cy, 1);
    ASSERT_EQ_INT(cz, 5);

    /* 3x3x3 solid cube inside chunk (0,0,0) -> 54 exposed faces */
    voxel_fill_box(vw, 6, 4, 6, 4, 2, 4, VOX_STONE);
    ASSERT_TRUE(voxel_is_solid(vw, 5, 3, 5));
    ASSERT_TRUE(voxel_is_opaque(VOX_STONE));
    ASSERT_FALSE(voxel_is_opaque(VOX_GLASS));
    ASSERT_FALSE(voxel_is_opaque(VOX_WATER));
    ASSERT_FALSE(voxel_is_opaque(VOX_AIR));

    /* Passable liquid / hazard volumes */
    voxel_set(vw, 8, 2, 8, VOX_WATER);
    voxel_set(vw, 9, 2, 8, VOX_WATER);
    voxel_set(vw, 10, 2, 8, VOX_LAVA);
    voxel_set(vw, 11, 2, 8, VOX_SLIME);
    voxel_set(vw, 12, 2, 8, VOX_SPIKES);
    ASSERT_FALSE(voxel_is_solid(vw, 8, 2, 8));
    ASSERT_FALSE(voxel_is_solid(vw, 10, 2, 8));
    ASSERT_FALSE(voxel_is_solid(vw, 11, 2, 8));
    ASSERT_FALSE(voxel_is_solid(vw, 12, 2, 8));

    VoxelMesh mesh;
    voxel_mesh_init(&mesh);
    ASSERT_TRUE(voxel_build_mesh(vw, &mesh));
    ASSERT_TRUE(mesh.vertex_count > 54 * 4);

    ASSERT_TRUE(voxel_build_chunk_mesh(vw, voxel_chunk_index(0, 0, 0), &mesh));
    ASSERT_FALSE(voxel_build_chunk_mesh(vw, -1, &mesh));
    ASSERT_FALSE(voxel_build_chunk_mesh(vw, VOX_CHUNK_COUNT, &mesh));

    /* Seam-aware chunk dirtying across chunk borders */
    memset(vw->chunk_dirty, 0, sizeof(vw->chunk_dirty));
    voxel_set(vw, 15, 15, 15, VOX_WOOD);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(0, 0, 0)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(1, 0, 0)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(0, 1, 0)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(0, 0, 1)]);

    memset(vw->chunk_dirty, 0, sizeof(vw->chunk_dirty));
    voxel_set(vw, 16, 16, 16, VOX_MIRROR);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(1, 1, 1)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(0, 1, 1)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(1, 0, 1)]);
    ASSERT_TRUE(vw->chunk_dirty[voxel_chunk_index(1, 1, 0)]);

    voxel_fill_box(vw, 20, 1, 20, 22, 3, 20, VOX_CRACKED);
    ASSERT_TRUE(voxel_explode_cracked(vw, vec3_make(21.5f, 2.5f, 20.5f), 2.5f) > 0);
    ASSERT_EQ_INT(voxel_explode_cracked(vw, vec3_make(21.5f, 2.5f, 20.5f), -1.0f), 0);

    voxel_fill_box(vw, 30, 1, 30, 32, 3, 30, VOX_GLASS);
    ASSERT_TRUE(voxel_break_glass(vw, vec3_make(31.5f, 2.5f, 30.5f), 2.0f) > 0);
    ASSERT_EQ_INT(voxel_break_glass(vw, vec3_make(31.5f, 2.5f, 30.5f), -1.0f), 0);

    voxel_set(vw, 40, 2, 40, VOX_CINEMA_STATIC);
    ASSERT_EQ_INT(voxel_replace_all(vw, VOX_CINEMA_STATIC, VOX_CINEMA_LIT), 1);
    ASSERT_EQ_INT(voxel_replace_all(vw, VOX_CINEMA_LIT, VOX_CINEMA_LIT), 0);

    /* Cover every material color & atlas tile path (0..VOX_TYPE_COUNT) */
    for (uint8_t t = 0; t <= VOX_TYPE_COUNT; ++t) {
        float rgba[4];
        float em = 0.0f;
        voxel_type_color(t, rgba, &em);
        ASSERT_TRUE(rgba[3] > 0.0f && rgba[3] <= 1.0f);
        int tile = voxel_type_atlas_tile(t);
        ASSERT_TRUE(tile >= 0 && tile < VOX_ATLAS_COLS * VOX_ATLAS_ROWS);
    }

    VoxelRayHit rh;
    ASSERT_TRUE(voxel_raycast_ext(vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(1, 0, 0), 10.0f, &rh));
    ASSERT_NEAR(rh.normal.x, -1.0f, 1e-4f);
    ASSERT_EQ_INT(rh.type, VOX_STONE);

    ASSERT_TRUE(voxel_raycast_ext(vw, vec3_make(5.5f, 7.0f, 5.5f), vec3_make(0, -1, 0), 10.0f, &rh));
    ASSERT_NEAR(rh.normal.y, 1.0f, 1e-4f);

    ASSERT_TRUE(voxel_raycast_ext(vw, vec3_make(5.5f, 3.5f, 9.0f), vec3_make(0, 0, -1), 10.0f, &rh));
    ASSERT_NEAR(rh.normal.z, 1.0f, 1e-4f);

    float dist = 0.0f;
    Vec3 hit = vec3_make(0, 0, 0);
    ASSERT_TRUE(voxel_raycast(vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(1, 0, 0), 10.0f, &dist, &hit));
    ASSERT_TRUE(dist > 1.5f && dist < 3.0f);
    ASSERT_FALSE(voxel_raycast(vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(0, 0, 0), 10.0f, &dist, &hit));
    ASSERT_FALSE(voxel_raycast(vw, vec3_make(2.0f, 3.5f, 5.5f), vec3_make(-1, 0, 0), 5.0f, &dist, &hit));
    ASSERT_FALSE(voxel_build_mesh(NULL, &mesh));

    voxel_mesh_free(&mesh);
}

static void test_net_proto_and_room_codes(void) {
    char code[NET_ROOM_CODE_LEN + 1];
    net_room_code_generate(42u, code);
    ASSERT_TRUE(net_room_code_valid(code));
    ASSERT_FALSE(net_room_code_valid(NULL));
    ASSERT_FALSE(net_room_code_valid("SHORT"));
    ASSERT_FALSE(net_room_code_valid("BAD0OO"));

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

    touch_on_down(&tc, 10, 0.16f, 0.76f);
    ASSERT_TRUE(tc.enabled);
    ASSERT_TRUE(tc.stick_active);
    touch_on_move(&tc, 10, 0.165f, 0.755f);
    ASSERT_NEAR(tc.move_y, 0.0f, 1e-4f);
    touch_on_move(&tc, 10, 0.30f, 0.50f);
    ASSERT_TRUE(tc.sprint);
    ASSERT_TRUE(tc.move_y > 0.5f);

    touch_on_down(&tc, 20, 0.55f, 0.35f);
    touch_on_move(&tc, 20, 0.65f, 0.30f);
    float dx = 0.0f, dy = 0.0f;
    touch_consume_look(&tc, &dx, &dy);
    ASSERT_TRUE(dx > 0.2f);
    ASSERT_TRUE(dy < 0.0f);

    touch_on_down(&tc, 30, btns[0].cx, btns[0].cy);
    ASSERT_TRUE((tc.buttons & TOUCH_BTN_FIRE) != 0u);
    touch_on_up(&tc, 30);
    ASSERT_TRUE((tc.buttons & TOUCH_BTN_FIRE) == 0u);

    touch_on_down(&tc, 10, 0.16f, 0.76f);
    touch_on_up(&tc, 20);
    touch_set_enabled(&tc, false);
    ASSERT_FALSE(tc.enabled);
    ASSERT_FALSE(tc.stick_active);
}

static void test_retro_audio_synth(void) {
    RetroAudioSynth synth;
    retro_audio_init(&synth);
    retro_audio_trigger_events(&synth, SFX_PISTOL | SFX_SHOTGUN | SFX_SHRINK | SFX_STOMP | SFX_THROW |
                                       SFX_EXPLODE | SFX_JETPACK | SFX_PICKUP | SFX_SWITCH |
                                       SFX_PORTAL | SFX_HURT | SFX_SHARD | SFX_GLASS | SFX_DOOR | SFX_SECRET |
                                       SFX_ROCKET | SFX_LIGHTNING | SFX_FLAME | SFX_MELEE | SFX_TELEPORT |
                                       SFX_QUAD | SFX_FLASH);
    float buf[256];
    retro_audio_render_samples(&synth, SCENE_HUB, 3, buf, 256);
    float energy = 0.0f;
    for (int i = 0; i < 256; ++i) energy += fabsf(buf[i]);
    ASSERT_TRUE(energy > 1.0f);

    retro_audio_render_samples(&synth, SCENE_DAY01, 1, buf, 256);
    retro_audio_render_samples(&synth, SCENE_DAY04, 4, buf, 256);
    retro_audio_render_samples(&synth, SCENE_DAY06, 6, buf, 256);
    synth.muted = true;
    retro_audio_render_samples(&synth, SCENE_HUB, 0, buf, 64);
    ASSERT_NEAR(buf[0], 0.0f, 1e-6f);
}

static void test_world_hub_preview_and_act1_edges(void) {
    SwietaDateTime nov = {2026, 11, 19, 12, 0, 0};
    GameWorld *w = &g_test_world;
    world_init(w, nov, false, 1);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);

    /* Query helpers for all 25 weapons + out-of-range guards */
    for (int wid = 0; wid <= WEAP_COUNT; ++wid) {
        const char *nm = world_weapon_short_name((WeaponId)wid);
        ASSERT_TRUE(nm != NULL && nm[0] != '\0');
        int am = world_player_weapon_ammo(&w->players[0], (WeaponId)wid);
        ASSERT_TRUE(am >= 0);
    }
    ASSERT_EQ_INT(world_player_weapon_ammo(NULL, WEAP_PISTOL), 0);
    ASSERT_EQ_INT(world_slot_weapon(w, 99), WEAP_PISTOL);

    /* Split-screen player scaling up/down */
    world_set_player_count(w, 3);
    ASSERT_EQ_INT(w->player_count, 3);
    world_set_player_count(w, 0);
    ASSERT_EQ_INT(w->player_count, 1);

    /* Locked portal cannot be entered when unlock_all=false */
    world_load_day(w, 1);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);

    /* Vertical grounding stability: zero bounce while standing still or walking on flat floor */
    PlayerInput in[WORLD_MAX_PLAYERS];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < WORLD_MAX_PLAYERS; ++i) in[i].select_weapon = -1;
    for (int t = 0; t < 30; ++t) {
        world_step(w, in, WORLD_FIXED_DT);
        ASSERT_NEAR(w->players[0].pos.y, 1.0f, 0.015f);
        ASSERT_TRUE(w->players[0].on_ground);
    }
    in[0].move_y = 1.0f;
    for (int t = 0; t < 20; ++t) {
        world_step(w, in, WORLD_FIXED_DT);
        ASSERT_NEAR(w->players[0].pos.y, 1.0f, 0.015f);
        ASSERT_TRUE(w->players[0].on_ground);
    }
    in[0].move_y = 0.0f;

    /* Jump + ceiling clamp test against Hub vaulted ceiling (y=16) */
    w->players[0].pos = vec3_make(62.5f, 14.2f, 84.5f);
    w->players[0].on_ground = true;
    in[0].buttons = INP_JUMP;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].pos.y <= 14.30f);
    in[0].buttons = 0;

    /* Cycle all 6 SANTA-BIOS 1999 terminal logs */
    w->players[0].pos = w->santa_bios_pos;
    for (int k = 0; k < 6; ++k) {
        in[0].buttons = INP_USE;
        world_step(w, in, WORLD_FIXED_DT);
        ASSERT_TRUE((w->sound_events & SFX_SWITCH) != 0u);
        in[0].buttons = 0;
        world_step(w, in, WORLD_FIXED_DT);
    }
    ASSERT_EQ_INT(w->santa_log_index, 0);

    /* Unlock-all enters preview Day 15 (Magic Carpet 2) & collects preview shard & returns */
    w->unlock_all = true;
    world_load_day(w, 15);
    ASSERT_EQ_INT(w->scene, SCENE_PREVIEW);
    w->players[0].pos = vec3_make(64.5f, 2.5f, 59.5f);
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE((w->shard_mask & (1u << 14)) != 0u);
    w->players[0].pos = w->return_portal_pos;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->scene, SCENE_HUB);

    /* Day 1 edge cases: locked door feedback, early nuke-exit guard, weapon cycling, safe vent un-shrink */
    world_load_day(w, 1);
    w->players[0].pos = vec3_make(28.0f, 1.0f, 74.0f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->doors[0].open);
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);

    w->players[0].pos = vec3_make(30.5f, 12.0f, 17.5f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->scene, SCENE_DAY01);
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);

    w->players[0].weapon = WEAP_PISTOL;
    w->players[0].has_shotgun = false;
    in[0].buttons = INP_NEXT_WEAP;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->players[0].weapon, WEAP_SHRINKER);
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);

    w->players[0].has_shotgun = true;
    w->players[0].owned_weapons |= (1u << WEAP_SHOTGUN);
    w->players[0].weapon = WEAP_SHOTGUN;
    w->players[0].ammo_shells = 0;
    in[0].buttons = INP_FIRE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->players[0].weapon, WEAP_PISTOL);
    in[0].buttons = 0;

    /* Safe anti-stuck un-shrink inside 1-voxel air vent */
    w->players[0].pos = vec3_make(15.5f, 1.0f, 59.5f);
    w->players[0].self_shrunk_timer = 0.01f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].self_shrunk_timer > 0.0f);
    w->players[0].pos = vec3_make(15.5f, 1.0f, 54.5f);
    w->players[0].self_shrunk_timer = 0.01f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_NEAR(w->players[0].self_shrunk_timer, 0.0f, 1e-5f);

    /* Shrink-Ray deals direct damage to mechanical Turret (enemy #10) */
    int turret_hp_before = w->enemies[10].hp;
    w->projectiles[0].active = true;
    w->projectiles[0].kind = PROJ_SHRINK_ORB;
    w->projectiles[0].owner_player = 0;
    w->projectiles[0].owner_enemy = -1;
    w->projectiles[0].reflected = false;
    w->projectiles[0].pos = vec3_add(w->enemies[10].pos, vec3_make(0, 0.9f, 0));
    w->projectiles[0].vel = vec3_make(0, 0, 0);
    w->projectiles[0].life = 1.0f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->enemies[10].hp < turret_hp_before);

    /* Player death & respawn at checkpoint preserving keys & shotgun */
    w->players[0].keys = KEY_BLUE;
    w->players[0].has_shotgun = true;
    w->players[0].hp = 5;
    w->players[0].armor = 0;
    w->projectiles[1].active = true;
    w->projectiles[1].kind = PROJ_BOSS_ROCKET;
    w->projectiles[1].owner_player = -1;
    w->projectiles[1].owner_enemy = -1;
    w->projectiles[1].pos = vec3_add(w->players[0].pos, vec3_make(0, 1.0f, 0));
    w->projectiles[1].vel = vec3_make(0, 0, 0);
    w->projectiles[1].life = 1.0f;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->players[0].hp, 0);
    for (int t = 0; t < 130; ++t) world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->players[0].hp, 100);
    ASSERT_TRUE(w->players[0].has_shotgun);
    ASSERT_TRUE((w->players[0].keys & KEY_BLUE) != 0u);

    /* Day 2 Ledge Mantle + Spike hazard + early exit guard */
    world_load_day(w, 2);
    w->players[0].pos = vec3_make(44.5f, 5.0f, 10.5f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->scene, SCENE_DAY02); /* Blocked until T-Rex defeated */
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);

    /* Ledge Mantle up 2-voxel wall at Cistern (x=55.5 -> 56.0, y=1 -> y=3) */
    w->players[0].pos = vec3_make(63.5f, 1.0f, 99.0f); /* Standing on VOX_SPIKES */
    int hp_sp = w->players[0].hp;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].hp < hp_sp);

    /* Mantle climb along +X and -Z ledges */
    voxel_fill_box(&w->voxels, 48, 1, 110, 48, 2, 110, VOX_SANDSTONE);
    w->players[0].pos = vec3_make(47.62f, 1.0f, 110.5f);
    w->players[0].yaw = SW_PI * 0.5f;
    in[0].move_y = 1.0f;
    in[0].buttons = INP_JUMP;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].pos.y >= 2.5f);
    in[0].move_y = 0.0f;
    in[0].buttons = 0;

    /* Day 3 Buy Crate insufficient cash + early exit guard */
    world_load_day(w, 3);
    w->players[0].cash = 50;
    w->players[0].pos = vec3_make(60.5f, 1.0f, 108.5f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->players[0].has_defuse_kit);
    in[0].buttons = 0;
    world_step(w, in, WORLD_FIXED_DT);
    w->players[0].pos = vec3_make(92.5f, 1.0f, 17.5f);
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_EQ_INT(w->scene, SCENE_DAY03); /* Blocked until 2 C4 defused */
    in[0].buttons = 0;

    /* Day 4 Lava hazard + empty ammo weapon fallback checks */
    world_load_day(w, 4);
    w->players[0].pos = vec3_make(30.0f, 1.0f, 98.0f); /* Above VOX_LAVA */
    int hp_lava = w->players[0].hp;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].hp < hp_lava);

    /* Empty-ammo fallback checks across Act I weapons */
    w->players[0].owned_weapons = 0xFFFFFFFFu;
    w->players[0].ammo_bullets = 0;
    w->players[0].ammo_rockets = 0;
    w->players[0].ammo_cells = 0;
    w->players[0].ammo_shells = 0;
    static const WeaponId kFallbacks[] = {
        WEAP_MAGNUM, WEAP_DEAGLE, WEAP_CARBINE, WEAP_SNIPER, WEAP_NAILGUN,
        WEAP_ROCKET, WEAP_THUNDERBOLT, WEAP_FLAREGUN, WEAP_AEROSOL, WEAP_VOODOO,
        WEAP_SUPER_SHOTGUN, WEAP_CHAINGUN, WEAP_BFG
    };
    for (size_t k = 0; k < sizeof(kFallbacks) / sizeof(kFallbacks[0]); ++k) {
        w->players[0].weapon = kFallbacks[k];
        w->players[0].fire_cooldown = 0.0f;
        in[0].buttons = INP_FIRE;
        world_step(w, in, WORLD_FIXED_DT);
        ASSERT_TRUE(w->players[0].weapon != kFallbacks[k]);
        in[0].buttons = 0;
    }

    /* Day 6 Slime hazard without Radiation Suit + locked Yellow Door prompt */
    world_load_day(w, 6);
    w->players[0].pos = vec3_make(46.0f, 1.0f, 82.0f); /* Above VOX_SLIME */
    int hp_slime = w->players[0].hp;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_TRUE(w->players[0].hp < hp_slime);

    w->players[0].pos = vec3_make(82.5f, 1.0f, 63.0f); /* Beside locked Yellow Door */
    in[0].buttons = INP_USE;
    world_step(w, in, WORLD_FIXED_DT);
    ASSERT_FALSE(w->doors[1].open);
}

int main(void) {
    RUN_TEST(test_math3d_vectors_and_angles);
    RUN_TEST(test_math3d_matrices_and_collisions);
    RUN_TEST(test_calendar_gating_and_progress);
    RUN_TEST(test_voxel_world_and_chunked_mesher);
    RUN_TEST(test_net_proto_and_room_codes);
    RUN_TEST(test_touch_controller);
    RUN_TEST(test_retro_audio_synth);
    RUN_TEST(test_world_hub_preview_and_act1_edges);
    printf("\nUnit tests run: %d | Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
