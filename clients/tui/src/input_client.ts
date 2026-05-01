// Binary input channel: TS host → engine. One-way, latest-wins, no replies.
//
// Wire format mirrors clients/silencer/src/net/inputserver.{h,cpp}. Three
// message types, all layered on the engine side:
//
//   type 0x01 INPUT_SNAPSHOT (action-level)
//     12-byte payload — for programmatic / CLI / agent control. Bypasses
//     the keymap; addresses Input fields directly.
//
//     payload[0..3]   u32 LE keymask     (bit positions in KEY_BIT below)
//     payload[4]      u8  weapon         (bits 0..3 = weapon[0..3], bit 4 = mousedown)
//     payload[5..6]   u16 LE mousex      (0xFFFF sentinel = "no mouse position")
//     payload[7..8]   u16 LE mousey
//     payload[9..11]  u8  reserved
//
//   type 0x02 SCANCODE_SNAPSHOT (scancode-level)
//     64-byte bitmask — for the TUI client. Engine writes into Game::keystate
//     and runs the same UpdateInputState pipeline as native SDL, so the
//     user's keymap profile is honored automatically.
//
//   type 0x03 MOUSE_SNAPSHOT
//     5-byte payload: [u16 LE x][u16 LE y][u8 buttons]; buttons bit 0 = left.
//     Engine pixel coordinates. Sent independently of scancodes/actions so
//     mouse motion doesn't trample held-key state.
//
// Handshake: client sends 1 byte (protocol version 0x01) before the first
// message. Any subset of message types may be sent on the same connection.

import { Socket } from 'node:net';
import { SCANCODE_BYTES } from './input';

const PROTO_VERSION = 0x01;
const MSG_ACTION   = 0x01;
const MSG_SCANCODE = 0x02;
const MSG_MOUSE    = 0x03;
const ACTION_PAYLOAD_BYTES = 12;
const MOUSE_PAYLOAD_BYTES  = 5;

// Bit positions MUST match clients/silencer/src/net/inputserver.cpp.
const KEY_BIT: Record<string, number> = {
  keymoveup: 0,
  keymovedown: 1,
  keymoveleft: 2,
  keymoveright: 3,
  keylookupleft: 4,
  keylookupright: 5,
  keylookdownleft: 6,
  keylookdownright: 7,
  keynextinv: 8,
  keynextcam: 9,
  keyprevcam: 10,
  keydetonate: 11,
  keyjump: 12,
  keyjetpack: 13,
  keyactivate: 14,
  keyuse: 15,
  keyfire: 16,
  keydisguise: 17,
  keynextweapon: 18,
  keyup: 19,
  keydown: 20,
  keyleft: 21,
  keyright: 22,
  keychat: 23,
};

/** Action-level wire payload. The TUI doesn't construct this — it's exposed
 *  for programmatic / agent clients that want to drive Input fields directly. */
export interface ActionInput {
  keymoveup: boolean;
  keymovedown: boolean;
  keymoveleft: boolean;
  keymoveright: boolean;
  keylookupleft: boolean;
  keylookupright: boolean;
  keylookdownleft: boolean;
  keylookdownright: boolean;
  keynextinv: boolean;
  keynextcam: boolean;
  keyprevcam: boolean;
  keydetonate: boolean;
  keyjump: boolean;
  keyjetpack: boolean;
  keyactivate: boolean;
  keyuse: boolean;
  keyfire: boolean;
  keydisguise: boolean;
  keyweapon: [boolean, boolean, boolean, boolean];
  keynextweapon: boolean;
  keyup: boolean;
  keydown: boolean;
  keyleft: boolean;
  keyright: boolean;
  keychat: boolean;
  mousex: number;
  mousey: number;
  mousedown: boolean;
}

export class InputClient {
  private sock: Socket | null = null;

  connect(host: string, port: number): Promise<void> {
    return new Promise((resolve, reject) => {
      const s = new Socket();
      s.setNoDelay(true);
      s.once('error', reject);
      s.connect(port, host, () => {
        s.removeListener('error', reject);
        s.on('error', () => this.close());
        s.on('close', () => {
          this.sock = null;
        });
        // Drop any data the engine sends — channel is one-way client→engine.
        s.on('data', () => {});
        // Handshake: protocol version byte. If the engine doesn't recognise
        // it, it'll close the connection and the next write will fail.
        s.write(Uint8Array.of(PROTO_VERSION));
        this.sock = s;
        resolve();
      });
    });
  }

  /** Send an action-level snapshot. Cheap; safe to call every tick. */
  sendAction(state: ActionInput): void {
    if (!this.sock) return;
    const buf = new Uint8Array(3 + ACTION_PAYLOAD_BYTES);
    buf[0] = MSG_ACTION;
    buf[1] = ACTION_PAYLOAD_BYTES & 0xff;
    buf[2] = (ACTION_PAYLOAD_BYTES >> 8) & 0xff;

    let mask = 0;
    for (const [name, bit] of Object.entries(KEY_BIT)) {
      if ((state as unknown as Record<string, boolean>)[name]) mask |= 1 << bit;
    }
    buf[3] = mask & 0xff;
    buf[4] = (mask >>> 8) & 0xff;
    buf[5] = (mask >>> 16) & 0xff;
    buf[6] = (mask >>> 24) & 0xff;

    let weapon = 0;
    if (state.keyweapon[0]) weapon |= 0x01;
    if (state.keyweapon[1]) weapon |= 0x02;
    if (state.keyweapon[2]) weapon |= 0x04;
    if (state.keyweapon[3]) weapon |= 0x08;
    if (state.mousedown) weapon |= 0x10;
    buf[7] = weapon;

    buf[8] = state.mousex & 0xff;
    buf[9] = (state.mousex >>> 8) & 0xff;
    buf[10] = state.mousey & 0xff;
    buf[11] = (state.mousey >>> 8) & 0xff;

    this.sock.write(buf);
  }

  /** Send a scancode held-set bitmask (64 bytes). */
  sendScancodes(bitmask: Uint8Array): void {
    if (!this.sock) return;
    if (bitmask.length !== SCANCODE_BYTES) {
      throw new Error(`scancode bitmask must be ${SCANCODE_BYTES} bytes`);
    }
    const buf = new Uint8Array(3 + SCANCODE_BYTES);
    buf[0] = MSG_SCANCODE;
    buf[1] = SCANCODE_BYTES & 0xff;
    buf[2] = (SCANCODE_BYTES >> 8) & 0xff;
    buf.set(bitmask, 3);
    this.sock.write(buf);
  }

  /** Send a mouse position + button-state snapshot (engine pixel coords). */
  sendMouse(x: number, y: number, leftDown: boolean): void {
    if (!this.sock) return;
    const buf = new Uint8Array(3 + MOUSE_PAYLOAD_BYTES);
    buf[0] = MSG_MOUSE;
    buf[1] = MOUSE_PAYLOAD_BYTES & 0xff;
    buf[2] = (MOUSE_PAYLOAD_BYTES >> 8) & 0xff;
    buf[3] = x & 0xff;
    buf[4] = (x >>> 8) & 0xff;
    buf[5] = y & 0xff;
    buf[6] = (y >>> 8) & 0xff;
    buf[7] = leftDown ? 0x01 : 0x00;
    this.sock.write(buf);
  }

  close(): void {
    this.sock?.end();
    this.sock = null;
  }
}
