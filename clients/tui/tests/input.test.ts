import { test, expect } from 'bun:test';
import { TerminalInput, type KeyEvent } from '../src/input';

function ev(input: TerminalInput, bytes: string): KeyEvent[] {
  return input.feed(Buffer.from(bytes));
}

test('Enter key emits enter event', () => {
  const ti = new TerminalInput();
  expect(ev(ti, '\r')).toEqual([{ kind: 'name', name: 'enter' }]);
  expect(ev(ti, '\n')).toEqual([{ kind: 'name', name: 'enter' }]);
});

test('arrow CSI sequences emit named events', () => {
  const ti = new TerminalInput();
  expect(ev(ti, '\x1b[A')).toEqual([{ kind: 'name', name: 'up' }]);
  expect(ev(ti, '\x1b[B')).toEqual([{ kind: 'name', name: 'down' }]);
  expect(ev(ti, '\x1b[C')).toEqual([{ kind: 'name', name: 'right' }]);
  expect(ev(ti, '\x1b[D')).toEqual([{ kind: 'name', name: 'left' }]);
});

test('Tab/Esc/Backspace map to named events', () => {
  const ti = new TerminalInput();
  expect(ev(ti, '\t')).toEqual([{ kind: 'name', name: 'tab' }]);
  expect(ev(ti, '\x1b')).toEqual([{ kind: 'name', name: 'escape' }]);
  expect(ev(ti, '\x7f')).toEqual([{ kind: 'name', name: 'backspace' }]);
});

test('printable ASCII passes through as char events', () => {
  const ti = new TerminalInput();
  expect(ev(ti, 'a')).toEqual([{ kind: 'char', ascii: 97 }]);
  expect(ev(ti, 'Z')).toEqual([{ kind: 'char', ascii: 90 }]);
  expect(ev(ti, '5')).toEqual([{ kind: 'char', ascii: 53 }]);
});

test('Ctrl-C sets quitRequested without emitting an event', () => {
  const ti = new TerminalInput();
  expect(ev(ti, '\x03')).toEqual([]);
  expect(ti.quitRequested).toBe(true);
});

test('mixed chunk yields events in order', () => {
  const ti = new TerminalInput();
  // Down, Down, Enter — the menu_test sequence.
  expect(ev(ti, '\x1b[B\x1b[B\r')).toEqual([
    { kind: 'name', name: 'down' },
    { kind: 'name', name: 'down' },
    { kind: 'name', name: 'enter' },
  ]);
});

test('SGR mouse left-press emits press + tracks leftDown', () => {
  const ti = new TerminalInput();
  // CSI < 0 ; 10 ; 5 M — left button press at col 10, row 5 (1-based).
  expect(ev(ti, '\x1b[<0;10;5M')).toEqual([]);
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'press', cellX: 9, cellY: 4, leftDown: true },
  ]);
});

test('SGR mouse left-release emits release with leftDown=false', () => {
  const ti = new TerminalInput();
  ev(ti, '\x1b[<0;10;5M');
  ti.drainMouseEvents();
  ev(ti, '\x1b[<0;12;7m'); // lowercase m = release
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'release', cellX: 11, cellY: 6, leftDown: false },
  ]);
});

test('SGR mouse motion: drag while held vs move when not', () => {
  const ti = new TerminalInput();
  // Motion only (btn 32+3 = 35 = motion, no button) without a press — bare move.
  ev(ti, '\x1b[<35;3;4M');
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'move', cellX: 2, cellY: 3, leftDown: false },
  ]);
  // Press, then motion-with-button-held (32 = motion bit + 0 = left) = drag.
  ev(ti, '\x1b[<0;5;6M');
  ev(ti, '\x1b[<32;7;8M');
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'press', cellX: 4, cellY: 5, leftDown: true },
    { kind: 'drag', cellX: 6, cellY: 7, leftDown: true },
  ]);
});

test('SGR mouse interleaved with keys', () => {
  const ti = new TerminalInput();
  // 'a', mouse press, 'b'.
  const keys = ev(ti, 'a\x1b[<0;1;1Mb');
  expect(keys).toEqual([
    { kind: 'char', ascii: 97 },
    { kind: 'char', ascii: 98 },
  ]);
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'press', cellX: 0, cellY: 0, leftDown: true },
  ]);
});

test('SGR mouse split across chunks', () => {
  const ti = new TerminalInput();
  // Partial — terminator missing. Should buffer and emit nothing yet.
  expect(ev(ti, '\x1b[<0;10;5')).toEqual([]);
  expect(ti.drainMouseEvents()).toEqual([]);
  // Tail arrives next chunk.
  ev(ti, 'M');
  expect(ti.drainMouseEvents()).toEqual([
    { kind: 'press', cellX: 9, cellY: 4, leftDown: true },
  ]);
});
