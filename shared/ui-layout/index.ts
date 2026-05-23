import {
  UI_ALIGNS,
  UI_ATTACH_POINTS,
  UI_ATTACH_TO_VALUES,
  UI_AXES,
  UI_BUTTON_SIZES,
  UI_BUTTON_VARIANTS,
  UI_CONTAINER_NODE_KINDS,
  UI_DOCUMENT_FIELDS,
  UI_FONTS,
  UI_FLOATING_FIELDS,
  UI_FORBIDDEN_NODE_DECORATORS_BY_KIND,
  UI_IMAGE_FIELDS,
  UI_IMAGE_MODES,
  UI_JUSTIFIES,
  UI_LAYOUT_SCHEMA_VERSION,
  UI_NODE_FIELDS,
  UI_NODE_KINDS,
  UI_NODE_TOKEN_FIELDS_BY_KIND,
  UI_NUMERIC_LIMITS,
  UI_REQUIRED_TOKEN_FIELDS_BY_KIND,
  UI_SIZE_FIELDS,
  UI_SIZE_MODES,
  UI_SIZE_RULES_BY_KIND,
  UI_STYLE_FIELDS_BY_KIND,
  UI_STYLE_DEFAULTS_BY_KIND,
  UI_SURFACES,
  UI_SURFACE_TOKENS_BY_SURFACE,
  UI_VIEWPORT_FIELDS,
} from "./contract";
import mainMenuDocument from "../assets/ui-layouts/main-menu.silencer-ui.json";
import optionsAudioDocument from "../assets/ui-layouts/options-audio.silencer-ui.json";
import optionsDisplayDocument from "../assets/ui-layouts/options-display.silencer-ui.json";
import optionsDocument from "../assets/ui-layouts/options.silencer-ui.json";

export {
  UI_ALIGNS,
  UI_ATTACH_POINTS,
  UI_ATTACH_TO_VALUES,
  UI_AXES,
  UI_BUTTON_SIZES,
  UI_BUTTON_VARIANTS,
  UI_CONTAINER_NODE_KINDS,
  UI_DOCUMENT_FIELDS,
  UI_FONTS,
  UI_FLOATING_FIELDS,
  UI_FORBIDDEN_NODE_DECORATORS_BY_KIND,
  UI_IMAGE_FIELDS,
  UI_IMAGE_MODES,
  UI_JUSTIFIES,
  UI_LAYOUT_SCHEMA_VERSION,
  UI_NODE_FIELDS,
  UI_NODE_KINDS,
  UI_NODE_TOKEN_FIELDS_BY_KIND,
  UI_NUMERIC_LIMITS,
  UI_REQUIRED_TOKEN_FIELDS_BY_KIND,
  UI_SIZE_FIELDS,
  UI_SIZE_MODES,
  UI_SIZE_RULES_BY_KIND,
  UI_STYLE_FIELDS_BY_KIND,
  UI_STYLE_DEFAULTS_BY_KIND,
  UI_SURFACES,
  UI_SURFACE_TOKENS_BY_SURFACE,
  UI_VIEWPORT_FIELDS,
} from "./contract";

export type UiNodeKind = (typeof UI_NODE_KINDS)[number];
export type UiAxis = (typeof UI_AXES)[number];
export type UiAlign = (typeof UI_ALIGNS)[number];
export type UiJustify = (typeof UI_JUSTIFIES)[number];
export type UiSizeMode = (typeof UI_SIZE_MODES)[number];
export type UiFont = (typeof UI_FONTS)[number];
export type UiMovePlacement = "inside" | "before" | "after";
export type UiButtonVariant = (typeof UI_BUTTON_VARIANTS)[number];
export type UiButtonSize = (typeof UI_BUTTON_SIZES)[number];
export type UiImageMode = (typeof UI_IMAGE_MODES)[number];
export type UiAttachTo = (typeof UI_ATTACH_TO_VALUES)[number];
export type UiAttachPoint = (typeof UI_ATTACH_POINTS)[number];
type UiSurfaceName = (typeof UI_SURFACES)[number];

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
  components: readonly string[];
  textBindings: readonly string[];
  actions: readonly string[];
}

export interface UiDocumentValidationOptions {
  tokenManifests?: UiSurfaceTokenManifest[];
  requireTokenManifestForSurfaceTokens?: boolean;
}

type UiNodeOverrides = Omit<Partial<UiNode>, "style"> & {
  style?: Partial<UiStyle>;
};

const DOCUMENT_FIELDS = new Set<string>(UI_DOCUMENT_FIELDS);
const VIEWPORT_FIELDS = new Set<string>(UI_VIEWPORT_FIELDS);
const NODE_FIELDS = new Set<string>(UI_NODE_FIELDS);
const SIZE_FIELDS = new Set<string>(UI_SIZE_FIELDS);
const IMAGE_FIELDS = new Set<string>(UI_IMAGE_FIELDS);
const FLOATING_FIELDS = new Set<string>(UI_FLOATING_FIELDS);
const CONTAINER_NODE_KINDS = new Set<string>(UI_CONTAINER_NODE_KINDS);
const AXES = new Set<string>(UI_AXES);
const ALIGNS = new Set<string>(UI_ALIGNS);
const JUSTIFIES = new Set<string>(UI_JUSTIFIES);
const SIZE_MODES = new Set<string>(UI_SIZE_MODES);
const FONTS = new Set<string>(UI_FONTS);
const BUTTON_VARIANTS = new Set<string>(UI_BUTTON_VARIANTS);
const BUTTON_SIZES = new Set<string>(UI_BUTTON_SIZES);
const IMAGE_MODES = new Set<string>(UI_IMAGE_MODES);
const ATTACH_TO_VALUES = new Set<string>(UI_ATTACH_TO_VALUES);
const ATTACH_POINTS = new Set<string>(UI_ATTACH_POINTS);
const SURFACES = new Set<string>(UI_SURFACES);
const TOKEN_FIELD_KEYS = [
  "text",
  "action",
  "textBinding",
  "component",
  "buttonVariant",
  "buttonSize",
] as const;

const DEFAULT_UI_DOCUMENTS_BY_SURFACE = {
  "main-menu": mainMenuDocument,
  options: optionsDocument,
  "options-display": optionsDisplayDocument,
  "options-audio": optionsAudioDocument,
} as const satisfies Record<UiSurfaceName, unknown>;
const SIZE_AXIS_KEYS = ["width", "height"] as const;

const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: "Screen",
  panel: "Panel",
  stack: "Stack",
  row: "Row",
  text: "Text",
  button: "Button",
  spacer: "Spacer",
  component: "Component",
};

export const PALETTE_NODE_KINDS: UiNodeKind[] = [
  "panel",
  "stack",
  "row",
  "text",
  "button",
  "spacer",
  "component",
];

export function canHaveChildren(kind: UiNodeKind): boolean {
  return CONTAINER_NODE_KINDS.has(kind);
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

export function createDefaultUiDocument(surface = "main-menu"): UiDocument {
  const normalizedSurface = normalizeUiSurface(surface);
  if (!SURFACES.has(normalizedSurface)) {
    throw new Error(`No default UI document for surface: ${surface}`);
  }
  return validateUiDocument(
    cloneJson(DEFAULT_UI_DOCUMENTS_BY_SURFACE[normalizedSurface as UiSurfaceName]),
  );
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

export function validateUiSurfaceTokenManifest(value: unknown): UiSurfaceTokenManifest {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("UI surface token manifest must be an object.");
  }
  const candidate = value as Partial<UiSurfaceTokenManifest>;
  if (!candidate.surface || typeof candidate.surface !== "string") {
    throw new Error("UI surface token manifest surface is missing.");
  }
  return {
    surface: normalizeUiSurface(candidate.surface),
    components: validateTokenArray(candidate.components, "components"),
    textBindings: validateTokenArray(candidate.textBindings, "textBindings"),
    actions: validateTokenArray(candidate.actions, "actions"),
  };
}

export function validateUiDocument(
  value: unknown,
  options: UiDocumentValidationOptions = {},
): UiDocument {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Document must be an object.");
  }
  rejectUnknownObjectFields(value as Record<string, unknown>, DOCUMENT_FIELDS, "Document");
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
  validateSurfaceTokens(document, options);
  return document;
}

export function getUiSurfaceTokenManifest(
  surface: string,
  manifests: readonly UiSurfaceTokenManifest[] = [],
): UiSurfaceTokenManifest | null {
  return surfaceTokenManifest(surface, manifests) ?? builtInSurfaceTokenManifest(surface);
}

function defaultStyleForKind(kind: UiNodeKind): UiStyle {
  return cloneJson(UI_STYLE_DEFAULTS_BY_KIND[kind]) as UiStyle;
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

function rejectUnknownObjectFields(
  value: Record<string, unknown>,
  allowedFields: ReadonlySet<string>,
  label: string,
): void {
  for (const key of Object.keys(value)) {
    if (!allowedFields.has(key)) {
      throw new Error(`${label} has unsupported field: ${key}.`);
    }
  }
}

function isRecordObject(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}

function cloneJson<T>(value: T): T {
  return JSON.parse(JSON.stringify(value)) as T;
}

function validateNode(value: unknown, seenIds: Set<string>): void {
  if (!isRecordObject(value)) throw new Error("Node must be an object.");
  const node = value as unknown as UiNode;
  const nodeLabel = typeof value.id === "string" ? `Node ${value.id}` : "Node";
  rejectUnknownObjectFields(value, NODE_FIELDS, nodeLabel);
  if (!node.id || typeof node.id !== "string") throw new Error("Node id is missing.");
  if (seenIds.has(node.id)) throw new Error(`Duplicate node id: ${node.id}`);
  seenIds.add(node.id);
  if (!node.kind || !Object.prototype.hasOwnProperty.call(KIND_LABELS, node.kind))
    throw new Error(`Unsupported node kind: ${String(node.kind)}`);
  if (!node.name || typeof node.name !== "string")
    throw new Error(`Node ${node.id} name is missing.`);
  validateOptionalString(node, "text");
  validateOptionalString(node, "action");
  validateOptionalString(node, "textBinding");
  validateOptionalString(node, "component");
  validateKindSpecificFields(node);
  validateButtonSettings(node);
  validateRenderableNodeDecorators(node);
  validateImage(node);
  validateFloating(node);
  if (node.kind === "component" && !node.component) {
    throw new Error(`Node ${node.id} component is missing.`);
  }
  if (!isRecordObject(node.style)) throw new Error(`Node ${node.id} style is missing.`);
  validateStyleFields(node);
  validateSize(node, "width");
  validateSize(node, "height");
  validateKindSpecificStyleValues(node);
  validateNumber(node, "padding", UI_NUMERIC_LIMITS.paddingMin, UI_NUMERIC_LIMITS.paddingMax);
  validateNumber(node, "gap", UI_NUMERIC_LIMITS.gapMin, UI_NUMERIC_LIMITS.gapMax);
  validateNumber(node, "radius", UI_NUMERIC_LIMITS.radiusMin, UI_NUMERIC_LIMITS.radiusMax);
  validateEnum(node, "direction", AXES);
  validateEnum(node, "align", ALIGNS);
  validateEnum(node, "justify", JUSTIFIES);
  validateEnum(node, "font", FONTS);
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
  key: "text" | "action" | "textBinding" | "component",
): void {
  const value = node[key];
  if (value === undefined) return;
  if (typeof value !== "string") {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateTokenArray(value: unknown, key: string): string[] {
  if (!Array.isArray(value)) {
    throw new Error(`UI surface token manifest ${key} must be an array.`);
  }
  for (const item of value) {
    if (typeof item !== "string" || item.length === 0) {
      throw new Error(`UI surface token manifest ${key} entries must be non-empty strings.`);
    }
  }
  return [...value];
}

function validateButtonSettings(node: UiNode): void {
  if (node.buttonVariant !== undefined) {
    if (typeof node.buttonVariant !== "string" || !BUTTON_VARIANTS.has(node.buttonVariant)) {
      throw new Error(`Node ${node.id} has invalid buttonVariant.`);
    }
  }
  if (node.buttonSize !== undefined) {
    if (typeof node.buttonSize !== "string" || !BUTTON_SIZES.has(node.buttonSize)) {
      throw new Error(`Node ${node.id} has invalid buttonSize.`);
    }
  }
}

function validateKindSpecificFields(node: UiNode): void {
  const allowed = new Set<string>(UI_NODE_TOKEN_FIELDS_BY_KIND[node.kind]);
  for (const field of TOKEN_FIELD_KEYS) {
    if (node[field] !== undefined && !allowed.has(field)) {
      throw new Error(`Node ${node.id} ${node.kind} cannot use ${field}.`);
    }
  }
  for (const field of UI_REQUIRED_TOKEN_FIELDS_BY_KIND[node.kind]) {
    if (!node[field as keyof UiNode]) {
      throw new Error(`Node ${node.id} ${field} is missing.`);
    }
  }
}

function validateRenderableNodeDecorators(node: UiNode): void {
  for (const field of UI_FORBIDDEN_NODE_DECORATORS_BY_KIND[node.kind]) {
    if (node[field as "image" | "floating"] !== undefined) {
      throw new Error(`Node ${node.id} ${node.kind} cannot use ${field}.`);
    }
  }
}

function validateImage(node: UiNode): void {
  if (node.image === undefined) return;
  if (!isRecordObject(node.image)) {
    throw new Error(`Node ${node.id} has invalid image.`);
  }
  rejectUnknownObjectFields(node.image, IMAGE_FIELDS, `Node ${node.id} image`);
  if (
    !Number.isInteger(node.image.bank) ||
    node.image.bank < UI_NUMERIC_LIMITS.imageBankMin ||
    node.image.bank > UI_NUMERIC_LIMITS.imageBankMax
  ) {
    throw new Error(`Node ${node.id} has invalid image bank.`);
  }
  if (
    !Number.isInteger(node.image.index) ||
    node.image.index < UI_NUMERIC_LIMITS.imageIndexMin ||
    node.image.index > UI_NUMERIC_LIMITS.imageIndexMax
  ) {
    throw new Error(`Node ${node.id} has invalid image index.`);
  }
  const mode = node.image.mode ?? "normal";
  if (!IMAGE_MODES.has(mode)) {
    throw new Error(`Node ${node.id} has invalid image mode.`);
  }
}

function validateFloating(node: UiNode): void {
  if (node.floating === undefined) return;
  if (!isRecordObject(node.floating)) {
    throw new Error(`Node ${node.id} has invalid floating.`);
  }
  rejectUnknownObjectFields(node.floating, FLOATING_FIELDS, `Node ${node.id} floating`);
  if (!ATTACH_TO_VALUES.has(node.floating.attachTo)) {
    throw new Error(`Node ${node.id} has invalid floating attachTo.`);
  }
  validateAttachPoint(node, node.floating.elementAttach, "elementAttach");
  validateAttachPoint(node, node.floating.parentAttach, "parentAttach");
  validateFloatingNumber(
    node,
    "offsetX",
    UI_NUMERIC_LIMITS.floatingOffsetMin,
    UI_NUMERIC_LIMITS.floatingOffsetMax,
  );
  validateFloatingNumber(
    node,
    "offsetY",
    UI_NUMERIC_LIMITS.floatingOffsetMin,
    UI_NUMERIC_LIMITS.floatingOffsetMax,
  );
  validateFloatingNumber(
    node,
    "zIndex",
    UI_NUMERIC_LIMITS.floatingZIndexMin,
    UI_NUMERIC_LIMITS.floatingZIndexMax,
    true,
  );
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
  if (typeof value !== "string" || !ATTACH_POINTS.has(value)) {
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
  return new Set(UI_STYLE_FIELDS_BY_KIND[kind] as readonly (keyof UiStyle)[]);
}

function validateKindSpecificStyleValues(node: UiNode): void {
  const rules = UI_SIZE_RULES_BY_KIND[node.kind as keyof typeof UI_SIZE_RULES_BY_KIND];
  if (!rules) return;
  for (const axis of SIZE_AXIS_KEYS) {
    const rule = rules[axis as keyof typeof rules];
    if (!rule) continue;
    const size = node.style[axis];
    if (!rule.modes.includes(size.mode as never)) {
      throw new Error(`Node ${node.id} ${node.kind} ${axis} must be ${joinHuman(rule.modes)}.`);
    }
    if (!rule.allowBounds && (size.min !== undefined || size.max !== undefined)) {
      throw new Error(`Node ${node.id} ${node.kind} ${axis} cannot use min or max.`);
    }
  }
}

function validateViewport(viewport: unknown): void {
  if (!isRecordObject(viewport)) throw new Error("Document viewport is missing.");
  rejectUnknownObjectFields(viewport, VIEWPORT_FIELDS, "Document viewport");
  const width = viewport.width;
  const height = viewport.height;
  if (
    typeof width !== "number" ||
    !Number.isInteger(width) ||
    width < UI_NUMERIC_LIMITS.viewportMin ||
    width > UI_NUMERIC_LIMITS.viewportMax
  ) {
    throw new Error("Document viewport width is invalid.");
  }
  if (
    typeof height !== "number" ||
    !Number.isInteger(height) ||
    height < UI_NUMERIC_LIMITS.viewportMin ||
    height > UI_NUMERIC_LIMITS.viewportMax
  ) {
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

function validateEnum(
  node: UiNode,
  key: "direction" | "align" | "justify" | "font",
  values: ReadonlySet<string>,
): void {
  const value = node.style[key];
  if (value === undefined) return;
  if (typeof value !== "string" || !values.has(value)) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validatePalette(
  node: UiNode,
  key: "backgroundPalette" | "borderPalette" | "textPalette",
): void {
  const value = node.style[key];
  if (value === undefined) return;
  const min =
    key === "textPalette" ? UI_NUMERIC_LIMITS.paletteTextMin : UI_NUMERIC_LIMITS.paletteMin;
  if (!Number.isInteger(value) || value < min || value > UI_NUMERIC_LIMITS.paletteMax) {
    throw new Error(`Node ${node.id} has invalid ${key}.`);
  }
}

function validateSize(node: UiNode, key: "width" | "height"): void {
  const size = node.style[key];
  if (!isRecordObject(size)) {
    throw new Error(`Node ${node.id} has invalid ${key} sizing.`);
  }
  rejectUnknownObjectFields(size, SIZE_FIELDS, `Node ${node.id} ${key} sizing`);
  if (typeof size.mode !== "string" || !SIZE_MODES.has(size.mode)) {
    throw new Error(`Node ${node.id} has invalid ${key} sizing.`);
  }
  validateSizeBound(node, key, "min");
  validateSizeBound(node, key, "max");
  if (size.max !== undefined && size.min !== undefined && size.min > size.max) {
    throw new Error(`Node ${node.id} ${key} min cannot exceed max.`);
  }
  if (size.mode === "fixed") {
    if (size.min !== undefined || size.max !== undefined) {
      throw new Error(`Node ${node.id} fixed ${key} sizing cannot use min or max.`);
    }
    const value = size.value;
    if (typeof value !== "number") {
      throw new Error(`Node ${node.id} fixed ${key} sizing needs a value.`);
    }
    if (
      !Number.isFinite(value) ||
      value < UI_NUMERIC_LIMITS.sizeMin ||
      value > UI_NUMERIC_LIMITS.sizeMax
    ) {
      throw new Error(`Node ${node.id} has invalid fixed ${key} sizing.`);
    }
  } else if (size.value !== undefined) {
    throw new Error(`Node ${node.id} ${key} value is only valid for fixed sizing.`);
  }
}

function validateSizeBound(node: UiNode, key: "width" | "height", bound: "min" | "max"): void {
  const value = node.style[key][bound];
  if (value === undefined) return;
  if (
    !Number.isFinite(value) ||
    value < UI_NUMERIC_LIMITS.sizeMin ||
    value > UI_NUMERIC_LIMITS.sizeMax
  ) {
    throw new Error(`Node ${node.id} has invalid ${key} ${bound}.`);
  }
}

function validateSurfaceTokens(document: UiDocument, options: UiDocumentValidationOptions): void {
  const manifest = getUiSurfaceTokenManifest(document.surface, options.tokenManifests ?? []);
  if (!manifest) {
    if (options.requireTokenManifestForSurfaceTokens && nodeHasSurfaceTokens(document.root)) {
      throw new Error(`Surface ${document.surface} needs a UI token manifest.`);
    }
    return;
  }
  validateSurfaceNodeTokens(document.root, manifest);
}

function surfaceTokenManifest(
  surface: string,
  manifests: readonly UiSurfaceTokenManifest[],
): UiSurfaceTokenManifest | null {
  const normalized = normalizeUiSurface(surface);
  return manifests.find((manifest) => normalizeUiSurface(manifest.surface) === normalized) ?? null;
}

function builtInSurfaceTokenManifest(surface: string): UiSurfaceTokenManifest | null {
  const normalized = normalizeUiSurface(surface);
  if (!SURFACES.has(normalized)) return null;
  const tokens = UI_SURFACE_TOKENS_BY_SURFACE[normalized as UiSurfaceName];
  return { surface: normalized, ...tokens };
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

function joinHuman(values: readonly string[]): string {
  if (values.length <= 1) return values[0] ?? "";
  return `${values.slice(0, -1).join(", ")} or ${values.at(-1)}`;
}
