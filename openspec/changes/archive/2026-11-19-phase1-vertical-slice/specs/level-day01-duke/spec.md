## ADDED Requirements

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
