#ifndef SWIETA_CALENDAR_H
#define SWIETA_CALENDAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWIETA_PORTAL_COUNT 24
#define SWIETA_YEAR 2026
#define SWIETA_MONTH 12

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} SwietaDateTime;

typedef struct {
    int day;                  /* 1..24 */
    int act;                  /* 1..4 */
    const char *inspiration;
    const char *portal_title;
    const char *shard_name;
    float color_r;
    float color_g;
    float color_b;
    bool full_playable;       /* true for implemented full levels */
} PortalMeta;

const PortalMeta *swieta_portal_meta(int day);

int64_t swieta_datetime_to_unix(SwietaDateTime dt);
bool swieta_portal_is_unlocked(int day, SwietaDateTime now, bool unlock_all);
int64_t swieta_portal_seconds_until_unlock(int day, SwietaDateTime now, bool unlock_all);
int swieta_calendar_unlocked_count(SwietaDateTime now, bool unlock_all);

/* Bitmask (bits 0..23) serialization for persistent Memory Shards */
bool swieta_progress_encode(uint32_t shard_mask, char *out_buf, size_t buf_size);
bool swieta_progress_decode(const char *str, uint32_t *out_shard_mask);
int swieta_progress_count_shards(uint32_t shard_mask);

#endif /* SWIETA_CALENDAR_H */
