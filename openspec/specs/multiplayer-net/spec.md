# multiplayer-net Specification

## Purpose
Governs local 2-4 player split-screen viewports and host-authoritative peer-to-peer WebRTC DataChannel multiplayer including binary input/snapshot serialization and room-code/SDP signaling.

## Requirements

### Requirement: Local Split-Screen Multiplayer
The game SHALL support 1, 2, or 4 simultaneous local players with independent cameras, viewports, HUDs, and inputs sharing the same world simulation.

#### Scenario: Two-player split-screen simulation
- **WHEN** local player count is set to 2 and Player 1 and Player 2 issue distinct movement inputs
- **THEN** both avatars move independently in the shared world and render into two non-overlapping viewports

### Requirement: Network Multiplayer Protocol and WebRTC Transport
The game SHALL serialize player inputs and world state snapshots into deterministic compact binary packets suitable for unreliable/unordered WebRTC DataChannels and support room-code relay plus manual copy-paste SDP signaling.

#### Scenario: Binary input and snapshot roundtrip
- **WHEN** an input packet or world snapshot is encoded and decoded across the network protocol layer
- **THEN** all player transforms, active level state, shard flags, and entity states match identically and malformed packets are rejected safely
