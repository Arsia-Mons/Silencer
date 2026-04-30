// Cross-file and within-file referential checks that JSON Schema
// can't express. Today this is just ID uniqueness inside each
// collection. Add new checks here as the data model grows
// references between files.

import type { GASError } from "./errors";

type AnyEntry = Record<string, unknown>;
type AnyFile  = Record<string, unknown>;

interface CollectionSpec {
  file: string;        // e.g. "weapons.json"
  arrayKey: string;    // e.g. "weapons" — the array property inside the parsed JSON
  idKey: string;       // e.g. "id" — entry property used as the unique identifier
}

const COLLECTIONS: CollectionSpec[] = [
  { file: "weapons.json",     arrayKey: "weapons",   idKey: "id" },
  { file: "items.json",       arrayKey: "items",     idKey: "id" },
  { file: "enemies.json",     arrayKey: "enemies",   idKey: "id" },
  { file: "agencies.json",    arrayKey: "agencies",  idKey: "id" },
  { file: "abilities.json",   arrayKey: "abilities", idKey: "id" },
  { file: "gameobjects.json", arrayKey: "gameObjects", idKey: "id" },
  { file: "gameobjects.json", arrayKey: "terminals", idKey: "id" },
];

export function checkReferences(files: Record<string, AnyFile>): GASError[] {
  const errors: GASError[] = [];

  for (const spec of COLLECTIONS) {
    const parsed = files[spec.file];
    if (!parsed) continue; // missing file — schema layer reports it
    const arr = parsed[spec.arrayKey];
    if (!Array.isArray(arr)) continue; // schema layer reports shape errors

    const seen = new Map<string | number, number>();
    for (let i = 0; i < arr.length; i++) {
      const entry = arr[i] as AnyEntry;
      const id = entry?.[spec.idKey] as string | number | undefined;
      if (id === undefined) continue; // schema layer flags missing required id
      if (seen.has(id)) {
        const firstIdx = seen.get(id)!;
        errors.push({
          file: spec.file,
          instancePath: `/${spec.arrayKey}/${i}/${spec.idKey}`,
          code: "REFERENCE_ERROR",
          message: `duplicate ${spec.idKey} "${id}" — first defined at /${spec.arrayKey}/${firstIdx}`,
        });
      } else {
        seen.set(id, i);
      }
    }
  }

  return errors;
}
