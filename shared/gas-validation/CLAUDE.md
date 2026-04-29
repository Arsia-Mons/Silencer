# shared/gas-validation — GAS schemas + validator

Bun + TS workspace package consumed by:
- `web/admin/` — Monaco hover docs/inline checks (`GAS_SCHEMAS`) and
  the `/gas` page's "Validate all" pass (`validateAll`).
- `clients/cli/` — `silencer-cli gas validate` runs `validateDirectory`
  against a local `shared/assets/gas/` checkout.
- `clients/silencer/` (C++ loader, indirectly) — emits errors over the
  control socket using the same `GASError` shape declared here.

## Public surface

```ts
import {
  GAS_SCHEMAS,        // Record<basename, { uri, schema }> — Monaco-shaped
  GAS_FILES,          // readonly tuple of every known filename
  validateFile,       // (file, parsedJson) → GASError[]
  validateAll,        // (Record<file, parsedJson>) → ValidationResult
  validateDirectory,  // (dir) → Promise<ValidationResult>  // fs-backed
  checkReferences,    // (parsedFiles) → GASError[]         // cross-file
} from "@silencer/gas-validation";

import type { GASError, GASErrorCode, ValidationResult } from "@silencer/gas-validation";
```

## Error shape

Every error — schema, reference, parse, file-open — lands in:

```ts
{ file: "weapons.json", instancePath: "/weapons/3/fireDelay",
  code: "SCHEMA_ERROR", message: "must be integer" }
```

`instancePath` is RFC 6901 JSON Pointer, suitable for round-tripping
into an `Edit` call against the source file. `code` is one of
`OPEN_FAILED | PARSE_ERROR | SCHEMA_ERROR | REFERENCE_ERROR`.

The C++ loader (`clients/silencer/src/gas/gasloader.cpp`) emits the
same shape over the control socket so the agent's remediation loop
doesn't care which side caught the problem.

## Schema authorship

`schemas.ts` mirrors the field set in
`clients/silencer/src/gas/gasloader.h`. When you add a field to a
`*Def` struct in C++, add it here too — same name, matching type,
description copied from the C++ comment. Drift is caught by the
admin UI (Monaco flags unknown properties) but only after a deploy,
so prefer keeping them in lockstep at edit time.

## Adding a referential check

Cross-file rules go in `references.ts`. The current pass enforces ID
uniqueness within each collection; future checks (e.g. items.json
referencing nonexistent agency ids) hang off the same `checkReferences`
function and reuse the `GASError` shape.

## Build / run

```bash
bun install        # at repo root — workspaces resolve this package
bun run typecheck  # from this dir
```

Nothing to compile; consumers import the `.ts` directly via the
package's `exports` map.
