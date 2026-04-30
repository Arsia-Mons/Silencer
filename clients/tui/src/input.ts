// Terminal stdin → Silencer Input struct.
//
// We don't get keyup events from a normal terminal, so each keypress sets the
// corresponding Input field true and arms an autorelease timer. If no repeat
// arrives within HOLD_MS the field flips back to false. This gives "hold"
// semantics that closely match the C++ side even though we never see release.
// Modern terminals' autorepeat fires fast enough (~30-40 ms) that gameplay
// keys feel held; HOLD_MS of 120 ms tolerates one missed repeat.

const HOLD_MS = 120;

// Input fields that have boolean state on the C++ side.
type BoolField =
  | 'keymoveup'
  | 'keymovedown'
  | 'keymoveleft'
  | 'keymoveright'
  | 'keylookupleft'
  | 'keylookupright'
  | 'keylookdownleft'
  | 'keylookdownright'
  | 'keynextinv'
  | 'keynextcam'
  | 'keyprevcam'
  | 'keydetonate'
  | 'keyjump'
  | 'keyjetpack'
  | 'keyactivate'
  | 'keyuse'
  | 'keyfire'
  | 'keydisguise'
  | 'keynextweapon'
  | 'keyup'
  | 'keydown'
  | 'keyleft'
  | 'keyright'
  | 'keychat';

// Default keybinds. These mirror the ergonomic shape of the SDL client's
// default profile but use terminal escape sequences as keys.
//
// Movement WASD; jump/jetpack/fire/use/activate on familiar keys; arrow keys
// drive interface navigation (keyup/keydown/keyleft/keyright/keyactivate-Enter).
const KEYMAP: Record<string, BoolField | BoolField[]> = {
  // Movement.
  w: 'keymoveup',
  s: 'keymovedown',
  a: 'keymoveleft',
  d: 'keymoveright',
  // Aim diagonals (8-direction discrete aim).
  q: 'keylookupleft',
  e: 'keylookupright',
  z: 'keylookdownleft',
  c: 'keylookdownright',
  // Action keys.
  ' ': 'keyjump',
  f: 'keyfire',
  r: 'keyactivate',
  g: 'keyuse',
  x: 'keydetonate',
  v: 'keydisguise',
  '\t': 'keynextweapon',
  // Weapon group is 4 mutually-exclusive bits — handled separately below.
  // Interface navigation (arrows).
  '\x1b[A': 'keyup',
  '\x1b[B': 'keydown',
  '\x1b[D': 'keyleft',
  '\x1b[C': 'keyright',
  '\r': 'keyactivate',
  '\n': 'keyactivate',
};

const WEAPON_KEYS: Record<string, number> = {
  '1': 0,
  '2': 1,
  '3': 2,
  '4': 3,
};

export interface InputState {
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

function emptyInput(): InputState {
  return {
    keymoveup: false,
    keymovedown: false,
    keymoveleft: false,
    keymoveright: false,
    keylookupleft: false,
    keylookupright: false,
    keylookdownleft: false,
    keylookdownright: false,
    keynextinv: false,
    keynextcam: false,
    keyprevcam: false,
    keydetonate: false,
    keyjump: false,
    keyjetpack: false,
    keyactivate: false,
    keyuse: false,
    keyfire: false,
    keydisguise: false,
    keyweapon: [false, false, false, false],
    keynextweapon: false,
    keyup: false,
    keydown: false,
    keyleft: false,
    keyright: false,
    keychat: false,
    // 0xFFFF sentinel = "no mouse position", per input.cpp:Serialize.
    mousex: 0xffff,
    mousey: 0xffff,
    mousedown: false,
  };
}

export class TerminalInput {
  private state: InputState = emptyInput();
  private lastSeen = new Map<BoolField, number>();
  private weaponLastSeen = new Map<number, number>();
  /** User pressed Ctrl-C / Ctrl-Q — host should exit. */
  quitRequested = false;
  /** User pressed Esc — host should send a "back" control op. */
  backRequested = false;

  feed(chunk: Buffer): void {
    const s = chunk.toString('utf8');
    let i = 0;
    while (i < s.length) {
      const c = s[i]!;
      // Ctrl-C / Ctrl-Q → quit.
      if (c === '\x03' || c === '\x11') {
        this.quitRequested = true;
        i++;
        continue;
      }
      // Esc — could be a lone Esc or the start of a CSI sequence.
      if (c === '\x1b') {
        if (i + 2 < s.length && s[i + 1] === '[') {
          const seq = s.slice(i, i + 3);
          this.press(seq);
          i += 3;
          continue;
        }
        // Lone Esc.
        this.backRequested = true;
        i++;
        continue;
      }
      this.press(c);
      i++;
    }
  }

  private press(key: string): void {
    const now = performance.now();
    const mapped = KEYMAP[key];
    if (mapped !== undefined) {
      const fields = Array.isArray(mapped) ? mapped : [mapped];
      for (const f of fields) {
        (this.state as Record<BoolField, boolean>)[f] = true;
        this.lastSeen.set(f, now);
      }
      return;
    }
    if (key in WEAPON_KEYS) {
      const idx = WEAPON_KEYS[key]!;
      this.state.keyweapon[idx] = true;
      this.weaponLastSeen.set(idx, now);
    }
  }

  /** Call once per tick; expires keys whose last press is older than HOLD_MS. */
  decay(): void {
    const now = performance.now();
    for (const [f, t] of this.lastSeen) {
      if (now - t > HOLD_MS) {
        (this.state as Record<BoolField, boolean>)[f] = false;
        this.lastSeen.delete(f);
      }
    }
    for (const [i, t] of this.weaponLastSeen) {
      if (now - t > HOLD_MS) {
        this.state.keyweapon[i as 0 | 1 | 2 | 3] = false;
        this.weaponLastSeen.delete(i);
      }
    }
  }

  snapshot(): InputState {
    return {
      ...this.state,
      keyweapon: [...this.state.keyweapon] as InputState['keyweapon'],
    };
  }
}
