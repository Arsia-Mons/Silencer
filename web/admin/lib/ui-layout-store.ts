import { mkdir, readdir, readFile, stat, writeFile } from "node:fs/promises";
import { join, resolve, sep } from "node:path";
import {
  normalizeUiSurface,
  uiLayoutFilename,
  validateUiDocument,
  type UiDocument,
  type UiDocumentReference,
} from "./ui-layout";

const DEFAULT_UI_LAYOUTS_DIR = process.cwd().endsWith(`${sep}web${sep}admin`)
  ? resolve(process.cwd(), "../../shared/assets/ui-layouts")
  : resolve(process.cwd(), "shared/assets/ui-layouts");

function uiLayoutsDir(): string {
  return process.env.SILENCER_UI_LAYOUTS_DIR ?? DEFAULT_UI_LAYOUTS_DIR;
}

function uiLayoutPath(surface: string): string {
  return join(uiLayoutsDir(), uiLayoutFilename(surface));
}

function referenceForDocument(
  document: UiDocument,
  filename: string,
  updatedAt: string,
): UiDocumentReference {
  return {
    surface: normalizeUiSurface(document.surface),
    filename,
    title: document.root.name,
    updatedAt,
  };
}

export async function listUiLayoutDocuments(): Promise<UiDocumentReference[]> {
  const dir = uiLayoutsDir();
  await mkdir(dir, { recursive: true });
  const filenames = (await readdir(dir))
    .filter((filename) => filename.endsWith(".silencer-ui.json"))
    .sort((a, b) => a.localeCompare(b));

  const documents: UiDocumentReference[] = [];
  for (const filename of filenames) {
    const path = join(dir, filename);
    const [raw, info] = await Promise.all([readFile(path, "utf8"), stat(path)]);
    try {
      const document = validateUiDocument(JSON.parse(raw));
      const expectedFilename = uiLayoutFilename(document.surface);
      if (filename !== expectedFilename) {
        throw new Error(`filename must be ${expectedFilename}`);
      }
      documents.push(referenceForDocument(document, filename, info.mtime.toISOString()));
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      throw new Error(`Invalid UI layout document ${filename}: ${message}`);
    }
  }
  return documents;
}

export async function readUiLayoutDocument(surface: string): Promise<UiDocument> {
  const normalizedSurface = normalizeUiSurface(surface);
  const filename = uiLayoutFilename(normalizedSurface);
  const raw = await readFile(uiLayoutPath(normalizedSurface), "utf8");
  const document = validateUiDocument(JSON.parse(raw));
  if (normalizeUiSurface(document.surface) !== normalizedSurface) {
    throw new Error(`Document ${filename} declares surface ${document.surface}.`);
  }
  return document;
}

export async function writeUiLayoutDocument(document: UiDocument): Promise<{
  document: UiDocument;
  reference: UiDocumentReference;
}> {
  const normalized = validateUiDocument({
    ...document,
    surface: normalizeUiSurface(document.surface),
  });
  const dir = uiLayoutsDir();
  await mkdir(dir, { recursive: true });
  const filename = uiLayoutFilename(normalized.surface);
  const path = join(dir, filename);
  await writeFile(path, `${JSON.stringify(normalized, null, 2)}\n`, "utf8");
  const info = await stat(path);
  return {
    document: normalized,
    reference: referenceForDocument(normalized, filename, info.mtime.toISOString()),
  };
}
