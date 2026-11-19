/* SWIETA 2026 — WebXR Immersive VR Stereo Bridge (MIT License) */
(function () {
  let canvas = null;
  let mod = null;
  let session = null;
  let refSpace = null;
  let lastTime = 0;

  async function enterVR() {
    if (!navigator.xr) {
      alert("WebXR is not supported in this browser. Try Meta Quest Browser or a WebXR-capable desktop browser.");
      return;
    }
    const supported = await navigator.xr.isSessionSupported("immersive-vr");
    if (!supported) {
      alert("No immersive-vr headset detected. Continuing in flat retro 3D mode.");
      return;
    }
    const gl = canvas.getContext("webgl2");
    if (!gl) return;
    await gl.makeXRCompatible();

    session = await navigator.xr.requestSession("immersive-vr", {
      optionalFeatures: ["local-floor"]
    });
    window.SwietaXR.active = true;
    session.addEventListener("end", () => {
      window.SwietaXR.active = false;
      session = null;
      document.getElementById("btnXR").textContent = "Enter VR";
    });

    const layer = new XRWebGLLayer(session, gl);
    session.updateRenderState({ baseLayer: layer });
    refSpace = await session.requestReferenceSpace("local");
    document.getElementById("btnXR").textContent = "Exit VR";
    lastTime = performance.now();

    const projPtr = mod._malloc(16 * 4);
    const viewPtr = mod._malloc(16 * 4);

    const onFrame = (t, frame) => {
      if (!session) {
        mod._free(projPtr);
        mod._free(viewPtr);
        return;
      }
      session.requestAnimationFrame(onFrame);
      const dt = Math.min((t - lastTime) * 0.001, 0.05);
      lastTime = t;

      if (mod._swieta_xr_begin_frame) {
        mod._swieta_xr_begin_frame(dt);
      }

      const pose = frame.getViewerPose(refSpace);
      if (!pose) return;
      const baseLayer = session.renderState.baseLayer;
      gl.bindFramebuffer(gl.FRAMEBUFFER, baseLayer.framebuffer);

      for (const view of pose.views) {
        const vp = baseLayer.getViewport(view);
        mod.HEAPF32.set(view.projectionMatrix, projPtr >> 2);
        mod.HEAPF32.set(view.transform.inverse.matrix, viewPtr >> 2);
        if (mod._swieta_xr_render_eye) {
          mod._swieta_xr_render_eye(projPtr, viewPtr, vp.x, vp.y, vp.width, vp.height);
        }
      }
    };
    session.requestAnimationFrame(onFrame);
  }

  window.SwietaXR = {
    active: false,
    init: (canvasEl, moduleObj) => {
      canvas = canvasEl;
      mod = moduleObj;
    },
    toggleSession: async () => {
      if (session) {
        await session.end();
      } else {
        await enterVR();
      }
    }
  };
})();
