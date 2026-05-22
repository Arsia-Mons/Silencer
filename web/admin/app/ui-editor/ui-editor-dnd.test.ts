import { describe, expect, test } from 'bun:test';
import { createDefaultUiDocument, createNode } from '../../lib/ui-layout';
import { resolveMovePlacement } from './ui-editor-dnd';

describe('ui-editor drag placement', () => {
  test('maps target edges to sibling moves and target centers to reparenting', () => {
    const panel = createNode('panel', 'drop-target');
    const bounds = { width: 100, height: 100 };

    expect(resolveMovePlacement(panel, { x: 50, y: 5 }, bounds, 'vertical')).toBe('before');
    expect(resolveMovePlacement(panel, { x: 50, y: 50 }, bounds, 'vertical')).toBe('inside');
    expect(resolveMovePlacement(panel, { x: 50, y: 95 }, bounds, 'vertical')).toBe('after');

    expect(resolveMovePlacement(panel, { x: 5, y: 50 }, bounds, 'horizontal')).toBe('before');
    expect(resolveMovePlacement(panel, { x: 95, y: 50 }, bounds, 'horizontal')).toBe('after');
  });

  test('keeps root drops inside and leaf center drops after the leaf', () => {
    const document = createDefaultUiDocument();
    const button = createNode('button', 'drop-target');
    const bounds = { width: 100, height: 100 };

    expect(resolveMovePlacement(document.root, { x: 50, y: 5 }, bounds, 'vertical')).toBe('inside');
    expect(resolveMovePlacement(button, { x: 50, y: 50 }, bounds, 'vertical')).toBe('after');
  });
});
