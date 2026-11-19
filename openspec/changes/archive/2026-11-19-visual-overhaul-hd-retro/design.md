# Design: High-Detail Normal-Mapped Retro-3D Renderer

## Shader & Lighting Pipeline (OpenGL 3.3 Core & WebGL2 / ES 3.0)
1. **Sky Pass (`prog_sky`):** Fullscreen ray-direction sky shader computing world-space view ray from inverse `proj * mat3(view)` to render per-scene atmospheric gradients, sun/moon discs, layered scrolling storm/desert clouds, starry night skies, and distant horizon mountain/city silhouettes with depth-write disabled.
2. **Shadow Pass (`prog_shadow`):** `1024×1024` player-centered directional shadow map sampled with 3×3 Percentage-Closer Filtering (PCF) for smooth, non-blocky shadow edges.
3. **World Pass (`prog_world`):**
   - `GL_TEXTURE0`: `1024×192` (`64×64` × `16×3`) multi-octave procedural diffuse texture atlas (or `1024×128` animated `64×64` sprite atlas for billboards).
   - `GL_TEXTURE1`: `1024×1024` shadow depth map.
   - `GL_TEXTURE2`: `1024×192` tangent-space normal (RGB) + specular gloss (A) map synthesized via Sobel gradient over each material's heightfield.
   - Tangent space derived analytically from voxel face normals (zero extra VBO attributes) for crisp bump lighting, Blinn-Phong specular highlights, hemisphere sky/ground bounce, subtle rim light, dynamic point lights, and ACES-like filmic tone mapping.
4. **Pre-Baked 3D Mesh Library (`MeshGpu`):** Static GPU VAOs/VBOs generated once at init for Cylinder (12-seg), Sphere (lat/lon faceted), Cone (8-seg), and Box, eliminating per-draw `glBufferData` stalls and enabling richly sculpted barrels, boulders, tree ornaments, articulated 3D bosses, and detailed first-person weapon viewmodels.
5. **High-Detail Animated Sprite Atlas (`1024×128`):** `16` distinct `64×64` archetypes × `2` walk/flap/idle frames, using anatomical ellipse/limb primitives, dark 1px outlines, directional top-left lighting, glowing eyes/visors, and held weapons.
6. **Crisp Retro-HD Framebuffer:** Default internal framebuffer `640×360` (portrait `360×640` on phones), selectable across `320×240`, `480×270`, `640×360`, and `960×540`.
