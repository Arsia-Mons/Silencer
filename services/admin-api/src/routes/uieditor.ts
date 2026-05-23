/**
 * UI editor document endpoints.
 *
 * The filesystem is the source of truth. UI layout documents are stored as
 * JSON files in shared/assets/ui-layouts/ and committed to git. The admin UI
 * reads and writes them through this API so edits use the same mounted asset
 * root as other game data.
 *
 * GET /ui-editor/documents          — list layout documents      (admin only)
 * GET /ui-editor/documents/:surface — return one layout document (admin only)
 * PUT /ui-editor/documents/:surface — write one layout document  (admin only)
 */

import { createHash, randomUUID } from "crypto";
import { existsSync, readdirSync, readFileSync, renameSync, statSync, writeFileSync } from "fs";
import { join } from "path";
import { Router } from "express";
import { requireAuth, requireRole } from "../auth/jwt.js";
import { ASSETS_DIR } from "../config.js";

const UI_LAYOUT_SCHEMA_VERSION = 1;
const NODE_KINDS = new Set<UiNodeKind>([
  "screen",
  "panel",
  "stack",
  "row",
  "text",
  "button",
  "input",
  "spacer",
]);
const CONTAINER_KINDS = new Set<UiNodeKind>(["screen", "panel", "stack", "row"]);

const router = Router();

type UiNodeKind = "screen" | "panel" | "stack" | "row" | "text" | "button" | "input" | "spacer";
type UiSizeMode = "fit" | "grow" | "fixed";

interface UiSize {
  mode: UiSizeMode;
  value?: number;
}

interface UiStyle {
  width: UiSize;
  height: UiSize;
  direction?: "row" | "column";
  align?: "start" | "center" | "end";
  justify?: "start" | "center" | "end";
  padding?: number;
  gap?: number;
  backgroundPalette?: number;
  borderPalette?: number;
  textPalette?: number;
  font?: "ui" | "uiLarge" | "title" | "tiny";
  radius?: number;
}

interface UiNode {
  id: string;
  kind: UiNodeKind;
  name: string;
  text?: string;
  placeholder?: string;
  action?: string;
  style: UiStyle;
  children?: UiNode[];
}

interface UiDocument {
  schemaVersion: typeof UI_LAYOUT_SCHEMA_VERSION;
  surface: string;
  viewport: {
    width: number;
    height: number;
  };
  root: UiNode;
}

interface UiDocumentReference {
  surface: string;
  filename: string;
  title: string;
  updatedAt: string;
  revision: string;
}

interface UiDocumentPayload {
  document: UiDocument;
  expectedRevision?: string | null;
}

interface RouteRequest {
  params: Record<string, string>;
  body: unknown;
}

interface RouteResponse {
  json(value: unknown): void;
  status(code: number): RouteResponse;
}

class HttpError extends Error {
  status: number;

  constructor(message: string, status: number) {
    super(message);
    this.status = status;
  }
}

export function normalizeUiSurface(value: unknown): string {
  const normalized = String(value ?? "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/(^-|-$)/g, "");
  return normalized || "unnamed";
}

export function uiLayoutFilename(surface: string): string {
  return `${normalizeUiSurface(surface)}.silencer-ui.json`;
}

function requireLayoutsDir(): void {
  const dir = uiLayoutsDir();
  if (!existsSync(dir)) {
    throw new Error(`UI layout asset directory is missing: ${dir}`);
  }
}

function uiLayoutsDir(): string {
  return join(process.env.ASSETS_DIR || ASSETS_DIR, "ui-layouts");
}

function diskPath(surface: string): string {
  return join(uiLayoutsDir(), uiLayoutFilename(surface));
}

function revisionForText(text: string): string {
  return createHash("sha256").update(text).digest("hex");
}

function referenceForDocument(
  document: UiDocument,
  filename: string,
  info: { mtime: Date },
  revision: string,
): UiDocumentReference {
  return {
    surface: normalizeUiSurface(document.surface),
    filename,
    title: document.root.name,
    updatedAt: info.mtime.toISOString(),
    revision,
  };
}

export function validateUiDocument(value: unknown): UiDocument {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Document must be an object.");
  }
  const candidate = value as Partial<UiDocument>;
  if (candidate.schemaVersion !== UI_LAYOUT_SCHEMA_VERSION) {
    throw new Error(`Unsupported UI layout schema: ${String(candidate.schemaVersion)}`);
  }
  if (!candidate.surface || typeof candidate.surface !== "string")
    throw new Error("Surface name is missing.");
  if (!candidate.viewport || typeof candidate.viewport !== "object")
    throw new Error("Document viewport is missing.");
  validateViewport(candidate.viewport);
  if (!candidate.root || typeof candidate.root !== "object")
    throw new Error("Document root is missing.");
  if (candidate.root.kind !== "screen") throw new Error("Document root must be a screen node.");
  validateNode(candidate.root, new Set<string>());
  return { ...candidate, surface: normalizeUiSurface(candidate.surface) } as UiDocument;
}

export function listUiLayoutDocuments(): UiDocumentReference[] {
  requireLayoutsDir();
  const dir = uiLayoutsDir();
  return readdirSync(dir)
    .filter((filename) => filename.endsWith(".silencer-ui.json"))
    .sort((a, b) => a.localeCompare(b))
    .map((filename) => {
      const path = join(dir, filename);
      const raw = readFileSync(path, "utf8");
      const document = validateStoredDocument(filename, raw);
      return referenceForDocument(document, filename, statSync(path), revisionForText(raw));
    });
}

export function readUiLayoutDocument(surface: string): {
  document: UiDocument;
  reference: UiDocumentReference;
} {
  requireLayoutsDir();
  const normalizedSurface = normalizeUiSurface(surface);
  const filename = uiLayoutFilename(normalizedSurface);
  const path = diskPath(normalizedSurface);
  if (!existsSync(path)) throw new Error("Not found");
  const raw = readFileSync(path, "utf8");
  const document = validateStoredDocument(filename, raw);
  if (normalizeUiSurface(document.surface) !== normalizedSurface) {
    throw new Error(`Document ${filename} declares surface ${document.surface}.`);
  }
  return {
    document,
    reference: referenceForDocument(document, filename, statSync(path), revisionForText(raw)),
  };
}

export function writeUiLayoutDocument(
  surface: string,
  body: unknown,
): {
  document: UiDocument;
  reference: UiDocumentReference;
} {
  requireLayoutsDir();
  const normalizedSurface = normalizeUiSurface(surface);
  const payload = body as Partial<UiDocumentPayload> | null | undefined;
  const document = validateUiDocument(payload?.document);
  if (document.surface !== normalizedSurface) {
    throw new HttpError(
      `Route surface ${normalizedSurface} does not match document surface ${document.surface}.`,
      400,
    );
  }

  const path = diskPath(normalizedSurface);
  const expectedRevision = payload?.expectedRevision;
  if (existsSync(path)) {
    const currentRevision = revisionForText(readFileSync(path, "utf8"));
    if (typeof expectedRevision !== "string" || expectedRevision !== currentRevision) {
      throw new HttpError("UI layout document changed before save.", 409);
    }
  } else if (expectedRevision !== null) {
    throw new HttpError("New UI layout saves must use expectedRevision: null.", 409);
  }

  const text = `${JSON.stringify(document, null, 2)}\n`;
  const tempPath = `${path}.${process.pid}.${randomUUID()}.tmp`;
  writeFileSync(tempPath, text, "utf8");
  renameSync(tempPath, path);
  return readUiLayoutDocument(normalizedSurface);
}

function validateStoredDocument(filename: string, raw: string): UiDocument {
  try {
    const document = validateUiDocument(JSON.parse(raw));
    const expectedFilename = uiLayoutFilename(document.surface);
    if (filename !== expectedFilename) throw new Error(`filename must be ${expectedFilename}`);
    return document;
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    throw new Error(`Invalid UI layout document ${filename}: ${message}`);
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

function validateNode(node: UiNode, seenIds: Set<string>): void {
  if (!node.id || typeof node.id !== "string") throw new Error("Node id is missing.");
  if (seenIds.has(node.id)) throw new Error(`Duplicate node id: ${node.id}`);
  seenIds.add(node.id);
  if (!NODE_KINDS.has(node.kind)) throw new Error(`Unsupported node kind: ${String(node.kind)}`);
  if (!node.name || typeof node.name !== "string")
    throw new Error(`Node ${node.id} name is missing.`);
  validateOptionalString(node, "text");
  validateOptionalString(node, "placeholder");
  validateOptionalString(node, "action");
  if (!node.style || typeof node.style !== "object" || Array.isArray(node.style)) {
    throw new Error(`Node ${node.id} style is missing.`);
  }
  validateStyleFields(node);
  validateSize(node, "width");
  validateSize(node, "height");
  if (node.kind === "button" && node.style.height.mode !== "fit") {
    throw new Error(`Node ${node.id} button height must be fit.`);
  }
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
  if ((node.children?.length ?? 0) > 0 && !CONTAINER_KINDS.has(node.kind)) {
    throw new Error(`Node ${node.id} cannot have children.`);
  }
  for (const child of node.children ?? []) validateNode(child, seenIds);
}

function validateOptionalString(node: UiNode, key: "text" | "placeholder" | "action"): void {
  const value = node[key];
  if (value !== undefined && typeof value !== "string")
    throw new Error(`Node ${node.id} has invalid ${key}.`);
}

function validateStyleFields(node: UiNode): void {
  const allowed = allowedStyleFields(node.kind);
  for (const key of Object.keys(node.style)) {
    if (!allowed.has(key)) throw new Error(`Node ${node.id} has unsupported ${key} style.`);
  }
}

function allowedStyleFields(kind: UiNodeKind): Set<keyof UiStyle> {
  if (CONTAINER_KINDS.has(kind)) {
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

function validateNumber(
  node: UiNode,
  key: "padding" | "gap" | "radius",
  min: number,
  max: number,
): void {
  const value = node.style[key];
  if (value === undefined) return;
  if (!Number.isInteger(value) || value < min || value > max)
    throw new Error(`Node ${node.id} has invalid ${key}.`);
}

function validateEnum<T extends string>(
  node: UiNode,
  key: "direction" | "align" | "justify" | "font",
  values: T[],
): void {
  const value = node.style[key];
  if (value !== undefined && (typeof value !== "string" || !values.includes(value))) {
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
  if (!Number.isInteger(value) || value < min || value > 255)
    throw new Error(`Node ${node.id} has invalid ${key}.`);
}

function validateSize(node: UiNode, key: "width" | "height"): void {
  const size = node.style[key];
  if (!size || !["fit", "grow", "fixed"].includes(size.mode)) {
    throw new Error(`Node ${node.id} has invalid ${key} sizing.`);
  }
  if (size.mode === "fixed") {
    if (typeof size.value !== "number")
      throw new Error(`Node ${node.id} fixed ${key} sizing needs a value.`);
    if (!Number.isFinite(size.value) || size.value < 0 || size.value > 4096) {
      throw new Error(`Node ${node.id} has invalid fixed ${key} sizing.`);
    }
  }
}

router.get("/", requireAuth, requireRole("admin"), (_req: RouteRequest, res: RouteResponse) => {
  try {
    res.json({ ok: true, documents: listUiLayoutDocuments() });
  } catch (err) {
    res.status(500).json({ ok: false, error: errorMessage(err) });
  }
});

router.get(
  "/:surface",
  requireAuth,
  requireRole("admin"),
  (req: RouteRequest, res: RouteResponse) => {
    try {
      res.json({ ok: true, ...readUiLayoutDocument(req.params.surface) });
    } catch (err) {
      const message = errorMessage(err);
      res.status(message === "Not found" ? 404 : 500).json({ ok: false, error: message });
    }
  },
);

router.put(
  "/:surface",
  requireAuth,
  requireRole("admin"),
  (req: RouteRequest, res: RouteResponse) => {
    try {
      res.json({ ok: true, ...writeUiLayoutDocument(req.params.surface, req.body) });
    } catch (err) {
      res
        .status(err instanceof HttpError ? err.status : 400)
        .json({ ok: false, error: errorMessage(err) });
    }
  },
);

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export default router;
