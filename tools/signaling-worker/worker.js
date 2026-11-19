/* SWIETA 2026 — Ultra-Lightweight WebRTC Signaling Relay (MIT License)
 * Handles ONLY the 1-second SDP/ICE room handshake; zero game packets touch this worker.
 * Deployable free on Cloudflare Workers (Durable Object or in-memory dev room map).
 */
const rooms = new Map();

export default {
  async fetch(request) {
    if (request.headers.get("Upgrade") !== "websocket") {
      return new Response("SWIETA 2026 WebRTC Signaling Relay OK\n", { status: 200 });
    }
    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    server.accept();

    let joinedRoom = null;
    server.addEventListener("message", (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        const code = String(msg.room || "").toUpperCase().slice(0, 6);
        if (!code) return;
        if (msg.type === "host") {
          joinedRoom = code;
          rooms.set(code, { host: server, peers: [] });
        } else if (msg.type === "join") {
          const r = rooms.get(code);
          if (r && r.host) {
            joinedRoom = code;
            r.peers.push(server);
            r.host.send(JSON.stringify({ type: "peer-joined", room: code }));
          }
        } else if (msg.type === "signal") {
          const r = rooms.get(code);
          if (!r) return;
          if (server === r.host) {
            r.peers.forEach((p) => p.send(JSON.stringify(msg)));
          } else if (r.host) {
            r.host.send(JSON.stringify(msg));
          }
        }
      } catch (_) {}
    });

    server.addEventListener("close", () => {
      if (joinedRoom && rooms.has(joinedRoom)) {
        const r = rooms.get(joinedRoom);
        if (r.host === server) rooms.delete(joinedRoom);
      }
    });

    return new Response(null, { status: 101, webSocket: client });
  }
};
