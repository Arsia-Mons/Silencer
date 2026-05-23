import mainMenuSurfaceTokens from "../assets/ui-layouts/main-menu.silencer-ui.tokens.json";

export const UI_LAYOUT_SCHEMA_VERSION = 1 as const;

export type UiNodeKind =
  | "screen"
  | "panel"
  | "stack"
  | "row"
  | "text"
  | "button"
  | "input"
  | "spacer"
  | "component";
export type UiAxis = "row" | "column";
export type UiAlign = "start" | "center" | "end";
export type UiJustify = "start" | "center" | "end";
export type UiSizeMode = "fit" | "grow" | "fixed";
export type UiFont = "ui" | "uiLarge" | "title" | "tiny" | "footer";
export type UiMovePlacement = "inside" | "before" | "after";
export type UiButtonVariant = "oval" | "chrome" | "text" | "ghost";
export type UiButtonSize = "sm" | "md" | "lg" | "compact" | "auto";
export type UiImageMode = "normal" | "contain" | "stretch";
export type UiAttachTo = "parent" | "root";
export type UiAttachPoint =
  | "left-top"
  | "left-center"
  | "left-bottom"
  | "center-top"
  | "center"
  | "center-bottom"
  | "right-top"
  | "right-center"
  | "right-bottom";

export interface UiSize {
  mode: UiSizeMode;
  value?: number;
  min?: number;
  max?: number;
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

export interface UiImageRef {
  bank: number;
  index: number;
  mode?: UiImageMode;
}

export interface UiFloating {
  attachTo: UiAttachTo;
  elementAttach: UiAttachPoint;
  parentAttach: UiAttachPoint;
  offsetX?: number;
  offsetY?: number;
  zIndex?: number;
  pointerPassthrough?: boolean;
}

export interface UiNode {
  id: string;
  kind: UiNodeKind;
  name: string;
  text?: string;
  placeholder?: string;
  action?: string;
  textBinding?: string;
  component?: string;
  buttonVariant?: UiButtonVariant;
  buttonSize?: UiButtonSize;
  image?: UiImageRef;
  floating?: UiFloating;
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

export interface UiSurfaceTokenManifest {
  surface: string;
  components: string[];
  textBindings: string[];
  actions: string[];
}

type UiNodeOverrides = Omit<Partial<UiNode>, "style"> & {
  style?: Partial<UiStyle>;
};

const SURFACE_TOKEN_MANIFESTS: UiSurfaceTokenManifest[] = [
  mainMenuSurfaceTokens as UiSurfaceTokenManifest,
];

const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: "Screen",
  panel: "Panel",
  stack: "Stack",
  row: "Row",
  text: "Text",
  button: "Button",
  input: "Input",
  spacer: "Spacer",
  component: "Component",
};

export const PALETTE_NODE_KINDS: UiNodeKind[] = [
  "panel",
  "stack",
  "row",
  "text",
  "button",
  "input",
  "spacer",
  "component",
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
    viewport: { width: 640, height: 480 },
    root: {
      id: "MainMenuRoot",
      kind: "screen",
      name: "Main Menu",
      image: { bank: 6, index: 0, mode: "normal" },
      style: {
        width: { mode: "grow" },
        height: { mode: "grow" },
        direction: "column",
        align: "center",
        justify: "center",
        padding: 0,
        gap: 0,
      },
      children: [
        {
          id: "MainMenuLogoGroup",
          kind: "stack",
          name: "Logo Group",
          style: {
            width: { mode: "grow", max: 350 },
            height: { mode: "grow" },
            direction: "column",
            align: "center",
            justify: "center",
            padding: 0,
            gap: 0,
          },
          children: [
            {
              id: "MainMenuSilencerLogo",
              kind: "component",
              name: "Silencer Logo",
              component: "main-menu.logo",
              style: {
                width: { mode: "fit" },
                height: { mode: "fit" },
              },
            },
          ],
        },
        {
          id: "MainMenuActionGroup",
          kind: "panel",
          name: "Action Group",
          floating: {
            attachTo: "root",
            elementAttach: "center",
            parentAttach: "center",
            offsetX: 0,
            offsetY: 31,
            zIndex: 1,
          },
          style: {
            width: { mode: "fit" },
            height: { mode: "fit" },
            direction: "column",
            align: "start",
            justify: "start",
            padding: 0,
            gap: 0,
          },
          children: [
            {
              id: "MainMenuActionStack",
              kind: "stack",
              name: "Action Stack",
              style: {
                width: { mode: "fit" },
                height: { mode: "fit" },
                direction: "column",
                align: "start",
                justify: "start",
                padding: 0,
                gap: 34,
              },
              children: [
                createMainMenuActionRow("Tutorial", 40, "main_menu.tutorial"),
                createMainMenuActionRow("Connect To Lobby", 80, "main_menu.lobby"),
                createMainMenuActionRow("Options", 40, "main_menu.options"),
                createMainMenuActionRow("Exit", 0, "main_menu.exit"),
              ],
            },
          ],
        },
        {
          id: "MainMenuVersion",
          kind: "text",
          name: "Version Footer",
          text: "Silencer v00000",
          textBinding: "client.version",
          floating: {
            attachTo: "root",
            elementAttach: "left-top",
            parentAttach: "left-bottom",
            offsetX: 10,
            offsetY: -17,
            zIndex: 1,
            pointerPassthrough: true,
          },
          style: {
            width: { mode: "fit" },
            height: { mode: "fit" },
            font: "footer",
            textPalette: 112,
          },
        },
      ],
    },
  };
}

function createMainMenuActionRow(label: string, offset: number, action: string): UiNode {
  return {
    id: `MainMenu${toPascalCase(label)}Row`,
    kind: "row",
    name: `${label} Row`,
    style: {
      width: { mode: "fit" },
      height: { mode: "fit" },
      direction: "row",
      align: "start",
      justify: "start",
      padding: 0,
      gap: 0,
    },
    children: [
      {
        id: `MainMenu${toPascalCase(label)}Spacer`,
        kind: "spacer",
        name: `${label} Offset`,
        style: {
          width: { mode: "fixed", value: offset },
          height: { mode: "fixed", value: 1 },
        },
      },
      {
        id: `MainMenu${toPascalCase(label)}Button`,
        kind: "button",
        name: `${label} Button`,
        text: label,
        action,
        buttonVariant: "oval",
        buttonSize: "md",
        style: {
          width: { mode: "fixed", value: 196 },
          height: { mode: "fit" },
          textPalette: 0,
          padding: 0,
        },
      },
    ],
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
    base.buttonVariant = "chrome";
    base.buttonSize = "auto";
  }
  if (kind === "input") base.placeholder = "Value";
  if (kind === "component") base.component = "component-id";
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
  const document = { ...candidate, surface: normalizeUiSurface(candidate.surface) } as UiDocument;
  validateSurfaceTokens(document);
  return document;
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
  if (kind === "component") {
    return {
      width: { mode: "fit" },
      height: { mode: "fit" },
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
    width: { mode: "fixed", value: 180 },
    height: { mode: "fixed", value: 24 },
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
  validateOptionalString(node, "textBinding");
  validateOptionalString(node, "component");
  validateButtonSettings(node);
  validateRenderableNodeDecorators(node);
  validateImage(node);
  validateFloating(node);
  if (node.kind === "component" && !node.component) {
    throw new Error(`Node ${node.id} component is missing.`);
  }
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
  validateEnum(node, "font", ["ui", "uiLarge", "title", "tiny", "footer"]);
  validatePalette(node, "backgroundPalette");
  validatePalette(node, "borderPalette");
  validatePalette(node, "textPalette");
  if ((node.children?.length ?? 0) > 0 && !canHaveChildren(node.kind)) {
    throw new Error(`Node ${node.id} cannot have children.`);
  }
  for (const child of node.children ?? []) validateNode(child, seenIds);
}

function validateOptionalString(
  node: UiNode,
  key: "text" | "placeholder" | "action" | "textBinding" | "component",
): void {
  const value = node[key];
  if (value === undefined) return;
  if (typeof value !== "string") {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateButtonSettings(node: UiNode): void {
  if (node.buttonVariant !== undefined) {
    if (
      typeof node.buttonVariant !== "string" ||
      !["oval", "chrome", "text", "ghost"].includes(node.buttonVariant)
    ) {
      throw new Error(`Node ${node.id} has invalid buttonVariant.`);
    }
  }
  if (node.buttonSize !== undefined) {
    if (
      typeof node.buttonSize !== "string" ||
      !["sm", "md", "lg", "compact", "auto"].includes(node.buttonSize)
    ) {
      throw new Error(`Node ${node.id} has invalid buttonSize.`);
    }
  }
}

function validateRenderableNodeDecorators(node: UiNode): void {
  if ((node.kind === "button" || node.kind === "input") && node.image !== undefined) {
    throw new Error(`Node ${node.id} ${node.kind} cannot use image.`);
  }
  if ((node.kind === "button" || node.kind === "input") && node.floating !== undefined) {
    throw new Error(`Node ${node.id} ${node.kind} cannot use floating.`);
  }
}

function validateImage(node: UiNode): void {
  if (node.image === undefined) return;
  if (!node.image || typeof node.image !== "object" || Array.isArray(node.image)) {
    throw new Error(`Node ${node.id} has invalid image.`);
  }
  if (!Number.isInteger(node.image.bank) || node.image.bank < 0 || node.image.bank > 255) {
    throw new Error(`Node ${node.id} has invalid image bank.`);
  }
  if (!Number.isInteger(node.image.index) || node.image.index < 0 || node.image.index > 65535) {
    throw new Error(`Node ${node.id} has invalid image index.`);
  }
  const mode = node.image.mode ?? "normal";
  if (!["normal", "contain", "stretch"].includes(mode)) {
    throw new Error(`Node ${node.id} has invalid image mode.`);
  }
}

function validateFloating(node: UiNode): void {
  if (node.floating === undefined) return;
  if (!node.floating || typeof node.floating !== "object" || Array.isArray(node.floating)) {
    throw new Error(`Node ${node.id} has invalid floating.`);
  }
  if (!["parent", "root"].includes(node.floating.attachTo)) {
    throw new Error(`Node ${node.id} has invalid floating attachTo.`);
  }
  validateAttachPoint(node, node.floating.elementAttach, "elementAttach");
  validateAttachPoint(node, node.floating.parentAttach, "parentAttach");
  validateFloatingNumber(node, "offsetX", -4096, 4096);
  validateFloatingNumber(node, "offsetY", -4096, 4096);
  validateFloatingNumber(node, "zIndex", -32768, 32767, true);
  if (
    node.floating.pointerPassthrough !== undefined &&
    typeof node.floating.pointerPassthrough !== "boolean"
  ) {
    throw new Error(`Node ${node.id} has invalid floating pointerPassthrough.`);
  }
}

function validateAttachPoint(
  node: UiNode,
  value: UiAttachPoint,
  key: "elementAttach" | "parentAttach",
): void {
  if (
    typeof value !== "string" ||
    ![
      "left-top",
      "left-center",
      "left-bottom",
      "center-top",
      "center",
      "center-bottom",
      "right-top",
      "right-center",
      "right-bottom",
    ].includes(value)
  ) {
    throw new Error(`Node ${node.id} has invalid floating ${key}.`);
  }
}

function validateFloatingNumber(
  node: UiNode,
  key: "offsetX" | "offsetY" | "zIndex",
  min: number,
  max: number,
  integer = false,
): void {
  const value = node.floating?.[key];
  if (value === undefined) return;
  if (typeof value !== "number" || !Number.isFinite(value)) {
    throw new Error(`Node ${node.id} has invalid floating ${key}.`);
  }
  if (integer && !Number.isInteger(value)) {
    throw new Error(`Node ${node.id} has invalid floating ${key}.`);
  }
  if (value < min || value > max) {
    throw new Error(`Node ${node.id} has invalid floating ${key}.`);
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
  if (node.kind === "button") {
    if (node.style.width.mode === "grow") {
      throw new Error(`Node ${node.id} button width must be fit or fixed.`);
    }
    if (node.style.width.min !== undefined || node.style.width.max !== undefined) {
      throw new Error(`Node ${node.id} button width cannot use min or max.`);
    }
    if (node.style.height.mode !== "fit") {
      throw new Error(`Node ${node.id} button height must be fit.`);
    }
    if (node.style.height.min !== undefined || node.style.height.max !== undefined) {
      throw new Error(`Node ${node.id} button height cannot use min or max.`);
    }
  }
  if (node.kind === "input") {
    if (node.style.width.mode !== "fixed" || node.style.height.mode !== "fixed") {
      throw new Error(`Node ${node.id} input width and height must be fixed.`);
    }
    if (
      node.style.width.min !== undefined ||
      node.style.width.max !== undefined ||
      node.style.height.min !== undefined ||
      node.style.height.max !== undefined
    ) {
      throw new Error(`Node ${node.id} input sizing cannot use min or max.`);
    }
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
  validateSizeBound(node, key, "min");
  validateSizeBound(node, key, "max");
  if (size.max !== undefined && size.min !== undefined && size.min > size.max) {
    throw new Error(`Node ${node.id} ${key} min cannot exceed max.`);
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

function validateSizeBound(
  node: UiNode,
  key: "width" | "height",
  bound: "min" | "max",
): void {
  const value = node.style[key][bound];
  if (value === undefined) return;
  if (!Number.isFinite(value) || value < 0 || value > 4096) {
    throw new Error(`Node ${node.id} has invalid ${key} ${bound}.`);
  }
}

function validateSurfaceTokens(document: UiDocument): void {
  const manifest = surfaceTokenManifest(document.surface);
  if (!manifest) {
    if (nodeHasSurfaceTokens(document.root)) {
      throw new Error(`Surface ${document.surface} needs a UI token manifest.`);
    }
    return;
  }
  validateSurfaceNodeTokens(document.root, manifest);
}

function surfaceTokenManifest(surface: string): UiSurfaceTokenManifest | null {
  const normalized = normalizeUiSurface(surface);
  return SURFACE_TOKEN_MANIFESTS.find((manifest) => normalizeUiSurface(manifest.surface) === normalized) ??
    null;
}

function validateSurfaceNodeTokens(node: UiNode, manifest: UiSurfaceTokenManifest): void {
  if (node.kind === "component" && !manifest.components.includes(node.component ?? "")) {
    throw new Error(`Node ${node.id} references unknown component ${node.component ?? ""}.`);
  }
  if (node.textBinding && !manifest.textBindings.includes(node.textBinding)) {
    throw new Error(`Node ${node.id} references unknown text binding ${node.textBinding}.`);
  }
  if (node.kind === "button" && !manifest.actions.includes(node.action ?? "")) {
    throw new Error(`Node ${node.id} references unknown action ${node.action ?? ""}.`);
  }
  for (const child of node.children ?? []) validateSurfaceNodeTokens(child, manifest);
}

function nodeHasSurfaceTokens(node: UiNode): boolean {
  if (node.kind === "component" || node.kind === "button" || node.textBinding) return true;
  return (node.children ?? []).some(nodeHasSurfaceTokens);
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
