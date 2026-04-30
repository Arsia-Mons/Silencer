// Shared error shape emitted by every GAS validation/load source.
//
// Wherever an error originates — schema check (this package), reference
// check (this package), C++ loader parse (`gasloader.cpp` over the
// control socket) — it surfaces in the exact same shape so an agent's
// remediation loop is platform-agnostic.

export type GASErrorCode =
  | "OPEN_FAILED"      // file missing or unreadable (C++ loader)
  | "PARSE_ERROR"      // JSON syntax error (C++ loader or this package)
  | "SCHEMA_ERROR"     // failed JSON Schema validation (this package)
  | "REFERENCE_ERROR"  // cross-file referential integrity (this package)
  | "FIELD_ERROR";     // C++ loader field-walk threw (e.g. type mismatch in nlohmann::json::value)

export interface GASError {
  /** filename relative to the gas dir, e.g. "weapons.json" */
  file: string;
  /**
   * JSON Pointer (RFC 6901) into the file's JSON, e.g. "/weapons/3/fireDelay".
   * Empty string for whole-file errors (open/parse failures).
   */
  instancePath: string;
  code: GASErrorCode;
  message: string;
}

export interface ValidationResult {
  ok: boolean;
  errors: GASError[];
}
