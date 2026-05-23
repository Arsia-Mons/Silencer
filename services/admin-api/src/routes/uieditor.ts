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
import {
  normalizeUiSurface,
  uiLayoutFilename,
  validateUiDocument,
  type UiDocument,
  type UiDocumentReference,
} from "@silencer/ui-layout";
import { requireAuth, requireRole } from "../auth/jwt.js";
import { ASSETS_DIR } from "../config.js";

const router = Router();

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
