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
