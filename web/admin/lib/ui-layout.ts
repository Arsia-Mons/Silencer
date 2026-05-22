export const UI_LAYOUT_SCHEMA_VERSION = 1 as const;

export type UiNodeKind = 'screen' | 'panel' | 'stack' | 'row' | 'text' | 'button' | 'input' | 'spacer';
export type UiAxis = 'row' | 'column';
export type UiAlign = 'start' | 'center' | 'end';
export type UiJustify = 'start' | 'center' | 'end';
export type UiSizeMode = 'fit' | 'grow' | 'fixed';
export type UiFont = 'ui' | 'uiLarge' | 'title' | 'tiny';

export interface UiSize {
  mode: UiSizeMode;
  value?: number;
}

export interface UiStyle {
  width: UiSize;
  height: UiSize;
  direction?: UiAxis;
  align?: UiAlign;
  justify?: UiJustify;
  padding?: number;
  gap?: number;
  background?: string;
  border?: string;
  textColor?: string;
  backgroundPalette?: number;
  borderPalette?: number;
  textPalette?: number;
  font?: UiFont;
  radius?: number;
}

export interface UiNode {
  id: string;
  kind: UiNodeKind;
  name: string;
  text?: string;
  placeholder?: string;
  action?: string;
  style: UiStyle;
  children?: UiNode[];
}

export interface UiDocument {
  schemaVersion: typeof UI_LAYOUT_SCHEMA_VERSION;
  surface: string;
  viewport: {
    width: number;
    height: number;
  };
  root: UiNode;
}

type UiNodeOverrides = Omit<Partial<UiNode>, 'style'> & {
  style?: Partial<UiStyle>;
};

const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: 'Screen',
  panel: 'Panel',
  stack: 'Stack',
  row: 'Row',
  text: 'Text',
  button: 'Button',
  input: 'Input',
  spacer: 'Spacer',
};

export const PALETTE_NODE_KINDS: UiNodeKind[] = ['panel', 'stack', 'row', 'text', 'button', 'input', 'spacer'];

export function canHaveChildren(kind: UiNodeKind): boolean {
  return kind === 'screen' || kind === 'panel' || kind === 'stack' || kind === 'row';
}

export function createDefaultUiDocument(): UiDocument {
  return {
    schemaVersion: UI_LAYOUT_SCHEMA_VERSION,
    surface: 'main-menu',
    viewport: { width: 1280, height: 720 },
    root: {
      id: 'main-menu-root',
      kind: 'screen',
      name: 'Main Menu',
      style: {
        width: { mode: 'fixed', value: 1280 },
        height: { mode: 'fixed', value: 720 },
        direction: 'column',
        align: 'center',
        justify: 'center',
        padding: 48,
        gap: 28,
        background: '#050a05',
        textColor: '#d1fad7',
        backgroundPalette: 0,
        textPalette: 112,
        font: 'ui',
      },
      children: [
        {
          id: 'main-menu-title',
          kind: 'text',
          name: 'Title',
          text: 'SILENCER',
          style: {
            width: { mode: 'fit' },
            height: { mode: 'fit' },
            textColor: '#00a328',
            font: 'title',
          },
        },
        {
          id: 'main-menu-panel',
          kind: 'panel',
          name: 'Menu Panel',
          style: {
            width: { mode: 'fixed', value: 360 },
            height: { mode: 'fit' },
            direction: 'column',
            align: 'center',
            justify: 'start',
            padding: 18,
            gap: 10,
            background: '#10141c',
            border: '#565e6f',
            textColor: '#e0e7f1',
            backgroundPalette: 74,
            borderPalette: 216,
            textPalette: 0,
            font: 'ui',
            radius: 2,
          },
          children: [
            createNode('button', 'host-game', {
              text: 'HOST GAME',
              action: 'open-host-game',
              style: { width: { mode: 'fixed', value: 320 } },
            }),
            createNode('button', 'join-game', {
              text: 'JOIN GAME',
              action: 'open-join-game',
              style: { width: { mode: 'fixed', value: 320 } },
            }),
            createNode('button', 'options', {
              text: 'OPTIONS',
              action: 'open-options',
              style: { width: { mode: 'fixed', value: 320 } },
            }),
          ],
        },
      ],
    },
  };
}

export function createNode(kind: UiNodeKind, idSeed = nextIdSeed(), overrides: UiNodeOverrides = {}): UiNode {
  const id = `${kind}-${idSeed}`;
  const base: UiNode = {
    id,
    kind,
    name: KIND_LABELS[kind],
    style: defaultStyleForKind(kind),
  };

  if (kind === 'text') base.text = 'Text';
  if (kind === 'button') {
    base.text = 'Button';
    base.action = 'action-id';
  }
  if (kind === 'input') base.placeholder = 'Value';
  if (canHaveChildren(kind)) base.children = [];

  return {
    ...base,
    ...overrides,
    style: { ...base.style, ...overrides.style },
    children: overrides.children ?? base.children,
  };
}

export function findNode(root: UiNode, id: string): UiNode | null {
  if (root.id === id) return root;
  for (const child of root.children ?? []) {
    const found = findNode(child, id);
    if (found) return found;
  }
  return null;
}

export function findParent(root: UiNode, id: string): UiNode | null {
  for (const child of root.children ?? []) {
    if (child.id === id) return root;
    const found = findParent(child, id);
    if (found) return found;
  }
  return null;
}

export function updateNode(document: UiDocument, id: string, update: (node: UiNode) => UiNode): UiDocument {
  return { ...document, root: updateNodeInTree(document.root, id, update) };
}

export function insertChild(document: UiDocument, parentId: string, child: UiNode): UiDocument {
  const parent = findNode(document.root, parentId);
  if (!parent || !canHaveChildren(parent.kind)) return document;

  return updateNode(document, parentId, node => ({
    ...node,
    children: [...(node.children ?? []), child],
  }));
}

export function insertAfter(document: UiDocument, siblingId: string, nodeToInsert: UiNode): UiDocument {
  const parent = findParent(document.root, siblingId);
  if (!parent) return document;

  return updateNode(document, parent.id, node => {
    const children = node.children ?? [];
    const siblingIndex = children.findIndex(child => child.id === siblingId);
    if (siblingIndex < 0) return node;
    return {
      ...node,
      children: [
        ...children.slice(0, siblingIndex + 1),
        nodeToInsert,
        ...children.slice(siblingIndex + 1),
      ],
    };
  });
}

export function removeNode(document: UiDocument, id: string): UiDocument {
  if (document.root.id === id) return document;
  return { ...document, root: removeNodeFromTree(document.root, id) };
}

export function duplicateNode(document: UiDocument, id: string): UiDocument {
  const node = findNode(document.root, id);
  if (!node || node.id === document.root.id) return document;
  return insertAfter(document, id, cloneNodeWithNewIds(node));
}

export function validateUiDocument(value: unknown): UiDocument {
  if (!value || typeof value !== 'object') throw new Error('Document must be an object.');
  const candidate = value as Partial<UiDocument>;
  if (candidate.schemaVersion !== UI_LAYOUT_SCHEMA_VERSION) {
    throw new Error(`Unsupported UI layout schema: ${String(candidate.schemaVersion)}`);
  }
  if (!candidate.root || typeof candidate.root !== 'object') throw new Error('Document root is missing.');
  if (!candidate.surface || typeof candidate.surface !== 'string') throw new Error('Surface name is missing.');
  if (candidate.root.kind !== 'screen') throw new Error('Document root must be a screen node.');
  validateNode(candidate.root, new Set<string>());
  return candidate as UiDocument;
}

export function exportClaySnippet(document: UiDocument): string {
  const fnName = `Build${toPascalCase(document.surface)}Ui`;
  const lines = [
    `void ${fnName}(ScreenContext& ctx) {`,
    `  // Generated scaffold from ${document.surface}.silencer-ui.json.`,
    ...formatClayNode(document.root, 1),
    `}`,
  ];
  return lines.join('\n');
}

function defaultStyleForKind(kind: UiNodeKind): UiStyle {
  if (kind === 'screen') {
    return {
      width: { mode: 'fixed', value: 1280 },
      height: { mode: 'fixed', value: 720 },
      direction: 'column',
      align: 'start',
      justify: 'start',
      padding: 24,
      gap: 12,
      background: '#050a05',
      textColor: '#d1fad7',
      backgroundPalette: 0,
      textPalette: 0,
      font: 'ui',
    };
  }
  if (kind === 'panel') {
    return {
      width: { mode: 'fixed', value: 320 },
      height: { mode: 'fit' },
      direction: 'column',
      align: 'start',
      justify: 'start',
      padding: 14,
      gap: 8,
      background: '#10141c',
      border: '#565e6f',
      textColor: '#e0e7f1',
      backgroundPalette: 74,
      borderPalette: 216,
      textPalette: 0,
      font: 'ui',
      radius: 2,
    };
  }
  if (kind === 'stack' || kind === 'row') {
    return {
      width: { mode: 'grow' },
      height: { mode: 'fit' },
      direction: kind === 'row' ? 'row' : 'column',
      align: 'start',
      justify: 'start',
      padding: 0,
      gap: 8,
      textColor: '#d1fad7',
      textPalette: 0,
      font: 'ui',
    };
  }
  if (kind === 'spacer') {
    return {
      width: { mode: 'grow' },
      height: { mode: 'fixed', value: 12 },
    };
  }
  return {
    width: { mode: 'fit' },
    height: { mode: 'fit' },
    textColor: kind === 'button' ? '#050a05' : '#d1fad7',
    font: kind === 'text' ? 'uiLarge' : 'ui',
    background: kind === 'button' ? '#00a328' : undefined,
    border: kind === 'input' ? '#565e6f' : undefined,
    borderPalette: kind === 'input' ? 216 : undefined,
    textPalette: 0,
    padding: kind === 'button' || kind === 'input' ? 10 : 0,
    radius: 2,
  };
}

function updateNodeInTree(node: UiNode, id: string, update: (node: UiNode) => UiNode): UiNode {
  if (node.id === id) return update(node);
  if (!node.children) return node;
  return {
    ...node,
    children: node.children.map(child => updateNodeInTree(child, id, update)),
  };
}

function removeNodeFromTree(node: UiNode, id: string): UiNode {
  if (!node.children) return node;
  return {
    ...node,
    children: node.children
      .filter(child => child.id !== id)
      .map(child => removeNodeFromTree(child, id)),
  };
}

function cloneNodeWithNewIds(node: UiNode): UiNode {
  const idSeed = nextIdSeed();
  return {
    ...node,
    id: `${node.kind}-${idSeed}`,
    name: `${node.name} Copy`,
    children: node.children?.map(cloneNodeWithNewIds),
  };
}

function validateNode(node: UiNode, seenIds: Set<string>): void {
  if (!node.id || typeof node.id !== 'string') throw new Error('Node id is missing.');
  if (seenIds.has(node.id)) throw new Error(`Duplicate node id: ${node.id}`);
  seenIds.add(node.id);
  if (!node.kind || !KIND_LABELS[node.kind]) throw new Error(`Unsupported node kind: ${String(node.kind)}`);
  if (!node.name || typeof node.name !== 'string') throw new Error(`Node ${node.id} name is missing.`);
  if (!node.style || typeof node.style !== 'object') throw new Error(`Node ${node.id} style is missing.`);
  validateSize(node, 'width');
  validateSize(node, 'height');
  validateEnum(node, 'direction', ['row', 'column']);
  validateEnum(node, 'align', ['start', 'center', 'end']);
  validateEnum(node, 'justify', ['start', 'center', 'end']);
  validateEnum(node, 'font', ['ui', 'uiLarge', 'title', 'tiny']);
  validatePalette(node, 'backgroundPalette');
  validatePalette(node, 'borderPalette');
  validatePalette(node, 'textPalette');
  if ((node.children?.length ?? 0) > 0 && !canHaveChildren(node.kind)) {
    throw new Error(`Node ${node.id} cannot have children.`);
  }
  for (const child of node.children ?? []) validateNode(child, seenIds);
}

function validateEnum<T extends string>(node: UiNode, key: 'direction' | 'align' | 'justify' | 'font', values: T[]): void {
  const value = node.style[key];
  if (value === undefined) return;
  if (typeof value !== 'string' || !values.includes(value as T)) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validatePalette(node: UiNode, key: 'backgroundPalette' | 'borderPalette' | 'textPalette'): void {
  const value = node.style[key];
  if (value === undefined) return;
  const min = key === 'textPalette' ? 0 : -1;
  if (!Number.isInteger(value) || value < min || value > 255) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateSize(node: UiNode, key: 'width' | 'height'): void {
  const size = node.style[key];
  if (!size || (size.mode !== 'fit' && size.mode !== 'grow' && size.mode !== 'fixed')) {
    throw new Error(`Node ${node.id} has invalid ${key} sizing.`);
  }
  if (size.mode === 'fixed' && typeof size.value !== 'number') {
    throw new Error(`Node ${node.id} fixed ${key} sizing needs a value.`);
  }
}

let idCounter = 0;
function nextIdSeed(): string {
  idCounter += 1;
  return `${Date.now().toString(36)}-${idCounter.toString(36)}`;
}

function toPascalCase(value: string): string {
  const normalized = value.trim() || 'UiSurface';
  return normalized
    .split(/[^a-z0-9]+/i)
    .filter(Boolean)
    .map(part => part.charAt(0).toUpperCase() + part.slice(1))
    .join('');
}

function formatClayNode(node: UiNode, depth: number): string[] {
  const indent = '  '.repeat(depth);
  const childIndent = '  '.repeat(depth + 1);
  const lines = [
    `${indent}CLAY(CLAY_ID("${escapeForCpp(node.id)}"), ${formatClayLayout(node)}) {`,
  ];

  if (node.kind === 'text' && node.text) {
    lines.push(`${childIndent}Text("${escapeForCpp(node.text)}");`);
  } else if (node.kind === 'button') {
    lines.push(`${childIndent}Button("${escapeForCpp(node.text ?? node.name)}", "${escapeForCpp(node.action ?? '')}");`);
  } else if (node.kind === 'input') {
    lines.push(`${childIndent}TextInput("${escapeForCpp(node.placeholder ?? node.name)}");`);
  }

  for (const child of node.children ?? []) {
    lines.push(...formatClayNode(child, depth + 1));
  }
  lines.push(`${indent}}`);
  return lines;
}

function formatClayLayout(node: UiNode): string {
  const style = node.style;
  const parts = [
    `.sizing = { ${formatClaySizing(style.width)}, ${formatClaySizing(style.height)} }`,
  ];
  if (style.direction) parts.push(`.layoutDirection = ${style.direction === 'row' ? 'CLAY_LEFT_TO_RIGHT' : 'CLAY_TOP_TO_BOTTOM'}`);
  if (style.padding) parts.push(`.padding = CLAY_PADDING_ALL(${style.padding})`);
  if (style.gap) parts.push(`.childGap = ${style.gap}`);
  return `{ ${parts.join(', ')} }`;
}

function formatClaySizing(size: UiSize): string {
  if (size.mode === 'fixed') return `CLAY_SIZING_FIXED(${Math.max(0, Math.round(size.value ?? 0))})`;
  if (size.mode === 'grow') return 'CLAY_SIZING_GROW(0)';
  return 'CLAY_SIZING_FIT(0)';
}

function escapeForCpp(value: string): string {
  return value.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
}
