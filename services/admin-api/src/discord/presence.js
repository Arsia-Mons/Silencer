// Minimal Discord gateway client: just enough to hold a connection and keep
// the bot's presence ("Watching N players · M games") current. No discord.js —
// presence is a gateway-only op, so a raw WebSocket is the entire requirement
// and adds no dependency (Bun ships a native WebSocket).
//
// This lives inside admin-api because admin-api already consumes silencer.events
// and computes the live counts (ws/index.js getLiveState). The presence updater
// just formats those counts and pushes them to Discord on change — no extra
// process, queue, or poll.

const GATEWAY_URL = 'wss://gateway.discord.gg/?v=10&encoding=json';
const ACTIVITY_WATCHING = 3;
const PRESENCE_MIN_INTERVAL_MS = 5000; // Discord limits presence updates to ~5 / 20s.
const MAX_BACKOFF_MS = 30000;

// Gateway opcodes (https://discord.com/developers/docs/topics/gateway).
const OP_DISPATCH = 0;
const OP_HEARTBEAT = 1;
const OP_IDENTIFY = 2;
const OP_PRESENCE_UPDATE = 3;
const OP_RECONNECT = 7;
const OP_INVALID_SESSION = 9;
const OP_HELLO = 10;
const OP_HEARTBEAT_ACK = 11;

export function formatPresence(players, games) {
  const p = `${players} player${players === 1 ? '' : 's'}`;
  const g = `${games} game${games === 1 ? '' : 's'}`;
  return `${p} · ${g}`;
}

export class DiscordPresence {
  constructor(token) {
    this.token = token || '';
    this.ws = null;
    this.heartbeatTimer = null;
    this.heartbeatStart = null;
    this.lastSeq = null;
    this.acked = true;
    this.ready = false;
    this.stopped = false;
    this.backoffMs = 1000;

    this.desiredName = null;
    this.sentName = null;
    this.lastSentAt = 0;
    this.flushTimer = null;
  }

  start() {
    if (!this.token) {
      console.warn(
        '[discord] no DISCORD_TOKEN — presence disabled (counts logged on change, not sent)',
      );
      return;
    }
    this.connect();
  }

  // Update presence from live counts. Coalesced + throttled before the gateway.
  update(players, games) {
    this.setActivity(formatPresence(players, games));
  }

  setActivity(name) {
    this.desiredName = name;
    if (!this.token) {
      if (name !== this.sentName) {
        console.log(`[discord] (disabled) presence → Watching ${name}`);
        this.sentName = name;
      }
      return;
    }
    this.scheduleFlush();
  }

  stop() {
    this.stopped = true;
    this.clearTimers();
    if (this.ws) this.ws.close(1000, 'shutdown');
    this.ws = null;
  }

  connect() {
    // Clear any stale timers up front: a fast RECONNECT/INVALID_SESSION can
    // schedule connect() before onClose() fires, and a leftover heartbeat
    // timer would otherwise beat on the fresh socket before HELLO arrives.
    this.clearTimers();
    this.ready = false;
    this.acked = true;
    const ws = new WebSocket(GATEWAY_URL);
    this.ws = ws;
    ws.addEventListener('message', (ev) => this.onMessage(String(ev.data)));
    ws.addEventListener('close', (ev) => this.onClose(ev.code));
    ws.addEventListener('error', () => console.error('[discord] websocket error'));
  }

  onMessage(raw) {
    let msg;
    try {
      msg = JSON.parse(raw);
    } catch {
      return;
    }
    if (msg.s !== null && msg.s !== undefined) this.lastSeq = msg.s;

    switch (msg.op) {
      case OP_HELLO:
        this.startHeartbeat(msg.d.heartbeat_interval);
        this.identify();
        break;
      case OP_HEARTBEAT:
        this.sendHeartbeat();
        break;
      case OP_HEARTBEAT_ACK:
        this.acked = true;
        break;
      case OP_DISPATCH:
        if (msg.t === 'READY') {
          this.ready = true;
          this.backoffMs = 1000;
          console.log('[discord] gateway ready');
          this.flush(true);
        }
        break;
      case OP_RECONNECT:
      case OP_INVALID_SESSION:
        console.warn('[discord] gateway requested reconnect');
        if (this.ws) this.ws.close(4000, 'reconnect');
        break;
    }
  }

  startHeartbeat(intervalMs) {
    this.clearHeartbeat();
    // Jitter the first beat per the gateway spec, then beat on a fixed interval.
    this.heartbeatStart = setTimeout(() => {
      this.sendHeartbeat();
      this.heartbeatTimer = setInterval(() => {
        if (!this.acked) {
          // No ACK since the last beat → zombie connection. Drop and reconnect.
          console.warn('[discord] heartbeat not acked — reconnecting');
          if (this.ws) this.ws.close(4000, 'zombie');
          return;
        }
        this.sendHeartbeat();
      }, intervalMs);
    }, intervalMs * Math.random());
  }

  sendHeartbeat() {
    this.acked = false;
    this.send({ op: OP_HEARTBEAT, d: this.lastSeq });
  }

  identify() {
    this.send({
      op: OP_IDENTIFY,
      d: {
        token: this.token,
        intents: 0, // We subscribe to no events; we only push presence.
        properties: { os: 'linux', browser: 'silencer-admin-api', device: 'silencer-admin-api' },
        presence: this.presencePayload(this.desiredName),
      },
    });
  }

  scheduleFlush() {
    if (this.flushTimer) return;
    const wait = Math.max(0, PRESENCE_MIN_INTERVAL_MS - (Date.now() - this.lastSentAt));
    this.flushTimer = setTimeout(() => {
      this.flushTimer = null;
      this.flush(false);
    }, wait);
  }

  flush(force) {
    if (!this.ready || !this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    if (this.desiredName === null) return;
    if (!force && this.desiredName === this.sentName) return;
    if (!force && Date.now() - this.lastSentAt < PRESENCE_MIN_INTERVAL_MS) {
      this.scheduleFlush();
      return;
    }
    this.send({ op: OP_PRESENCE_UPDATE, d: this.presencePayload(this.desiredName) });
    this.sentName = this.desiredName;
    this.lastSentAt = Date.now();
  }

  presencePayload(name) {
    return {
      since: null,
      activities: name ? [{ name, type: ACTIVITY_WATCHING }] : [],
      status: 'online',
      afk: false,
    };
  }

  send(payload) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(payload));
    }
  }

  onClose(code) {
    this.clearTimers();
    this.ws = null;
    this.ready = false;
    if (this.stopped) return;
    console.warn(`[discord] disconnected (code ${code}) — reconnecting in ${this.backoffMs}ms`);
    setTimeout(() => this.connect(), this.backoffMs);
    this.backoffMs = Math.min(this.backoffMs * 2, MAX_BACKOFF_MS);
  }

  clearHeartbeat() {
    if (this.heartbeatStart) {
      clearTimeout(this.heartbeatStart);
      this.heartbeatStart = null;
    }
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer);
      this.heartbeatTimer = null;
    }
  }

  clearTimers() {
    this.clearHeartbeat();
    if (this.flushTimer) {
      clearTimeout(this.flushTimer);
      this.flushTimer = null;
    }
  }
}
