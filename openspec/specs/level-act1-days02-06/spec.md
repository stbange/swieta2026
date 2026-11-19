# level-act1-days02-06 Specification

## Purpose
Governs Act I levels Day 2 (Tomb Raider homage), Day 3 (Counter-Strike homage), Day 4 (Quake homage), Day 5 (Blood homage), and Day 6 (Doom Act I climax), including their signature weapons, environmental puzzles, enemy behaviors, bosses, and Memory Shard awards.

## Requirements

### Requirement: Day 2 Tomb Raider Mechanics and 4-Beat Temple Progression
Day 2 (*Vault of the Pixel Sun*) SHALL provide Dual Pistols with soft lock-on and acrobatic dodge bursts, first-person Ledge Mantling, pushable stone block pressure-plate puzzles, a Flooded Cistern water-level switch, throwable dynamic-light Magnesium Flares, a triggered Rolling Boulder corridor, and a T-Rex courtyard boss awarding the Scion Prism shard (bit 1).

#### Scenario: Solving the push-block plate and flooded cistern puzzle
- **WHEN** the player pushes a stone Push-Block onto the golden temple pressure plate and flips the Cistern Water Valve switch
- **THEN** the sealed temple gate opens and the Cistern water level rises so the player can swim across to the upper sanctuary

#### Scenario: Illuminating dark tombs and defeating the T-Rex
- **WHEN** the player tosses a green Magnesium Flare, triggers the Rolling Boulder corridor, and defeats the Valley T-Rex
- **THEN** an active dynamic green light illuminates the tomb, the boulder rolls down the hallway, and interacting with the sunlit Scion Prism awards Shard #2 and returns to the Grand Hall

### Requirement: Day 3 Counter-Strike Economy, Utility Grenades, and C4 Bomb Defusal
Day 3 (*de_yule_dust*) SHALL implement a kill-funded Cash Economy (`$`) with interactive Buy-Zone Supply Crates (Kevlar, Deagle, Carbine, Scoped Sniper, Defuse Kit), movement-accuracy rifle spread, Flashbang enemy blinding, Smoke Grenade line-of-sight blocking, and 2-site C4 Bomb Defusal awarding the Defusal Pliers shard (bit 2).

#### Scenario: Buying weapons and a Defuse Kit at a Buy-Zone Crate
- **WHEN** the player earns cash by eliminating Syndicate Troopers and interacts with a Buy-Zone Supply Crate
- **THEN** the player's cash is deducted and upgraded weapons or the Defuse Kit (halving bomb defusal time) are unlocked

#### Scenario: Blinding enemies with Flashbang and defusing Bombsites A and B
- **WHEN** the player deploys a Flashbang near enemies, deploys a Smoke screen, and completes C4 defusal at both Bombsite A and Bombsite B
- **THEN** flashed enemies stop firing while blinded, smoke blocks sniper fire, and defusing both sites awards Shard #3

### Requirement: Day 4 Quake Rocket Jumping, Quad Damage, Slipgate Telefrags, and Shambler Boss
Day 4 (*Slipgate of Brass & Rust*) SHALL implement Rocket Launcher self-knockback Rocket Jumping, a 4× Quad Damage Rune with blue dynamic lighting, Super Nailgun and Thunderbolt (including underwater AoE discharge), Jump Pads, Slipgate Teleporters capable of telefragging enemies, and a Shambler-homage boss awarding the Slipgate Rune shard (bit 3).

#### Scenario: Rocket jumping to high ledges and triggering Quad Damage
- **WHEN** the player collects the Quad Damage Rune and fires a Rocket downward while jumping
- **THEN** the player launches vertically with rocket knockback and weapon attacks deal 4× damage while bathed in blue dynamic light

#### Scenario: Telefragging via Slipgate and discharging Thunderbolt in water
- **WHEN** the player enters a Slipgate whose destination is occupied by an enemy or fires the Thunderbolt while submerged in water
- **THEN** the destination enemy is instantaneously telefragged, and underwater Thunderbolt discharge deals AoE electric damage to enemies in the water

### Requirement: Day 5 Blood Alt-Fire Horror Arsenal, Burn DoT, and Carnival Climax
Day 5 (*Carnival of the Damned*) SHALL provide secondary/alt-fire modes across its horror arsenal: Pitchfork melee, Flare Gun (sticking burn DoT + alt-fire starburst), Aerosol Flamethrower (+ alt-fire thrown Molotov), Dynamite TNT (+ alt-fire proximity mine), and Voodoo Doll (+ alt-fire AoE curse), progressing across Ghost Train and Big Top to defeat the Gargoyle Matriarch for the Crimson Chalice shard (bit 4).

#### Scenario: Igniting cultists with Flare Gun burn DoT and alt-fire attacks
- **WHEN** the player hits an enemy with the Flare Gun and uses alt-fire TNT or Voodoo Doll attacks
- **THEN** the struck enemy suffers periodic burning damage over time with firelight and alt-fire modes trigger distinct starburst, Molotov, proximity-mine, or AoE curse effects

#### Scenario: Ghost Train breach and Gargoyle Matriarch defeat
- **WHEN** the player activates the Ghost Train switch, blasts the Carnival crypt wall with TNT, defeats the Gargoyle Matriarch, and claims the Crimson Chalice
- **THEN** Shard #5 is awarded and the player returns to the Grand Hall

### Requirement: Day 6 Doom Keycard Labyrinth, Infighting, Barrel Chains, and Cyber-Titan Climax
Day 6 (*E1M24: Brimstone Hangar*) SHALL serve as the Act I climax featuring Chainsaw, Super Shotgun, Chaingun, Rocket Launcher, and Yule-BFG 9000, a Blue/Yellow/Red 3-Keycard labyrinth, explosive barrel chain reactions, demon-vs-demon monster infighting, Berserk Pack and Radiation Suit powerups, and the Cyber-Titan boss awarding the Hellfire Key shard (bit 5).

#### Scenario: Barrel chain reaction and demon monster infighting
- **WHEN** the player shoots an explosive red barrel adjacent to another barrel, or an enemy demon projectile strikes another active demon
- **THEN** adjacent barrels detonate in sequence dealing splash damage, and the struck demon switches aggro to attack the demon that hit it

#### Scenario: Completing the 3-keycard loop, Radiation Suit slime crossing, and Cyber-Titan boss
- **WHEN** the player collects the Radiation Suit to safely cross glowing slime, unlocks Blue, Yellow, and Red keycard doors, defeats the Cyber-Titan, and activates the Hellfire Exit
- **THEN** Shard #6 is awarded, completing Act I on the Grand Tree
