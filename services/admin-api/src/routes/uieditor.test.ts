import { mkdir, mkdtemp, readFile, rm, writeFile } from "fs/promises";
import { tmpdir } from "os";
import { join } from "path";
import { describe, expect, test } from "bun:test";
import { listUiLayoutDocuments, readUiLayoutDocument, writeUiLayoutDocument } from "./uieditor.ts";

describe("ui editor document store", () => {
  test("lists, reads, and writes validated layout documents with revisions", async () => {
    await withTempAssets(async (assetsDir: string) => {
      const document = createDocument("main-menu");
      await writeFile(
        join(assetsDir, "ui-layouts", "main-menu.silencer-ui.json"),
        `${JSON.stringify(document, null, 2)}\n`,
        "utf8",
      );

      const listed = listUiLayoutDocuments();
      const loaded = readUiLayoutDocument("main-menu");
      const saved = writeUiLayoutDocument("main-menu", {
        document: {
          ...loaded.document,
          root: { ...loaded.document.root, name: "Main Menu Edited" },
        },
        expectedRevision: loaded.reference.revision,
      });

      expect(listed.length).toBe(1);
      expect(listed[0].surface).toBe("main-menu");
      expect(loaded.reference.revision.length).toBe(64);
      expect(saved.reference.title).toBe("Main Menu Edited");
      expect(saved.reference.revision).not.toBe(loaded.reference.revision);
    });
  });

  test("rejects stale saves without overwriting the stored document", async () => {
    await withTempAssets(async (assetsDir: string) => {
      const path = join(assetsDir, "ui-layouts", "main-menu.silencer-ui.json");
      const document = createDocument("main-menu");
      await writeFile(path, `${JSON.stringify(document, null, 2)}\n`, "utf8");
      const loaded = readUiLayoutDocument("main-menu");
      writeUiLayoutDocument("main-menu", {
        document: { ...loaded.document, root: { ...loaded.document.root, name: "First Edit" } },
        expectedRevision: loaded.reference.revision,
      });

      let message = "";
      try {
        writeUiLayoutDocument("main-menu", {
          document: { ...loaded.document, root: { ...loaded.document.root, name: "Stale Edit" } },
          expectedRevision: loaded.reference.revision,
        });
      } catch (error) {
        message = error instanceof Error ? error.message : String(error);
      }

      expect(message).toContain("changed before save");
      expect(await readFile(path, "utf8")).toContain("First Edit");
    });
  });

  test("fails when the authoritative layout asset directory is missing", async () => {
    await withTempAssets(async (assetsDir: string) => {
      await rm(join(assetsDir, "ui-layouts"), { recursive: true, force: true });

      let message = "";
      try {
        listUiLayoutDocuments();
      } catch (error) {
        message = error instanceof Error ? error.message : String(error);
      }

      expect(message).toContain("UI layout asset directory is missing");
    });
  });
});

async function withTempAssets(run: (assetsDir: string) => Promise<void>): Promise<void> {
  const previousDir = process.env.ASSETS_DIR;
  const assetsDir = await mkdtemp(join(tmpdir(), "silencer-admin-api-assets-"));
  await mkdir(join(assetsDir, "ui-layouts"), { recursive: true });
  process.env.ASSETS_DIR = assetsDir;
  try {
    await run(assetsDir);
  } finally {
    if (previousDir === undefined) {
      delete process.env.ASSETS_DIR;
    } else {
      process.env.ASSETS_DIR = previousDir;
    }
    await rm(assetsDir, { recursive: true, force: true });
  }
}

function createDocument(surface: string) {
  return {
    schemaVersion: 1,
    surface,
    viewport: { width: 1280, height: 720 },
    root: {
      id: `${surface}-root`,
      kind: "screen",
      name: "Main Menu",
      style: {
        width: { mode: "fixed", value: 1280 },
        height: { mode: "fixed", value: 720 },
        direction: "column",
        align: "center",
        justify: "center",
      },
      children: [],
    },
  };
}
