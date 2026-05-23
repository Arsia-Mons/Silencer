export const UI_LAYOUT_SCHEMA_VERSION = 1 as const;

export type UiNodeKind =
  | "screen"
  | "panel"
  | "stack"
  | "row"
  | "text"
  | "button"
  | "input"
  | "spacer";
export type UiAxis = "row" | "column";
export type UiAlign = "start" | "center" | "end";
export type UiJustify = "start" | "center" | "end";
export type UiSizeMode = "fit" | "grow" | "fixed";
export type UiFont = "ui" | "uiLarge" | "title" | "tiny";
export type UiMovePlacement = "inside" | "before" | "after";

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

export interface UiDocumentReference {
  surface: string;
  filename: string;
  title: string;
  updatedAt: string;
  revision: string;
}

type UiNodeOverrides = Omit<Partial<UiNode>, "style"> & {
  style?: Partial<UiStyle>;
};

const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: "Screen",
  panel: "Panel",
  stack: "Stack",
  row: "Row",
  text: "Text",
  button: "Button",
  input: "Input",
  spacer: "Spacer",
};

export const PALETTE_NODE_KINDS: UiNodeKind[] = [
  "panel",
  "stack",
  "row",
  "text",
  "button",
  "input",
  "spacer",
];

export function canHaveChildren(kind: UiNodeKind): boolean {
  return kind === "screen" || kind === "panel" || kind === "stack" || kind === "row";
}

export function normalizeUiSurface(value: string): string {
  return (
    value
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/(^-|-$)/g, "") || "unnamed"
  );
}

export function uiLayoutFilename(surface: string): string {
  return `${normalizeUiSurface(surface)}.silencer-ui.json`;
}

export function createDefaultUiDocument(): UiDocument {
  return {
    schemaVersion: UI_LAYOUT_SCHEMA_VERSION,
    surface: "main-menu",
    viewport: { width: 1280, height: 720 },
    root: {
      id: "main-menu-root",
      kind: "screen",
      name: "Main Menu",
      style: {
        width: { mode: "fixed", value: 1280 },
        height: { mode: "fixed", value: 720 },
        direction: "column",
        align: "center",
        justify: "center",
        padding: 48,
        gap: 28,
        backgroundPalette: 0,
      },
      children: [
        {
          id: "main-menu-title",
          kind: "text",
          name: "Title",
          text: "SILENCER",
          style: {
            width: { mode: "fit" },
            height: { mode: "fit" },
            font: "title",
            textPalette: 112,
          },
        },
        {
          id: "main-menu-panel",
          kind: "panel",
          name: "Menu Panel",
          style: {
            width: { mode: "fixed", value: 360 },
            height: { mode: "fit" },
            direction: "column",
            align: "center",
            justify: "start",
            padding: 18,
            gap: 10,
            backgroundPalette: 74,
            borderPalette: 216,
            radius: 2,
          },
          children: [
            createNode("button", "host-game", {
              text: "HOST GAME",
              action: "open-host-game",
              style: { width: { mode: "fixed", value: 320 } },
            }),
            createNode("button", "join-game", {
              text: "JOIN GAME",
              action: "open-join-game",
              style: { width: { mode: "fixed", value: 320 } },
            }),
            createNode("button", "options", {
              text: "OPTIONS",
              action: "open-options",
              style: { width: { mode: "fixed", value: 320 } },
            }),
          ],
        },
      ],
    },
  };
}

export function createNode(
  kind: UiNodeKind,
  idSeed = nextIdSeed(),
  overrides: UiNodeOverrides = {},
): UiNode {
  const id = `${kind}-${idSeed}`;
  const base: UiNode = {
    id,
    kind,
    name: KIND_LABELS[kind],
    style: defaultStyleForKind(kind),
  };

  if (kind === "text") base.text = "Text";
  if (kind === "button") {
    base.text = "Button";
    base.action = "action-id";
  }
  if (kind === "input") base.placeholder = "Value";
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

export function updateNode(
  document: UiDocument,
  id: string,
  update: (node: UiNode) => UiNode,
): UiDocument {
  return { ...document, root: updateNodeInTree(document.root, id, update) };
}

export function insertChild(document: UiDocument, parentId: string, child: UiNode): UiDocument {
  const parent = findNode(document.root, parentId);
  if (!parent || !canHaveChildren(parent.kind)) return document;

  return updateNode(document, parentId, (node) => ({
    ...node,
    children: [...(node.children ?? []), child],
  }));
}

export function insertAfter(
  document: UiDocument,
  siblingId: string,
  nodeToInsert: UiNode,
): UiDocument {
  const parent = findParent(document.root, siblingId);
  if (!parent) return document;

  return updateNode(document, parent.id, (node) => {
    const children = node.children ?? [];
    const siblingIndex = children.findIndex((child) => child.id === siblingId);
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

export function insertBefore(
  document: UiDocument,
  siblingId: string,
  nodeToInsert: UiNode,
): UiDocument {
  const parent = findParent(document.root, siblingId);
  if (!parent) return document;

  return updateNode(document, parent.id, (node) => {
    const children = node.children ?? [];
    const siblingIndex = children.findIndex((child) => child.id === siblingId);
    if (siblingIndex < 0) return node;
    return {
      ...node,
      children: [...children.slice(0, siblingIndex), nodeToInsert, ...children.slice(siblingIndex)],
    };
  });
}

export function removeNode(document: UiDocument, id: string): UiDocument {
  if (document.root.id === id) return document;
  return { ...document, root: removeNodeFromTree(document.root, id) };
}

export function moveNode(
  document: UiDocument,
  nodeId: string,
  target: { targetId: string; placement: UiMovePlacement },
): UiDocument {
  if (nodeId === document.root.id || nodeId === target.targetId) return document;
  const moving = findNode(document.root, nodeId);
  const targetNode = findNode(document.root, target.targetId);
  if (!moving || !targetNode || containsNode(moving, target.targetId)) return document;
  if (target.placement === "inside" && !canHaveChildren(targetNode.kind)) return document;
  if (target.placement !== "inside" && !findParent(document.root, target.targetId)) return document;

  const withoutMoving = removeNode(document, nodeId);
  if (target.placement === "inside") return insertChild(withoutMoving, target.targetId, moving);
  if (target.placement === "before") return insertBefore(withoutMoving, target.targetId, moving);
  return insertAfter(withoutMoving, target.targetId, moving);
}

export function duplicateNode(document: UiDocument, id: string): UiDocument {
  const node = findNode(document.root, id);
  if (!node || node.id === document.root.id) return document;
  return insertAfter(document, id, cloneNodeWithNewIds(node));
}

export function validateUiDocument(value: unknown): UiDocument {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Document must be an object.");
  }
  const candidate = value as Partial<UiDocument>;
  if (candidate.schemaVersion !== UI_LAYOUT_SCHEMA_VERSION) {
    throw new Error(`Unsupported UI layout schema: ${String(candidate.schemaVersion)}`);
  }
  if (!candidate.root || typeof candidate.root !== "object")
    throw new Error("Document root is missing.");
  if (!candidate.surface || typeof candidate.surface !== "string")
    throw new Error("Surface name is missing.");
  if (!candidate.viewport || typeof candidate.viewport !== "object")
    throw new Error("Document viewport is missing.");
  validateViewport(candidate.viewport);
  if (candidate.root.kind !== "screen") throw new Error("Document root must be a screen node.");
  validateNode(candidate.root, new Set<string>());
  return { ...candidate, surface: normalizeUiSurface(candidate.surface) } as UiDocument;
}

export function exportClaySnippet(document: UiDocument): string {
  const fnName = `Build${toPascalCase(document.surface)}Ui`;
  const lines = [
    `void ${fnName}(ScreenContext& ctx) {`,
    `  // Generated scaffold from ${document.surface}.silencer-ui.json.`,
    ...formatClayNode(document.root, 1),
    `}`,
  ];
  return lines.join("\n");
}

function defaultStyleForKind(kind: UiNodeKind): UiStyle {
  if (kind === "screen") {
    return {
      width: { mode: "fixed", value: 1280 },
      height: { mode: "fixed", value: 720 },
      direction: "column",
      align: "start",
      justify: "start",
      padding: 24,
      gap: 12,
      backgroundPalette: 0,
    };
  }
  if (kind === "panel") {
    return {
      width: { mode: "fixed", value: 320 },
      height: { mode: "fit" },
      direction: "column",
      align: "start",
      justify: "start",
      padding: 14,
      gap: 8,
      backgroundPalette: 74,
      borderPalette: 216,
      radius: 2,
    };
  }
  if (kind === "stack" || kind === "row") {
    return {
      width: { mode: "grow" },
      height: { mode: "fit" },
      direction: kind === "row" ? "row" : "column",
      align: "start",
      justify: "start",
      padding: 0,
      gap: 8,
    };
  }
  if (kind === "spacer") {
    return {
      width: { mode: "grow" },
      height: { mode: "fixed", value: 12 },
    };
  }
  if (kind === "text") {
    return {
      width: { mode: "fit" },
      height: { mode: "fit" },
      font: "uiLarge",
      textPalette: 0,
    };
  }
  if (kind === "button") {
    return {
      width: { mode: "fit" },
      height: { mode: "fit" },
      textPalette: 0,
      padding: 10,
    };
  }
  return {
    width: { mode: "fit" },
    height: { mode: "fit" },
    font: "ui",
    padding: 10,
  };
}

function updateNodeInTree(node: UiNode, id: string, update: (node: UiNode) => UiNode): UiNode {
  if (node.id === id) return update(node);
  if (!node.children) return node;
  return {
    ...node,
    children: node.children.map((child) => updateNodeInTree(child, id, update)),
  };
}

function removeNodeFromTree(node: UiNode, id: string): UiNode {
  if (!node.children) return node;
  return {
    ...node,
    children: node.children
      .filter((child) => child.id !== id)
      .map((child) => removeNodeFromTree(child, id)),
  };
}

function containsNode(node: UiNode, id: string): boolean {
  if (node.id === id) return true;
  return (node.children ?? []).some((child) => containsNode(child, id));
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
  if (!node.id || typeof node.id !== "string") throw new Error("Node id is missing.");
  if (seenIds.has(node.id)) throw new Error(`Duplicate node id: ${node.id}`);
  seenIds.add(node.id);
  if (!node.kind || !Object.prototype.hasOwnProperty.call(KIND_LABELS, node.kind))
    throw new Error(`Unsupported node kind: ${String(node.kind)}`);
  if (!node.name || typeof node.name !== "string")
    throw new Error(`Node ${node.id} name is missing.`);
  validateOptionalString(node, "text");
  validateOptionalString(node, "placeholder");
  validateOptionalString(node, "action");
  if (!node.style || typeof node.style !== "object")
    throw new Error(`Node ${node.id} style is missing.`);
  validateStyleFields(node);
  validateSize(node, "width");
  validateSize(node, "height");
  validateKindSpecificStyleValues(node);
  validateNumber(node, "padding", 0, 512);
  validateNumber(node, "gap", 0, 512);
  validateNumber(node, "radius", 0, 64);
  validateEnum(node, "direction", ["row", "column"]);
  validateEnum(node, "align", ["start", "center", "end"]);
  validateEnum(node, "justify", ["start", "center", "end"]);
  validateEnum(node, "font", ["ui", "uiLarge", "title", "tiny"]);
  validatePalette(node, "backgroundPalette");
  validatePalette(node, "borderPalette");
  validatePalette(node, "textPalette");
  if ((node.children?.length ?? 0) > 0 && !canHaveChildren(node.kind)) {
    throw new Error(`Node ${node.id} cannot have children.`);
  }
  for (const child of node.children ?? []) validateNode(child, seenIds);
}

function validateOptionalString(node: UiNode, key: "text" | "placeholder" | "action"): void {
  const value = node[key];
  if (value === undefined) return;
  if (typeof value !== "string") {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateStyleFields(node: UiNode): void {
  const allowed = allowedStyleFields(node.kind);
  for (const key of Object.keys(node.style)) {
    if (!allowed.has(key as keyof UiStyle)) {
      throw new Error(`Node ${node.id} has unsupported ${key} style.`);
    }
  }
}

function allowedStyleFields(kind: UiNodeKind): Set<keyof UiStyle> {
  if (canHaveChildren(kind)) {
    return new Set([
      "width",
      "height",
      "direction",
      "align",
      "justify",
      "padding",
      "gap",
      "backgroundPalette",
      "borderPalette",
      "radius",
    ]);
  }
  if (kind === "text") {
    return new Set([
      "width",
      "height",
      "padding",
      "backgroundPalette",
      "borderPalette",
      "textPalette",
      "font",
      "radius",
    ]);
  }
  if (kind === "button") return new Set(["width", "height", "padding", "textPalette"]);
  if (kind === "input") return new Set(["width", "height", "padding", "font"]);
  return new Set(["width", "height"]);
}

function validateKindSpecificStyleValues(node: UiNode): void {
  if (node.kind === "button" && node.style.height.mode !== "fit") {
    throw new Error(`Node ${node.id} button height must be fit.`);
  }
}

function validateViewport(viewport: UiDocument["viewport"]): void {
  if (!Number.isInteger(viewport.width) || viewport.width < 160 || viewport.width > 4096) {
    throw new Error("Document viewport width is invalid.");
  }
  if (!Number.isInteger(viewport.height) || viewport.height < 160 || viewport.height > 4096) {
    throw new Error("Document viewport height is invalid.");
  }
}

function validateNumber(
  node: UiNode,
  key: "padding" | "gap" | "radius",
  min: number,
  max: number,
): void {
  const value = node.style[key];
  if (value === undefined) return;
  if (!Number.isInteger(value) || value < min || value > max) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateEnum<T extends string>(
  node: UiNode,
  key: "direction" | "align" | "justify" | "font",
  values: T[],
): void {
  const value = node.style[key];
  if (value === undefined) return;
  if (typeof value !== "string" || !values.includes(value as T)) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validatePalette(
  node: UiNode,
  key: "backgroundPalette" | "borderPalette" | "textPalette",
): void {
  const value = node.style[key];
  if (value === undefined) return;
  const min = key === "textPalette" ? 0 : -1;
  if (!Number.isInteger(value) || value < min || value > 255) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateSize(node: UiNode, key: "width" | "height"): void {
  const size = node.style[key];
  if (!size || (size.mode !== "fit" && size.mode !== "grow" && size.mode !== "fixed")) {
    throw new Error(`Node ${node.id} has invalid ${key} sizing.`);
  }
  if (size.mode === "fixed") {
    const value = size.value;
    if (typeof value !== "number") {
      throw new Error(`Node ${node.id} fixed ${key} sizing needs a value.`);
    }
    if (!Number.isFinite(value) || value < 0 || value > 4096) {
      throw new Error(`Node ${node.id} has invalid fixed ${key} sizing.`);
    }
  }
}

let idCounter = 0;
function nextIdSeed(): string {
  idCounter += 1;
  return `${Date.now().toString(36)}-${idCounter.toString(36)}`;
}

function toPascalCase(value: string): string {
  const normalized = value.trim() || "UiSurface";
  return normalized
    .split(/[^a-z0-9]+/i)
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join("");
}

function formatClayNode(node: UiNode, depth: number): string[] {
  const indent = "  ".repeat(depth);
  const childIndent = "  ".repeat(depth + 1);
  const lines = [`${indent}CLAY(CLAY_ID("${escapeForCpp(node.id)}"), ${formatClayLayout(node)}) {`];

  if (node.kind === "text" && node.text) {
    lines.push(`${childIndent}Text("${escapeForCpp(node.text)}");`);
  } else if (node.kind === "button") {
    lines.push(
      `${childIndent}Button("${escapeForCpp(node.text ?? node.name)}", "${escapeForCpp(node.action ?? "")}");`,
    );
  } else if (node.kind === "input") {
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
  if (style.direction)
    parts.push(
      `.layoutDirection = ${style.direction === "row" ? "CLAY_LEFT_TO_RIGHT" : "CLAY_TOP_TO_BOTTOM"}`,
    );
  if (style.padding) parts.push(`.padding = CLAY_PADDING_ALL(${style.padding})`);
  if (style.gap) parts.push(`.childGap = ${style.gap}`);
  return `{ ${parts.join(", ")} }`;
}

function formatClaySizing(size: UiSize): string {
  if (size.mode === "fixed")
    return `CLAY_SIZING_FIXED(${Math.max(0, Math.round(size.value ?? 0))})`;
  if (size.mode === "grow") return "CLAY_SIZING_GROW(0)";
  return "CLAY_SIZING_FIT(0)";
}

function escapeForCpp(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}
