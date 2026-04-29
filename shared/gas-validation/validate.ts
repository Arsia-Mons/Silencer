// Pure schema validation entry point — safe to import from any
// runtime, including browser bundles. The fs-backed
// `validateDirectory()` lives in `./validate-fs.ts` (exported as
// `@silencer/gas-validation/node`) so webpack/Next don't try to
// resolve `node:fs/promises` for the admin client bundle.

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
