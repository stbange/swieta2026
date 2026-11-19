## ADDED Requirements

### Requirement: Four-Beat Day 1 Progression, Keycards, Checkpoints, and Secrets
Day 1 (*Neon Strip '96*) SHALL span four interconnected zones (Neon Boulevard, Yule Cinema, Rooftop Broadcast Skyline, and Penthouse Boss Arena) gated by Blue and Red Keycards, featuring four progression checkpoints, three discoverable secrets, breakable glass windows, and two destructible Broadcast Relays.

#### Scenario: Keycard door unlocking
- **WHEN** a player approaches a Blue or Red Keycard door without the matching keycard
- **THEN** the door remains sealed; once the player collects the matching keycard and interacts or approaches, the door opens permanently

#### Scenario: Checkpoint respawn preservation
- **WHEN** a player dies after reaching Checkpoint 2 in the Yule Cinema
- **THEN** the player respawns at Checkpoint 2 rather than the initial alley start

### Requirement: Mirror Self-Shrink Vent Puzzle and Laser Tripmines
Day 1 SHALL support Shrink-Ray beam reflection off Mirror surfaces to shrink the player for crawling through 1-voxel air ducts, Shotgun spread fire, and deployable wall-mounted Laser Tripmines that detonate when enemies cross their beam.

#### Scenario: Self-shrinking via mirror reflection to traverse miniature vent
- **WHEN** a player fires the Shrink-Ray at a Mirror surface in the Cinema Mirror Room
- **THEN** the reflected shrink orb shrinks the player to miniature height (`<= 0.65m`), allowing passage through the 1-voxel projection booth air duct

#### Scenario: Enemy crossing an armed Laser Tripmine
- **WHEN** a player plants a Laser Tripmine on a wall and an enemy crosses the projected laser beam
- **THEN** the tripmine detonates and deals splash damage to nearby enemies

### Requirement: Multi-Phase Rooftop Battle-Mech Boss
The Rooftop Battle-Mech boss SHALL transition through three combat phases (standard rockets, shield + Trooper reinforcements at 66% HP, and overdrive barrage at 33% HP) before unlocking the Nuke-Exit switch.

#### Scenario: Boss phase transitions and Nuke-Exit unlock
- **WHEN** the Rooftop Battle-Mech HP drops below 66% and then 33% and finally reaches 0
- **THEN** phase 2 reinforcements spawn, phase 3 activates, and defeating the boss unlocks the Nuke-Exit button for the Golden Shades shard
