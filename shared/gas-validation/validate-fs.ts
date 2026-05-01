// Filesystem-backed validation. Imports `node:fs/promises` and
// `node:path` synchronously, so this module is node-only — exported
// at `@silencer/gas-validation/node`, never from the package root.
// Browser-targeting bundlers (Next/webpack on the admin web) would
// otherwise try to resolve `node:` schemes and fail, so the admin
// app imports only from `@silencer/gas-validation` (the pure entry).

import { readFile } from "node:fs/promises";
import * as path from "node:path";
import { GAS_FILES, validateAll } from "./validate";
import type { GASError, ValidationResult } from "./errors";

// Disk-backed entry point. Reads each known GAS file from `dir`,
// parses it, and routes failures into the same GASError pipeline.
// Missing files surface as OPEN_FAILED; bad JSON as PARSE_ERROR.
export async function validateDirectory(
  dir: string,
): Promise<ValidationResult> {
  const errors: GASError[] = [];
  const parsed: Record<string, unknown> = {};

  for (const file of GAS_FILES) {
    const fp = path.join(dir, file);
    let raw: string;
    try {
      raw = await readFile(fp, "utf8");
    } catch (e) {
      errors.push({
        file,
        instancePath: "",
        code: "OPEN_FAILED",
        message: e instanceof Error ? e.message : String(e),
      });
      continue;
    }
    try {
      parsed[file] = JSON.parse(raw);
    } catch (e) {
      errors.push({
        file,
        instancePath: "",
        code: "PARSE_ERROR",
        message: e instanceof Error ? e.message : String(e),
      });
      continue;
    }
  }

  const structural = validateAll(parsed);
  errors.push(...structural.errors);
  return { ok: errors.length === 0, errors };
}
