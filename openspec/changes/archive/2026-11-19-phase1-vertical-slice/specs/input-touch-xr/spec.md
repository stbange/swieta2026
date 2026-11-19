## ADDED Requirements

### Requirement: Adaptive Mobile Touch Controls
The web client SHALL automatically activate virtual touch controls upon touch input and translate multi-touch finger gestures into movement, camera look, shooting, jumping, weapon switching, and gadget actions.

#### Scenario: Multi-touch move and look simultaneously
- **WHEN** one finger drags in the left movement zone while a second finger drags in the right look zone
- **THEN** the touch controller emits simultaneous normalized movement vectors and camera yaw/pitch deltas

#### Scenario: Touch action button press
- **WHEN** a finger touches within the Fire, Alt-Fire, Jump, or Interact button bounds
- **THEN** the corresponding digital action bit is set until the finger is released

### Requirement: WebXR Stereoscopic Support
The web client SHALL expose a WebXR session bridge capable of rendering stereoscopic left and right eye views with tracked head pose and controller aim when an immersive VR session is active.

#### Scenario: WebXR session unavailable fallback
- **WHEN** the browser does not support WebXR immersive-vr
- **THEN** the game continues running normally in flat WebGL2 mode without errors
