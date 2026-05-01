// Terminal stdin → held SDL scancodes + edge-triggered menu KeyEvents.
//
// Topology:
//   The TUI is a dumb keyboard proxy. We translate terminal byte sequences
//   into SDL scancodes and ship the held-set over the binary input channel.
//   The engine consults the user's keymap profile (shared/assets/keybinds/
//   default.json or a custom profile) to derive Action-level Input fields,
//   exactly as native SDL does. Customizing the profile customizes the TUI
//   automatically — no duplicate keymap here.
//
// Two parsing modes:
//   - kitty: when the terminal supports kitty keyboard protocol mode 11,
//     every key arrives as `CSI <code>;<mods>:<event> <suffix>`. Press and
//     release are explicit, so the held-set is exact.
//   - legacy: cooked terminals don't deliver key-up events. Each press
//     refreshes an autorelease timer (HOLD_MS); without a refresh the
//     scancode flips off. Mode 11's release events make this unnecessary.

const HOLD_MS = 120;

// SDL_SCANCODE_COUNT = 512. Wire format = 64-byte bitmask.
export const SDL_SCANCODE_COUNT = 512;
export const SCANCODE_BYTES = SDL_SCANCODE_COUNT >> 3;

// SDL3 scancode values used in this file. Keep this list focused — anything
// the keymap profile doesn't reference doesn't need a translation entry.
export const Scancode = {
  A: 4, B: 5, C: 6, D: 7, E: 8, F: 9, G: 10, H: 11, I: 12, J: 13,
  K: 14, L: 15, M: 16, N: 17, O: 18, P: 19, Q: 20, R: 21, S: 22, T: 23,
  U: 24, V: 25, W: 26, X: 27, Y: 28, Z: 29,
  N1: 30, N2: 31, N3: 32, N4: 33, N5: 34,
  N6: 35, N7: 36, N8: 37, N9: 38, N0: 39,
  RETURN: 40, ESCAPE: 41, BACKSPACE: 42, TAB: 43, SPACE: 44,
  MINUS: 45, EQUALS: 46, LEFTBRACKET: 47, RIGHTBRACKET: 48, BACKSLASH: 49,
  SEMICOLON: 51, APOSTROPHE: 52, GRAVE: 53,
  COMMA: 54, PERIOD: 55, SLASH: 56,
  RIGHT: 79, LEFT: 80, DOWN: 81, UP: 82,
  LCTRL: 224, LSHIFT: 225, LALT: 226,
  RCTRL: 228, RSHIFT: 229, RALT: 230,
} as const;

// Translate a printable ASCII / control byte to its SDL scancode (US QWERTY).
// Returns -1 for keys we don't bind. Letters are layout-independent on the
// scancode side, so we lower-case before lookup.
function asciiToScancode(c: string): number {
  if (c.length !== 1) return -1;
  const code = c.charCodeAt(0);
  // Letters a-z (97-122) → SDL A-Z (4-29). Capital letters too (65-90).
  if (code >= 97 && code <= 122) return Scancode.A + (code - 97);
  if (code >= 65 && code <= 90)  return Scancode.A + (code - 65);
  // Digits 1-9 → 30-38, 0 → 39.
  if (code >= 49 && code <= 57)  return Scancode.N1 + (code - 49);
  if (code === 48)               return Scancode.N0;
  switch (c) {
    case ' ':    return Scancode.SPACE;
    case '\t':   return Scancode.TAB;
    case '\r':   return Scancode.RETURN;
    case '\n':   return Scancode.RETURN;
    case '\b':   return Scancode.BACKSPACE;
    case '\x7f': return Scancode.BACKSPACE;
    case '\x1b': return Scancode.ESCAPE;
    case '-':    return Scancode.MINUS;
    case '=':    return Scancode.EQUALS;
    case '[':    return Scancode.LEFTBRACKET;
    case ']':    return Scancode.RIGHTBRACKET;
    case '\\':   return Scancode.BACKSLASH;
    case ';':    return Scancode.SEMICOLON;
    case "'":    return Scancode.APOSTROPHE;
    case '`':    return Scancode.GRAVE;
    case ',':    return Scancode.COMMA;
    case '.':    return Scancode.PERIOD;
    case '/':    return Scancode.SLASH;
  }
  return -1;
}

// Kitty private-use codepoints for modifier and modifier-only keys we care
// about. Values from kitty keyboard protocol spec (functional keys table).
const KITTY_PUA_TO_SCANCODE: Record<number, number> = {
  57441: Scancode.LSHIFT,
  57442: Scancode.LCTRL,
  57443: Scancode.LALT,
  57447: Scancode.RSHIFT,
  57448: Scancode.RCTRL,
  57449: Scancode.RALT,
};

/** Edge-triggered key event for the engine's `key` control op. */
export type KeyEvent =
  | { kind: 'name'; name: 'up' | 'down' | 'left' | 'right' | 'tab' | 'enter' | 'escape' | 'backspace' }
  | { kind: 'char'; ascii: number };

/** Mouse event in terminal cell coordinates (1-based on the wire,
 *  0-based here). The host converts to engine pixels using the latest
 *  frame size before shipping to the engine. */
export type MouseEvent = {
  kind: 'press' | 'release' | 'move' | 'drag';
  cellX: number;
  cellY: number;
  /** True iff the left mouse button is currently held. */
  leftDown: boolean;
};

export class TerminalInput {
  // Held-scancode bitmask, wire-ready (bit i of byte i>>3).
  private state = new Uint8Array(SCANCODE_BYTES);
  // Last-seen timestamps per scancode for legacy autorelease.
  private lastSeen = new Map<number, number>();
  private kittyKbd = false;
  // Carries an incomplete CSI tail across stdin chunks.
  private kittyTail = '';
  // Carries an incomplete SGR mouse sequence across legacy-mode chunks.
  private legacyTail = '';
  // Mouse events parsed during feed() but not yet drained. SGR mouse
  // sequences arrive interleaved with keys; the host pulls them per-tick
  // via drainMouseEvents().
  private mouseQueue: MouseEvent[] = [];
  private leftDown = false;
  /** User pressed Ctrl-C / Ctrl-Q — host should exit. */
  quitRequested = false;

  setKittyKeyboard(enabled: boolean): void {
    this.kittyKbd = enabled;
  }

  feed(chunk: Buffer): KeyEvent[] {
    return this.kittyKbd ? this.feedKitty(chunk) : this.feedLegacy(chunk);
  }

  /** Held-scancode bitmask. Caller may not mutate. */
  snapshot(): Uint8Array {
    return this.state;
  }

  /** Drain mouse events buffered during feed(). Returns empty array if
   *  none. The host converts cell coords → engine pixel coords before
   *  shipping to the engine. */
  drainMouseEvents(): MouseEvent[] {
    if (this.mouseQueue.length === 0) return [];
    const out = this.mouseQueue;
    this.mouseQueue = [];
    return out;
  }

  /** Call once per tick. Skips when kitty mode delivers real release events,
   * but still drains lastSeen — handleKittyBareByte populates it for terminals
   * that advertise kitty kbd without honoring flag 0x8, and those scancodes
   * have no release event so they need autorelease. */
  decay(): void {
    if (this.kittyKbd && this.lastSeen.size === 0) return;
    const now = performance.now();
    for (const [sc, t] of this.lastSeen) {
      if (now - t > HOLD_MS) {
        this.scancodeOff(sc);
        this.lastSeen.delete(sc);
      }
    }
  }

  private scancodeOn(sc: number): void {
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return;
    this.state[sc >> 3] |= 1 << (sc & 7);
  }
  private scancodeOff(sc: number): void {
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return;
    this.state[sc >> 3] &= ~(1 << (sc & 7));
  }

  private feedLegacy(chunk: Buffer): KeyEvent[] {
    const events: KeyEvent[] = [];
    const s = this.legacyTail + chunk.toString('utf8');
    this.legacyTail = '';
    let i = 0;
    while (i < s.length) {
      const c = s[i]!;
      if (c === '\x03' || c === '\x11') {
        this.quitRequested = true;
        i++;
        continue;
      }
      // SGR mouse: \x1b[<btn;col;row M|m. Check before generic CSI so the
      // arrow-key matcher doesn't grab \x1b[< as a malformed arrow.
      if (c === '\x1b' && i + 2 < s.length && s[i + 1] === '[' && s[i + 2] === '<') {
        const consumed = this.tryParseSGRMouse(s, i);
        if (consumed > 0) {
          i += consumed;
          continue;
        }
        if (consumed < 0) {
          this.legacyTail = s.slice(i);
          return events;
        }
      }
      // CSI arrow keys: \x1b[A/B/C/D
      if (c === '\x1b' && i + 2 < s.length && s[i + 1] === '[') {
        const seq = s.slice(i, i + 3);
        const arrow = legacyArrowToScancode(seq);
        if (arrow >= 0) {
          this.scancodeOn(arrow);
          this.lastSeen.set(arrow, performance.now());
          const name = legacyArrowName(seq);
          if (name) events.push({ kind: 'name', name });
        }
        i += 3;
        continue;
      }
      // Lone Esc.
      if (c === '\x1b') {
        events.push({ kind: 'name', name: 'escape' });
        this.scancodeOn(Scancode.ESCAPE);
        this.lastSeen.set(Scancode.ESCAPE, performance.now());
        i++;
        continue;
      }
      // Named control chars.
      if (c === '\r' || c === '\n') events.push({ kind: 'name', name: 'enter' });
      else if (c === '\t')          events.push({ kind: 'name', name: 'tab' });
      else if (c === '\x7f' || c === '\b')
                                    events.push({ kind: 'name', name: 'backspace' });
      else if (c >= ' ' && c <= '~')
                                    events.push({ kind: 'char', ascii: c.charCodeAt(0) });

      const sc = asciiToScancode(c);
      if (sc >= 0) {
        this.scancodeOn(sc);
        this.lastSeen.set(sc, performance.now());
      }
      i++;
    }
    return events;
  }

  private feedKitty(chunk: Buffer): KeyEvent[] {
    const events: KeyEvent[] = [];
    // Prepend any tail left over from a split sequence.
    const s = this.kittyTail + chunk.toString('binary');
    this.kittyTail = '';
    let i = 0;
    while (i < s.length) {
      const c = s[i]!;
      if (c === '\x03' || c === '\x11') {
        this.quitRequested = true;
        i++;
        continue;
      }
      if (c === '\x1b' && i + 1 < s.length && s[i + 1] === '[') {
        // SGR mouse sequences share the CSI envelope but terminate on `M`/`m`
        // and start with `<`. Handle before the generic CSI matcher so the
        // mouse params (which include `;`) aren't misread as kitty kbd args.
        if (i + 2 < s.length && s[i + 2] === '<') {
          const consumed = this.tryParseSGRMouse(s, i);
          if (consumed > 0) {
            i += consumed;
            continue;
          }
          if (consumed < 0) {
            // Incomplete — partial sequence still pending bytes.
            this.kittyTail = s.slice(i);
            return events;
          }
        }
        let j = i + 2;
        while (j < s.length && !/[A-Za-z~]/.test(s[j]!)) j++;
        if (j >= s.length) {
          this.kittyTail = s.slice(i);
          return events;
        }
        const params = s.slice(i + 2, j);
        const suffix = s[j]!;
        i = j + 1;
        this.handleKittyEvent(params, suffix, events);
        continue;
      }
      if (c === '\x1b' && i + 1 >= s.length) {
        this.kittyTail = s.slice(i);
        return events;
      }
      // Bare-byte fallback for terminals that advertise kitty kbd but don't
      // honor flag 0x8 (report-all-keys-as-escape-codes). Still emit the
      // menu KeyEvent and toggle the scancode briefly (autorelease).
      this.handleKittyBareByte(c, events);
      i++;
    }
    return events;
  }

  private handleKittyEvent(
    params: string,
    suffix: string,
    events: KeyEvent[],
  ): void {
    let codeStr = '';
    let modsStr = '';
    let eventStr = '';
    let part = 0;
    for (const ch of params) {
      if (ch === ';') { part = 1; continue; }
      if (ch === ':' && part === 1) { part = 2; continue; }
      if (part === 0) codeStr += ch;
      else if (part === 1) modsStr += ch;
      else eventStr += ch;
    }
    const evType = eventStr ? parseInt(eventStr, 10) : 1;
    const isPress   = evType === 1;
    const isRepeat  = evType === 2;
    const isRelease = evType === 3;

    const modBits = modsStr ? parseInt(modsStr, 10) - 1 : 0;
    const ctrl = (modBits & 4) !== 0;

    let code = codeStr ? parseInt(codeStr, 10) : 0;

    // Ctrl-C / Ctrl-Q quit the TUI; don't propagate the scancode either.
    if (ctrl && isPress && (code === 99 || code === 113)) {
      this.quitRequested = true;
      return;
    }

    // Arrow keys keep their legacy A/B/C/D suffix in kitty mode.
    if (suffix === 'A' || suffix === 'B' || suffix === 'C' || suffix === 'D') {
      const sc = legacyArrowToScancode('\x1b[' + suffix);
      const name = legacyArrowName('\x1b[' + suffix);
      if (isPress || isRepeat) {
        if (sc >= 0) this.scancodeOn(sc);
        if (name) events.push({ kind: 'name', name });
      } else if (isRelease) {
        if (sc >= 0) this.scancodeOff(sc);
      }
      return;
    }
    if (suffix !== 'u') return;

    // Map the codepoint to an SDL scancode. ASCII range first, then PUA.
    let sc = -1;
    if (code >= 32 && code <= 126) {
      sc = asciiToScancode(String.fromCharCode(code));
    } else if (code === 27)  sc = Scancode.ESCAPE;
    else if (code === 13)    sc = Scancode.RETURN;
    else if (code === 9)     sc = Scancode.TAB;
    else if (code === 127 || code === 8) sc = Scancode.BACKSPACE;
    else if (KITTY_PUA_TO_SCANCODE[code] !== undefined) sc = KITTY_PUA_TO_SCANCODE[code]!;

    // Menu KeyEvent emission (press + repeat — repeat enables text autorepeat).
    if (isPress || isRepeat) {
      if (code === 27)             events.push({ kind: 'name', name: 'escape' });
      else if (code === 13)        events.push({ kind: 'name', name: 'enter' });
      else if (code === 9)         events.push({ kind: 'name', name: 'tab' });
      else if (code === 127 || code === 8)
                                   events.push({ kind: 'name', name: 'backspace' });
      else if (code >= 32 && code <= 126)
                                   events.push({ kind: 'char', ascii: code });
    }

    if (sc < 0) return;
    if (isPress || isRepeat) this.scancodeOn(sc);
    else if (isRelease)      this.scancodeOff(sc);
  }

  /** Try to parse a single SGR mouse report at s[start]. The form is
   *   `\x1b[<btn;col;row M`  (press / drag / motion)
   *   `\x1b[<btn;col;row m`  (release)
   *  with col/row 1-based.
   *
   *  Return value:
   *    >0  bytes consumed
   *    0   not an SGR mouse sequence (malformed)
   *   -1   incomplete — full terminator hasn't arrived yet, caller should
   *        stash and retry on the next chunk. */
  private tryParseSGRMouse(s: string, start: number): number {
    // Caller guarantees s[start..start+2] === '\x1b[<'.
    let j = start + 3;
    while (j < s.length && s[j] !== 'M' && s[j] !== 'm') j++;
    if (j >= s.length) return -1;
    const params = s.slice(start + 3, j);
    const isPress = s[j] === 'M';
    const parts = params.split(';');
    if (parts.length !== 3) return j + 1 - start; // malformed but consumed
    const btn = parseInt(parts[0]!, 10);
    const col = parseInt(parts[1]!, 10);
    const row = parseInt(parts[2]!, 10);
    if (!Number.isFinite(btn) || !Number.isFinite(col) || !Number.isFinite(row)) {
      return j + 1 - start;
    }
    // SGR button byte: low 2 bits select button (0=left, 1=middle, 2=right);
    // bit 5 (32) = motion (drag if any button held, else move); bit 6 (64) =
    // wheel (we ignore for now). Modifier bits (4=shift, 8=meta, 16=ctrl)
    // are ignored.
    const isWheel = (btn & 64) !== 0;
    const isMotion = (btn & 32) !== 0;
    const baseBtn = btn & 0x03;
    const cellX = Math.max(0, col - 1);
    const cellY = Math.max(0, row - 1);

    if (isWheel) {
      // Wheel events still update position but don't change leftDown.
      this.mouseQueue.push({
        kind: 'move',
        cellX,
        cellY,
        leftDown: this.leftDown,
      });
      return j + 1 - start;
    }

    let kind: MouseEvent['kind'];
    if (isMotion) {
      kind = this.leftDown ? 'drag' : 'move';
    } else if (isPress && baseBtn === 0) {
      this.leftDown = true;
      kind = 'press';
    } else if (!isPress && baseBtn === 0) {
      this.leftDown = false;
      kind = 'release';
    } else {
      // Non-left button press/release — still report position; leftDown
      // unchanged. Treat as 'move' so the host updates mousex/mousey only.
      kind = 'move';
    }

    this.mouseQueue.push({ kind, cellX, cellY, leftDown: this.leftDown });
    return j + 1 - start;
  }

  private handleKittyBareByte(c: string, events: KeyEvent[]): void {
    // Mirror the legacy menu path AND fire a brief autorelease scancode so
    // gameplay still works when the terminal under-implements mode 0x8.
    if (c === '\x1b') events.push({ kind: 'name', name: 'escape' });
    else if (c === '\r' || c === '\n') events.push({ kind: 'name', name: 'enter' });
    else if (c === '\t') events.push({ kind: 'name', name: 'tab' });
    else if (c === '\x7f' || c === '\b') events.push({ kind: 'name', name: 'backspace' });
    else if (c >= ' ' && c <= '~') events.push({ kind: 'char', ascii: c.charCodeAt(0) });

    const sc = asciiToScancode(c);
    if (sc >= 0) {
      this.scancodeOn(sc);
      this.lastSeen.set(sc, performance.now());
    }
  }
}

function legacyArrowToScancode(seq: string): number {
  switch (seq) {
    case '\x1b[A': return Scancode.UP;
    case '\x1b[B': return Scancode.DOWN;
    case '\x1b[C': return Scancode.RIGHT;
    case '\x1b[D': return Scancode.LEFT;
  }
  return -1;
}

function legacyArrowName(seq: string): 'up' | 'down' | 'left' | 'right' {
  switch (seq) {
    case '\x1b[A': return 'up';
    case '\x1b[B': return 'down';
    case '\x1b[C': return 'right';
    case '\x1b[D': return 'left';
  }
  // Unreachable — caller checked legacyArrowToScancode first.
  return 'up';
}
