# advent-hub Specification

## Purpose
Governs the 3D Grand Festive Hall hub world, 24-portal calendar unlock gating (2026-12-01 to 2026-12-24), Playtest Unlock-All mode, and persistent Memory Shard progression.

## Requirements

### Requirement: Advent Calendar Portal Gating
The system SHALL gate each of the 24 portals to unlock on its corresponding day from `2026-12-01` through `2026-12-24` in standard mode, while supporting an explicit Playtest Unlock-All override.

#### Scenario: Standard mode before December 1, 2026
- **WHEN** the current date is `2026-11-19` and Playtest Unlock-All is disabled
- **THEN** all 24 portals report as locked and provide positive countdown seconds until `2026-12-01`

#### Scenario: Standard mode on December 6, 2026
- **WHEN** the current date is `2026-12-06` and Playtest Unlock-All is disabled
- **THEN** portals 1 through 6 are unlocked and portals 7 through 24 remain locked

#### Scenario: Playtest Unlock-All enabled
- **WHEN** Playtest Unlock-All is enabled via configuration or URL flag
- **THEN** all 24 portals report as unlocked regardless of the system date

### Requirement: Grand Festive Hall Navigation and Progression
The system SHALL place players in the 3D Grand Festive Hall containing 24 identifiable portals, a central tree tracking collected Memory Shards, and seamless portal entry and return.

#### Scenario: Entering an unlocked portal
- **WHEN** a player walks into an unlocked portal trigger in the Grand Festive Hall
- **THEN** the active scene transitions to that portal's level and initializes its mission state

#### Scenario: Returning with a collected Memory Shard
- **WHEN** a player completes Day 1 and returns to the Grand Festive Hall
- **THEN** Memory Shard #1 is recorded as collected, the Grand Tree ornament #1 is illuminated, and progress persists

### Requirement: Vaulted Grand Hall Cathedral and Cycling SANTA-BIOS Logs
The Grand Festive Hall SHALL occupy an expanded vaulted cathedral with distinct ornament lights corresponding to individual Memory Shards (1–24) and interactive SANTA-BIOS terminal entries cycling through 1999 LAN-party story logs.

#### Scenario: Cycling SANTA-BIOS 1999 LAN logs
- **WHEN** a player repeatedly interacts with the SANTA-BIOS terminal in the Grand Festive Hall
- **THEN** each interaction advances to the next 1999 LAN-party archive log entry and wraps around cleanly
