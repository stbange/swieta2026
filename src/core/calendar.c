#include "core/calendar.h"

#include <stdio.h>
#include <string.h>

static const PortalMeta kPortals[SWIETA_PORTAL_COUNT] = {
    { 1, 1, "Duke Nukem 3D",             "Neon Strip '96",           "Golden Shades",    1.00f, 0.25f, 0.70f, true  },
    { 2, 1, "Tomb Raider",               "Vault of the Pixel Sun",   "Scion Prism",      0.95f, 0.72f, 0.25f, false },
    { 3, 1, "Counter-Strike",            "de_yule_dust",             "Defusal Pliers",   0.85f, 0.75f, 0.45f, false },
    { 4, 1, "Quake",                     "Slipgate of Brass & Rust", "Slipgate Rune",    0.75f, 0.42f, 0.20f, false },
    { 5, 1, "Blood",                     "Carnival of the Damned",   "Crimson Chalice",  0.88f, 0.12f, 0.18f, false },
    { 6, 1, "Doom",                      "E1M24: Brimstone Hangar",  "Hellfire Key",     0.95f, 0.28f, 0.12f, false },
    { 7, 2, "Half-Life",                 "Sector C: Cascade",        "Resonance Crystal",1.00f, 0.55f, 0.10f, false },
    { 8, 2, "Command & Conquer",         "Crystal Dawn Basin",       "Ion Core",         0.20f, 0.95f, 0.35f, false },
    { 9, 2, "MechWarrior 2",             "Trial of Iron Snow",       "Gyro Matrix",      0.55f, 0.75f, 0.95f, false },
    {10, 2, "Descent",                   "Sublevel Zero-G",          "Fusion Cell",      0.30f, 0.85f, 1.00f, false },
    {11, 2, "Starsiege: Tribes",         "Glacier Spinfusor",        "Tribal Flag",      0.40f, 0.65f, 1.00f, false },
    {12, 2, "Warcraft II",               "Tides of Timber & Oil",    "Runic Crest",      0.20f, 0.65f, 0.35f, false },
    {13, 3, "Diablo",                    "Cathedral of Ash",         "Soulstone Ember",  0.75f, 0.15f, 0.15f, false },
    {14, 3, "Secret of Mana",            "Grove of the Ring",        "Mana Seed",        0.35f, 0.92f, 0.55f, false },
    {15, 3, "Magic Carpet 2",            "Dunes of the Nether",      "Djinn Pearl",      0.92f, 0.50f, 0.20f, false },
    {16, 3, "Jedi Knight",               "Valley of the Saber",      "Kyber Shard",      0.25f, 0.95f, 0.85f, false },
    {17, 3, "Syndicate Wars",            "Rain Over Sector 9",       "Neural Chip",      0.80f, 0.20f, 0.95f, false },
    {18, 3, "System Shock",              "Citadel Deck Zero",        "Neural Override",  0.15f, 0.95f, 0.70f, false },
    {19, 4, "X-COM: UFO Defense",        "Nightfall Crash Site",     "Elerium Orb",      0.35f, 0.95f, 0.25f, false },
    {20, 4, "Unreal",                    "Spire of Floating Waters", "Nali Star",        0.45f, 0.60f, 1.00f, false },
    {21, 4, "StarCraft",                 "Outpost Koprulu",          "Khaydarin Shard",  0.25f, 0.55f, 1.00f, false },
    {22, 4, "Age of Empires",            "Citadel of Four Ages",     "Golden Relic",     0.95f, 0.80f, 0.25f, false },
    {23, 4, "Total Annihilation",        "Metal & Energy Front",     "Commander Core",   0.70f, 0.80f, 0.90f, false },
    {24, 4, "C&C: Red Alert",            "Operation: Chrono-Eve",    "The Yule Star",    0.95f, 0.18f, 0.25f, false }
};

const PortalMeta *swieta_portal_meta(int day) {
    if (day < 1 || day > SWIETA_PORTAL_COUNT) {
        return NULL;
    }
    return &kPortals[day - 1];
}

/* Howard Hinnant civil-from-days algorithm (exact UTC Gregorian epoch days) */
static int64_t days_from_civil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = (int64_t)y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int64_t swieta_datetime_to_unix(SwietaDateTime dt) {
    int64_t days = days_from_civil(dt.year, dt.month, dt.day);
    return days * 86400 + (int64_t)dt.hour * 3600 + (int64_t)dt.minute * 60 + (int64_t)dt.second;
}

int64_t swieta_portal_seconds_until_unlock(int day, SwietaDateTime now, bool unlock_all) {
    if (day < 1 || day > SWIETA_PORTAL_COUNT) {
        return -1;
    }
    if (unlock_all) {
        return 0;
    }
    SwietaDateTime target = {SWIETA_YEAR, SWIETA_MONTH, day, 0, 0, 0};
    int64_t diff = swieta_datetime_to_unix(target) - swieta_datetime_to_unix(now);
    return (diff > 0) ? diff : 0;
}

bool swieta_portal_is_unlocked(int day, SwietaDateTime now, bool unlock_all) {
    if (day < 1 || day > SWIETA_PORTAL_COUNT) {
        return false;
    }
    return swieta_portal_seconds_until_unlock(day, now, unlock_all) == 0;
}

int swieta_calendar_unlocked_count(SwietaDateTime now, bool unlock_all) {
    int count = 0;
    for (int d = 1; d <= SWIETA_PORTAL_COUNT; ++d) {
        if (swieta_portal_is_unlocked(d, now, unlock_all)) {
            ++count;
        }
    }
    return count;
}

bool swieta_progress_encode(uint32_t shard_mask, char *out_buf, size_t buf_size) {
    if (!out_buf || buf_size < 12) {
        return false;
    }
    uint32_t masked = shard_mask & 0x00FFFFFFu;
    int written = snprintf(out_buf, buf_size, "SW26:%06X", masked);
    return written > 0 && (size_t)written < buf_size;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

bool swieta_progress_decode(const char *str, uint32_t *out_shard_mask) {
    if (!str || !out_shard_mask) {
        return false;
    }
    if (strncmp(str, "SW26:", 5) != 0 || strlen(str) != 11) {
        return false;
    }
    uint32_t val = 0;
    for (int i = 0; i < 6; ++i) {
        int h = hex_val(str[5 + i]);
        if (h < 0) {
            return false;
        }
        val = (val << 4) | (uint32_t)h;
    }
    *out_shard_mask = val & 0x00FFFFFFu;
    return true;
}

int swieta_progress_count_shards(uint32_t shard_mask) {
    uint32_t m = shard_mask & 0x00FFFFFFu;
    int count = 0;
    while (m != 0u) {
        count += (int)(m & 1u);
        m >>= 1u;
    }
    return count;
}
