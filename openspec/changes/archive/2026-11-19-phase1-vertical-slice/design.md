# Design: Phase 1 Vertical Slice

## Architecture Overview
- **Hexagonal Pure-C Core (`swieta_core` static library):** Contains `math3d`, `calendar`, `voxel_mesher`, `world`, `net_proto`, and `touch` with zero SDL3 or OpenGL dependencies. All logic runs in deterministic fixed `60 Hz` ticks (`dt = 1/60`), enabling ≥95% line/branch unit test coverage and 1000× speed headless automated game playthroughs.
- **SDL3 + OpenGL Shell (`swieta2026` executable / WASM):** Uses `SDL_MAIN_USE_CALLBACKS` (`SDL_AppInit`, `SDL_AppIterate`, `SDL_AppEvent`, `SDL_AppQuit`) for unified Windows, Linux, and single-threaded Emscripten execution (compatible with static GitHub Pages without COOP/COEP headers).
- **Unified Shader Subset:** Shared GLSL 3.30 Core (desktop) / GLSL 300 es (WebGL2) shaders with a small version header macro.
- **Two-Pass Retro Renderer:**
  1. Depth-only shadow map (`512×512`) from primary directional/spot light.
  2. Low-resolution offscreen FBO (`320×240` / `480×270`) rendering greedy-meshed voxels, low-poly 3D meshes, and lit billboard sprites with up to 12 dynamic point lights + shadow sampling + fog.
  3. Fullscreen nearest-neighbor upscale with subtle Bayer ordered dithering + crisp HUD / Split-Screen viewports / Touch Overlay.
- **WebRTC & WebXR Bridges (`web/`):** Thin JavaScript modules (`webrtc_bridge.js`, `webxr_bridge.js`) exchanging binary packets and XR eye matrices/controller states directly with exported C functions (`swieta_net_on_packet`, `swieta_xr_render_eye`).
- **Procedural CC BY 4.0 Assets (`tools/gen_assets.py`):** Deterministically generates retro pixel texture atlases, 8-way sprites, and FM/chiptune audio assets into `assets/` with clean drop-in overrides.
