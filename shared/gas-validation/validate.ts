// Validation entry points. `validateAll()` operates on already-parsed
// JSON (used by the admin web UI, in-browser); `validateDirectory()`
// reads from disk via fs/promises (used by the silencer-cli).

import Ajv, { type ErrorObject } from "ajv";
import { GAS_SCHEMAS } from "./schemas";
import { checkReferences } from "./references";
import type { GASError, ValidationResult } from "./errors";

// File names the validator knows about. Matches the keys of GAS_SCHEMAS
// (extension appended) and the C++ loader's hardcoded paths.
export const GAS_FILES = [
  "agencies.json",
  "abilities.json",
  "enemies.json",
  "gameobjects.json",
  "items.json",
  "player.json",
  "weapons.json",
] as const;
export type GASFileName = (typeof GAS_FILES)[number];

// Map filename → schema. The schemas are keyed by basename in
// GAS_SCHEMAS; reproject to filename here so the ".json" suffix is
// the user-facing key everywhere downstream.
function schemaFor(file: string): object | undefined {
  const basename = file.replace(/\.json$/, "");
  return GAS_SCHEMAS[basename]?.schema;
}

function ajvErrorToGAS(file: string, err: ErrorObject): GASError {
  // Pin the offending property name into instancePath when ajv reports
  // it via params (additionalProperties / required), so the agent has
  // an unambiguous JSON Pointer to feed back into Edit. Without this,
  // both classes of error land at the parent object's path with no
  // hint of which key was wrong.
  let path = err.instancePath || "";
  let label = err.message ?? "invalid";
  const params = err.params as Record<string, unknown> | undefined;
  if (err.keyword === "additionalProperties" && typeof params?.additionalProperty === "string") {
    path = `${path}/${params.additionalProperty}`;
    label = `unknown property "${params.additionalProperty}"`;
  } else if (err.keyword === "required" && typeof params?.missingProperty === "string") {
    path = `${path}/${params.missingProperty}`;
    label = `missing required property "${params.missingProperty}"`;
  }
  return {
    file,
    instancePath: path,
    code: "SCHEMA_ERROR",
    message: `${path || "/"} ${label}`,
  };
}

let cachedAjv: Ajv | null = null;
function getAjv(): Ajv {
  if (!cachedAjv) {
    cachedAjv = new Ajv({ allErrors: true, strict: false });
  }
  return cachedAjv;
}

// Validate a single parsed file against its schema. Returns a flat
// array of GASErrors (empty when clean). Unknown filename → empty
// (no schema to compare against; caller decides whether that's fatal).
export function validateFile(file: string, parsed: unknown): GASError[] {
  const schema = schemaFor(file);
  if (!schema) return [];
  const ajv = getAjv();
  const validate = ajv.compile(schema);
  const ok = validate(parsed);
  if (ok) return [];
  return (validate.errors ?? []).map((e) => ajvErrorToGAS(file, e));
}

// Validate a complete bundle of parsed GAS files. Runs schema checks
// on every file the caller supplied, then a referential pass over the
// whole bundle. Caller is responsible for parsing JSON; PARSE_ERRORs
// are emitted by validateDirectory() / the C++ loader.
export function validateAll(
  files: Record<string, unknown>,
): ValidationResult {
  const errors: GASError[] = [];
  for (const [file, parsed] of Object.entries(files)) {
    errors.push(...validateFile(file, parsed));
  }
  errors.push(
    ...checkReferences(files as Record<string, Record<string, unknown>>),
  );
  return { ok: errors.length === 0, errors };
}

// Disk-backed entry point. Reads each known GAS file from `dir`,
// parses it, and routes failures into the same GASError pipeline.
// Missing files surface as OPEN_FAILED; bad JSON as PARSE_ERROR.
export async function validateDirectory(
  dir: string,
): Promise<ValidationResult> {
  const { readFile } = await import("node:fs/promises");
  const path        = await import("node:path");

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
