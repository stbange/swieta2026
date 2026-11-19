#ifndef SWIETA_NET_PROTO_H
#define SWIETA_NET_PROTO_H

#include "core/math3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_MAGIC 0x5332u /* "S2" */
#define NET_MAX_PLAYERS 4
#define NET_MAX_PACKET_SIZE 512
#define NET_ROOM_CODE_LEN 6

typedef enum {
    NET_PKT_INPUT = 1,
    NET_PKT_SNAPSHOT = 2,
    NET_PKT_EVENT = 3
} NetPacketType;

typedef struct {
    uint8_t player_slot;
    uint32_t seq;
    float move_x;
    float move_y;
    float yaw;
    float pitch;
    uint16_t buttons;
    uint8_t weapon;
} NetInputPacket;

typedef struct {
    uint8_t active;
    Vec3 pos;
    float yaw;
    float pitch;
    int16_t hp;
    int16_t score;
    uint8_t weapon;
} NetPlayerState;

typedef struct {
    uint32_t tick;
    uint8_t scene_id;
    uint8_t active_day;
    uint32_t shard_mask;
    uint8_t player_count;
    NetPlayerState players[NET_MAX_PLAYERS];
} NetSnapshotPacket;

typedef struct {
    uint32_t seq;
    uint8_t event_kind;
    uint8_t arg_a;
    uint16_t arg_b;
} NetEventPacket;

bool net_room_code_valid(const char *code);
void net_room_code_generate(uint32_t seed, char out_code[NET_ROOM_CODE_LEN + 1]);

size_t net_encode_input(const NetInputPacket *pkt, uint8_t *out_buf, size_t cap);
bool net_decode_input(const uint8_t *buf, size_t len, NetInputPacket *out_pkt);

size_t net_encode_snapshot(const NetSnapshotPacket *pkt, uint8_t *out_buf, size_t cap);
bool net_decode_snapshot(const uint8_t *buf, size_t len, NetSnapshotPacket *out_pkt);

size_t net_encode_event(const NetEventPacket *pkt, uint8_t *out_buf, size_t cap);
bool net_decode_event(const uint8_t *buf, size_t len, NetEventPacket *out_pkt);

uint8_t net_peek_packet_type(const uint8_t *buf, size_t len);
NetPlayerState net_lerp_player_state(NetPlayerState a, NetPlayerState b, float t);

#endif /* SWIETA_NET_PROTO_H */
