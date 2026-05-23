import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describe, expect, test } from "bun:test";
import { createDefaultUiDocument } from "./ui-layout";
import {
  listUiLayoutDocuments,
  readUiLayoutDocument,
  writeUiLayoutDocument,
} from "./ui-layout-store";

describe("ui-layout-store", () => {
  test("writes validated dashboard layout documents to the shared layout directory", async () => {
    await withTempLayoutStore(async (tempDir) => {
      const document = {
        ...createDefaultUiDocument(),
        surface: "Main Menu",
      };
      const saved = await writeUiLayoutDocument(document);
      const savedText = await readFile(join(tempDir, "main-menu.silencer-ui.json"), "utf8");
      const listed = await listUiLayoutDocuments();
      const loaded = await readUiLayoutDocument("main-menu");

      expect(saved.document.surface).toBe("main-menu");
      expect(saved.reference.filename).toBe("main-menu.silencer-ui.json");
      expect(savedText).toContain('"surface": "main-menu"');
      expect(listed.map((candidate) => candidate.surface).join(",")).toBe("main-menu");
      expect(loaded.root.id).toBe("main-menu-root");
    });
  });

  test("fails the document list when a stored layout no longer validates", async () => {
    await withTempLayoutStore(async (tempDir) => {
      await writeFile(join(tempDir, "broken.silencer-ui.json"), '{"schemaVersion":1}', "utf8");
      let message = "";

      try {
        await listUiLayoutDocuments();
      } catch (error) {
        message = error instanceof Error ? error.message : String(error);
      }

      expect(message).toContain("Invalid UI layout document broken.silencer-ui.json");
    });
  });
});

async function withTempLayoutStore(run: (tempDir: string) => Promise<void>): Promise<void> {
  const previousDir = process.env.SILENCER_UI_LAYOUTS_DIR;
  const tempDir = await mkdtemp(join(tmpdir(), "silencer-ui-layout-store-"));
  process.env.SILENCER_UI_LAYOUTS_DIR = tempDir;
  try {
    await run(tempDir);
  } finally {
    if (previousDir === undefined) {
      delete process.env.SILENCER_UI_LAYOUTS_DIR;
    } else {
      process.env.SILENCER_UI_LAYOUTS_DIR = previousDir;
    }
    await rm(tempDir, { recursive: true, force: true });
  }
}
