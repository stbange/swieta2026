#include "net/net_proto.h"

#include <string.h>

static const char kRoomAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static uint16_t crc16_simple(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void write_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_f32(uint8_t *p, float f) {
    uint32_t u = 0;
    memcpy(&u, &f, sizeof(u));
    write_u32(p, u);
}

static float read_f32(const uint8_t *p) {
    uint32_t u = read_u32(p);
    float f = 0.0f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

bool net_room_code_valid(const char *code) {
    if (!code || strlen(code) != NET_ROOM_CODE_LEN) {
        return false;
    }
    for (int i = 0; i < NET_ROOM_CODE_LEN; ++i) {
        if (strchr(kRoomAlphabet, code[i]) == NULL) {
            return false;
        }
    }
    return true;
}

void net_room_code_generate(uint32_t seed, char out_code[NET_ROOM_CODE_LEN + 1]) {
    if (!out_code) return;
    uint32_t s = seed ^ 0x20261224u;
    size_t alpha_len = sizeof(kRoomAlphabet) - 1;
    for (int i = 0; i < NET_ROOM_CODE_LEN; ++i) {
        s = s * 1664525u + 1013904223u;
        out_code[i] = kRoomAlphabet[(s >> 16) % alpha_len];
    }
    out_code[NET_ROOM_CODE_LEN] = '\0';
}

/* Header layout (6 bytes): [magic u16][type u8][flags u8][crc16 u16] */
static size_t finalize_packet(uint8_t *buf, uint8_t type, size_t payload_len) {
    write_u16(buf + 0, NET_MAGIC);
    buf[2] = type;
    buf[3] = 0;
    uint16_t crc = crc16_simple(buf + 6, payload_len);
    write_u16(buf + 4, crc);
    return 6 + payload_len;
}

static bool validate_packet(const uint8_t *buf, size_t len, uint8_t expected_type, size_t min_payload) {
    if (!buf || len < 6 + min_payload) return false;
    if (read_u16(buf + 0) != NET_MAGIC) return false;
    if (buf[2] != expected_type) return false;
    uint16_t stored = read_u16(buf + 4);
    uint16_t computed = crc16_simple(buf + 6, len - 6);
    return stored == computed;
}

uint8_t net_peek_packet_type(const uint8_t *buf, size_t len) {
    if (!buf || len < 6 || read_u16(buf) != NET_MAGIC) {
        return 0;
    }
    return buf[2];
}

size_t net_encode_input(const NetInputPacket *pkt, uint8_t *out_buf, size_t cap) {
    const size_t payload = 24;
    if (!pkt || !out_buf || cap < 6 + payload) return 0;
    uint8_t *p = out_buf + 6;
    p[0] = pkt->player_slot;
    p[1] = pkt->weapon;
    write_u16(p + 2, pkt->buttons);
    write_u32(p + 4, pkt->seq);
    write_f32(p + 8, pkt->move_x);
    write_f32(p + 12, pkt->move_y);
    write_f32(p + 16, pkt->yaw);
    write_f32(p + 20, pkt->pitch);
    return finalize_packet(out_buf, NET_PKT_INPUT, payload);
}

bool net_decode_input(const uint8_t *buf, size_t len, NetInputPacket *out_pkt) {
    if (!out_pkt || !validate_packet(buf, len, NET_PKT_INPUT, 24)) return false;
    const uint8_t *p = buf + 6;
    out_pkt->player_slot = p[0];
    out_pkt->weapon = p[1];
    out_pkt->buttons = read_u16(p + 2);
    out_pkt->seq = read_u32(p + 4);
    out_pkt->move_x = read_f32(p + 8);
    out_pkt->move_y = read_f32(p + 12);
    out_pkt->yaw = read_f32(p + 16);
    out_pkt->pitch = read_f32(p + 20);
    return true;
}

size_t net_encode_snapshot(const NetSnapshotPacket *pkt, uint8_t *out_buf, size_t cap) {
    if (!pkt || !out_buf || pkt->player_count > NET_MAX_PLAYERS) return 0;
    size_t payload = 12 + (size_t)pkt->player_count * 26;
    if (cap < 6 + payload) return 0;

    uint8_t *p = out_buf + 6;
    write_u32(p + 0, pkt->tick);
    p[4] = pkt->scene_id;
    p[5] = pkt->active_day;
    p[6] = pkt->player_count;
    p[7] = 0;
    write_u32(p + 8, pkt->shard_mask);
    p += 12;

    for (uint8_t i = 0; i < pkt->player_count; ++i) {
        const NetPlayerState *ps = &pkt->players[i];
        p[0] = ps->active;
        p[1] = ps->weapon;
        write_u16(p + 2, (uint16_t)ps->hp);
        write_u16(p + 4, (uint16_t)ps->score);
        write_f32(p + 6, ps->pos.x);
        write_f32(p + 10, ps->pos.y);
        write_f32(p + 14, ps->pos.z);
        write_f32(p + 18, ps->yaw);
        write_f32(p + 22, ps->pitch);
        p += 26;
    }
    return finalize_packet(out_buf, NET_PKT_SNAPSHOT, payload);
}

bool net_decode_snapshot(const uint8_t *buf, size_t len, NetSnapshotPacket *out_pkt) {
    if (!out_pkt || !validate_packet(buf, len, NET_PKT_SNAPSHOT, 12)) return false;
    const uint8_t *p = buf + 6;
    uint8_t pcount = p[6];
    if (pcount > NET_MAX_PLAYERS) return false;
    size_t expected = 6 + 12 + (size_t)pcount * 26;
    if (len != expected) return false;

    memset(out_pkt, 0, sizeof(*out_pkt));
    out_pkt->tick = read_u32(p + 0);
    out_pkt->scene_id = p[4];
    out_pkt->active_day = p[5];
    out_pkt->player_count = pcount;
    out_pkt->shard_mask = read_u32(p + 8);
    p += 12;

    for (uint8_t i = 0; i < pcount; ++i) {
        NetPlayerState *ps = &out_pkt->players[i];
        ps->active = p[0];
        ps->weapon = p[1];
        ps->hp = (int16_t)read_u16(p + 2);
        ps->score = (int16_t)read_u16(p + 4);
        ps->pos.x = read_f32(p + 6);
        ps->pos.y = read_f32(p + 10);
        ps->pos.z = read_f32(p + 14);
        ps->yaw = read_f32(p + 18);
        ps->pitch = read_f32(p + 22);
        p += 26;
    }
    return true;
}

size_t net_encode_event(const NetEventPacket *pkt, uint8_t *out_buf, size_t cap) {
    const size_t payload = 8;
    if (!pkt || !out_buf || cap < 6 + payload) return 0;
    uint8_t *p = out_buf + 6;
    write_u32(p + 0, pkt->seq);
    p[4] = pkt->event_kind;
    p[5] = pkt->arg_a;
    write_u16(p + 6, pkt->arg_b);
    return finalize_packet(out_buf, NET_PKT_EVENT, payload);
}

bool net_decode_event(const uint8_t *buf, size_t len, NetEventPacket *out_pkt) {
    if (!out_pkt || !validate_packet(buf, len, NET_PKT_EVENT, 8)) return false;
    const uint8_t *p = buf + 6;
    out_pkt->seq = read_u32(p + 0);
    out_pkt->event_kind = p[4];
    out_pkt->arg_a = p[5];
    out_pkt->arg_b = read_u16(p + 6);
    return true;
}

NetPlayerState net_lerp_player_state(NetPlayerState a, NetPlayerState b, float t) {
    t = sw_clampf(t, 0.0f, 1.0f);
    NetPlayerState r = b;
    r.pos = vec3_lerp(a.pos, b.pos, t);
    float dyaw = sw_wrap_angle(b.yaw - a.yaw);
    r.yaw = sw_wrap_angle(a.yaw + dyaw * t);
    r.pitch = sw_lerpf(a.pitch, b.pitch, t);
    return r;
}
