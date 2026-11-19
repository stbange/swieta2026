/* SWIETA 2026 — WebRTC DataChannel P2P Bridge (MIT License)
 * Supports both lightweight WebSocket signaling relay (6-char room codes)
 * and zero-server manual copy/paste SDP invite tokens for static GitHub Pages.
 */
(function () {
  const ICE_CONFIG = {
    iceServers: [
      { urls: "stun:stun.l.google.com:19302" },
      { urls: "stun:stun1.l.google.com:19302" }
    ]
  };

  let mod = null;
  let pc = null;
  let dcUnreliable = null;
  let dcReliable = null;
  let ws = null;
  let role = 0; // 0=offline, 1=host, 2=client

  const setStatus = (msg) => {
    const el = document.getElementById("mpStatus");
    if (el) el.textContent = "Status: " + msg;
    console.log("[SwietaNet]", msg);
  };

  function forwardToWasm(arrayBuf) {
    if (!mod || !mod._swieta_net_on_packet) return;
    const bytes = new Uint8Array(arrayBuf);
    const ptr = mod._malloc(bytes.length);
    mod.HEAPU8.set(bytes, ptr);
    mod._swieta_net_on_packet(ptr, bytes.length);
    mod._free(ptr);
  }

  function wireChannel(dc, label) {
    dc.binaryType = "arraybuffer";
    dc.onopen = () => {
      setStatus("Connected (" + (role === 1 ? "HOST" : "CLIENT") + " · " + label + ")");
      if (mod && mod._swieta_net_set_role) {
        mod._swieta_net_set_role(role, role === 1 ? 0 : 1);
      }
    };
    dc.onmessage = (ev) => {
      if (ev.data instanceof ArrayBuffer) {
        forwardToWasm(ev.data);
      }
    };
    dc.onclose = () => setStatus("Peer disconnected");
  }

  function createPeer(isHost) {
    if (pc) pc.close();
    role = isHost ? 1 : 2;
    pc = new RTCPeerConnection(ICE_CONFIG);
    if (isHost) {
      dcUnreliable = pc.createDataChannel("game-unreliable", { ordered: false, maxRetransmits: 0 });
      dcReliable = pc.createDataChannel("game-reliable", { ordered: true });
      wireChannel(dcUnreliable, "UDP-like");
      wireChannel(dcReliable, "reliable");
    } else {
      pc.ondatachannel = (ev) => {
        if (ev.channel.label === "game-reliable") {
          dcReliable = ev.channel;
        } else {
          dcUnreliable = ev.channel;
        }
        wireChannel(ev.channel, ev.channel.label);
      };
    }
    return pc;
  }

  function waitIceComplete(peer) {
    return new Promise((resolve) => {
      if (peer.iceGatheringState === "complete") return resolve();
      const check = () => {
        if (peer.iceGatheringState === "complete") {
          peer.removeEventListener("icegatheringstatechange", check);
          resolve();
        }
      };
      peer.addEventListener("icegatheringstatechange", check);
      setTimeout(resolve, 2500);
    });
  }

  function genCode() {
    const a = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    let s = "";
    for (let i = 0; i < 6; ++i) s += a[Math.floor(Math.random() * a.length)];
    return s;
  }

  function connectRelay(relayUrl, roomCode, isHost) {
    if (!relayUrl) {
      setStatus("Enter a signaling relay URL or use Manual Copy/Paste SDP below.");
      return;
    }
    if (ws) ws.close();
    const peer = createPeer(isHost);
    ws = new WebSocket(relayUrl);
    ws.onopen = async () => {
      setStatus("Relay connected · Room " + roomCode);
      ws.send(JSON.stringify({ type: isHost ? "host" : "join", room: roomCode }));
    };
    ws.onmessage = async (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.type === "peer-joined" && isHost) {
        const offer = await peer.createOffer();
        await peer.setLocalDescription(offer);
        await waitIceComplete(peer);
        ws.send(JSON.stringify({ type: "signal", room: roomCode, sdp: peer.localDescription }));
      } else if (msg.type === "signal" && msg.sdp) {
        await peer.setRemoteDescription(msg.sdp);
        if (msg.sdp.type === "offer") {
          const ans = await peer.createAnswer();
          await peer.setLocalDescription(ans);
          await waitIceComplete(peer);
          ws.send(JSON.stringify({ type: "signal", room: roomCode, sdp: peer.localDescription }));
        }
      }
    };
    ws.onerror = () => setStatus("Relay unreachable — use Manual SDP fallback below.");
  }

  window.SwietaNet = {
    init: (moduleObj) => {
      mod = moduleObj;
      const outBox = document.getElementById("txtSdpOut");
      const inBox = document.getElementById("txtSdpIn");
      const relayInp = document.getElementById("inpRelay");
      const roomInp = document.getElementById("inpRoom");

      document.getElementById("btnHost").onclick = () => {
        const code = roomInp.value.trim().toUpperCase() || genCode();
        roomInp.value = code;
        connectRelay(relayInp.value.trim(), code, true);
      };
      document.getElementById("btnJoin").onclick = () => {
        const code = roomInp.value.trim().toUpperCase();
        if (code.length !== 6) return setStatus("Enter a 6-character room code.");
        connectRelay(relayInp.value.trim(), code, false);
      };
      document.getElementById("btnManualOffer").onclick = async () => {
        const peer = createPeer(true);
        const offer = await peer.createOffer();
        await peer.setLocalDescription(offer);
        await waitIceComplete(peer);
        outBox.value = btoa(JSON.stringify(peer.localDescription));
        setStatus("Offer created! Send token to friend, then paste their Answer.");
      };
      document.getElementById("btnManualAcceptOffer").onclick = async () => {
        try {
          const offer = JSON.parse(atob(inBox.value.trim()));
          const peer = createPeer(false);
          await peer.setRemoteDescription(offer);
          const ans = await peer.createAnswer();
          await peer.setLocalDescription(ans);
          await waitIceComplete(peer);
          outBox.value = btoa(JSON.stringify(peer.localDescription));
          setStatus("Answer created! Send back to Host.");
        } catch (e) {
          setStatus("Invalid Offer token.");
        }
      };
      document.getElementById("btnManualApplyAnswer").onclick = async () => {
        try {
          const ans = JSON.parse(atob(inBox.value.trim()));
          await pc.setRemoteDescription(ans);
          setStatus("Answer applied — establishing direct P2P DataChannel...");
        } catch (e) {
          setStatus("Invalid Answer token.");
        }
      };
    },
    sendPacket: (uint8Bytes, reliable) => {
      const ch = reliable ? dcReliable : (dcUnreliable || dcReliable);
      if (ch && ch.readyState === "open") {
        ch.send(uint8Bytes);
      }
    }
  };
})();
