// JSON-line TCP client for the silencer binary's control socket.
// Each request gets a monotonically increasing id; replies arrive on the same
// connection tagged with the request id. We multiplex pending requests by id.

import { Socket } from 'node:net';

interface PendingReply {
  resolve: (r: unknown) => void;
  reject: (e: Error) => void;
}

export class ControlClient {
  private sock: Socket | null = null;
  private buf = '';
  private nextId = 1;
  private pending = new Map<number, PendingReply>();

  connect(host: string, port: number): Promise<void> {
    return new Promise((resolve, reject) => {
      const s = new Socket();
      s.setNoDelay(true);
      s.once('error', reject);
      s.connect(port, host, () => {
        s.removeListener('error', reject);
        this.sock = s;
        s.on('data', (chunk: Buffer) => this.onData(chunk));
        s.on('close', () => this.onClose());
        s.on('error', () => this.onClose());
        resolve();
      });
    });
  }

  private onData(chunk: Buffer): void {
    this.buf += chunk.toString('utf8');
    let nl: number;
    while ((nl = this.buf.indexOf('\n')) >= 0) {
      const line = this.buf.slice(0, nl).trim();
      this.buf = this.buf.slice(nl + 1);
      if (!line) continue;
      let msg: { id?: number; ok?: boolean; result?: unknown; error?: string };
      try {
        msg = JSON.parse(line);
      } catch {
        continue;
      }
      if (typeof msg.id !== 'number') continue;
      const pending = this.pending.get(msg.id);
      if (!pending) continue;
      this.pending.delete(msg.id);
      if (msg.ok) pending.resolve(msg.result);
      else pending.reject(new Error(msg.error ?? 'control error'));
    }
  }

  private onClose(): void {
    for (const p of this.pending.values()) {
      p.reject(new Error('control socket closed'));
    }
    this.pending.clear();
    this.sock = null;
  }

  send(op: string, args: Record<string, unknown> = {}): Promise<unknown> {
    if (!this.sock) return Promise.reject(new Error('not connected'));
    const id = this.nextId++;
    // The server's parser wants args nested under "args".
    const line = JSON.stringify({ id, op, args }) + '\n';
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.sock!.write(line);
    });
  }

  // Fire-and-forget — engine sees `noreply: true` and skips writing back. No
  // entry in this.pending; no allocation per call beyond the JSON line itself.
  // Use for edge events (menu key presses) that don't need an ack.
  sendNoReply(op: string, args: Record<string, unknown> = {}): void {
    if (!this.sock) return;
    const line = JSON.stringify({ id: 0, op, args, noreply: true }) + '\n';
    this.sock.write(line);
  }

  close(): void {
    this.sock?.end();
    this.sock = null;
  }
}
