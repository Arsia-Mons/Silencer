import { describe, expect, test } from 'bun:test';
import {
  createDefaultUiDocument,
  createNode,
  duplicateNode,
  exportClaySnippet,
  findNode,
  insertChild,
  removeNode,
  validateUiDocument,
} from './ui-layout';

describe('ui-layout model', () => {
  test('inserts children into container nodes immutably', () => {
    const original = createDefaultUiDocument();
    const child = createNode('text', 'status', { text: 'READY' });
    const updated = insertChild(original, 'main-menu-panel', child);

    expect(findNode(original.root, 'text-status')).toBeNull();
    expect(findNode(updated.root, 'text-status')?.text).toBe('READY');
    expect(findNode(updated.root, 'main-menu-panel')?.children?.at(-1)?.id).toBe('text-status');
  });

  test('does not remove the root node', () => {
    const document = createDefaultUiDocument();
    const updated = removeNode(document, document.root.id);
    expect(updated.root.id).toBe(document.root.id);
    expect(updated.root.children?.length).toBe(document.root.children?.length);
  });

  test('duplicates a selected subtree with new ids', () => {
    const document = createDefaultUiDocument();
    const updated = duplicateNode(document, 'main-menu-panel');
    const rootChildren = updated.root.children ?? [];

    expect(rootChildren).toHaveLength(3);
    expect(rootChildren[2].id).not.toBe('main-menu-panel');
    expect(rootChildren[2].children?.[0].id).not.toBe('button-host-game');
  });

  test('validates imported documents and exports a Clay scaffold', () => {
    const document = createDefaultUiDocument();
    const parsed = validateUiDocument(JSON.parse(JSON.stringify(document)));
    const snippet = exportClaySnippet(parsed);

    expect(snippet).toContain('BuildMainMenuUi');
    expect(snippet).toContain('CLAY_ID("main-menu-root")');
    expect(snippet).toContain('Button("HOST GAME", "open-host-game")');
  });

  test('rejects duplicate imported node ids', () => {
    const document = createDefaultUiDocument();
    const imported = JSON.parse(JSON.stringify(document));
    imported.root.children[1].children[0].id = 'main-menu-title';

    expect(() => validateUiDocument(imported)).toThrow('Duplicate node id: main-menu-title');
  });

  test('rejects layout values that the client preview cannot render', () => {
    const document = createDefaultUiDocument();
    const imported = JSON.parse(JSON.stringify(document));
    imported.root.style.justify = 'between';

    expect(() => validateUiDocument(imported)).toThrow('Node main-menu-root has invalid justify.');
  });

  test('rejects imported documents with missing or invalid viewport', () => {
    const missing = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    delete missing.viewport;
    expect(() => validateUiDocument(missing)).toThrow('Document viewport is missing.');

    const invalid = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    invalid.viewport.width = Number.NaN;
    expect(() => validateUiDocument(invalid)).toThrow('Document viewport width is invalid.');
  });

  test('rejects imported nodes with invalid optional scalar fields', () => {
    const document = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    document.root.children[0].text = {};
    expect(() => validateUiDocument(document)).toThrow('Node main-menu-title has invalid text.');

    const actionDocument = JSON.parse(JSON.stringify(createDefaultUiDocument()));
    actionDocument.root.children[1].children[0].action = 42;
    expect(() => validateUiDocument(actionDocument)).toThrow('Node button-host-game has invalid action.');
  });
});
