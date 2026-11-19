# level-day01-duke Specification

## Purpose
Governs Day 1 (Neon Strip '96, Duke Nukem 3D homage), including the Shrink-Ray + Boot Stomp, Remote Pipebomb voxel destruction, Jetpack + Holo-Decoy, Yule Cinema projector switch, Rooftop Battle-Mech boss, and Golden Shades Memory Shard extraction.

## Requirements

### Requirement: Day 1 Signature Weapons and Mechanics
Day 1 (*Neon Strip '96*) SHALL provide a Blaster Pistol, Shrink-Ray with Boot Stomp, Remote Pipebombs capable of destroying cracked voxel walls, and a fuel-limited Jetpack with Holo-Decoy.

#### Scenario: Shrinking and stomping an enemy
- **WHEN** the player fires the Shrink-Ray at an Alien Trooper and then walks into contact with the shrunk enemy
- **THEN** the enemy shrinks to miniature scale on hit and is immediately defeated by the boot stomp on contact

#### Scenario: Destroying a cracked voxel wall with a Remote Pipebomb
- **WHEN** the player throws a Remote Pipebomb near a cracked voxel wall and triggers detonation
- **THEN** the cracked voxels are cleared into walkable space and nearby enemies take splash damage

#### Scenario: Jetpack elevation and Holo-Decoy distraction
- **WHEN** the player activates the Jetpack with remaining fuel and deploys a Holo-Decoy
- **THEN** the player ascends vertically while fuel drains, and nearby enemies prioritize targeting the active Holo-Decoy

### Requirement: Day 1 Mission Completion Loop
Day 1 SHALL progress through activating the Yule Cinema projector switch, defeating the Rooftop Battle-Mech boss, and activating the Nuke-Exit button to earn the Golden Shades Memory Shard.

#### Scenario: Full Day 1 completion
- **WHEN** the player flips the cinema switch, defeats the Rooftop Battle-Mech, and interacts with the Nuke-Exit button
- **THEN** the Golden Shades Memory Shard is awarded and the player returns to the Grand Festive Hall

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
