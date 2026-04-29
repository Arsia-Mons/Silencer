// Pure entry point — safe for browser bundles. Filesystem-backed
// `validateDirectory` lives at `@silencer/gas-validation/node` to
// keep `node:fs/promises` out of webpack's import graph for the
// admin client.
export { GAS_SCHEMAS } from "./schemas";
export { validateFile, validateAll, GAS_FILES } from "./validate";
export type { GASFileName } from "./validate";
export { checkReferences } from "./references";
export type { GASError, GASErrorCode, ValidationResult } from "./errors";
