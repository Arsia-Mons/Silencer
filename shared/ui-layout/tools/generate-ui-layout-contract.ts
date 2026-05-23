import { existsSync, readFileSync, writeFileSync } from "fs";
import { join } from "path";

import {
  UI_ALIGNS,
  UI_ATTACH_POINTS,
  UI_ATTACH_TO_VALUES,
  UI_AXES,
  UI_BUTTON_SIZES,
  UI_BUTTON_VARIANTS,
  UI_CONTAINER_NODE_KINDS,
  UI_DOCUMENT_FIELDS,
  UI_FONTS,
  UI_FLOATING_FIELDS,
  UI_FORBIDDEN_NODE_DECORATORS_BY_KIND,
  UI_IMAGE_FIELDS,
  UI_IMAGE_MODES,
  UI_JUSTIFIES,
  UI_LAYOUT_SCHEMA_VERSION,
  UI_NODE_FIELDS,
  UI_NODE_KINDS,
  UI_NODE_TOKEN_FIELDS_BY_KIND,
  UI_NUMERIC_LIMITS,
  UI_REQUIRED_TOKEN_FIELDS_BY_KIND,
  UI_SIZE_FIELDS,
  UI_SIZE_MODES,
  UI_SIZE_RULES_BY_KIND,
  UI_STYLE_FIELDS_BY_KIND,
  UI_SURFACES,
  UI_SURFACE_TOKENS_BY_SURFACE,
  UI_VIEWPORT_FIELDS,
} from "../contract";

const HEADER_PATH = join(
  import.meta.dir,
  "../../../clients/silencer/src/net/ui_layout_contract.generated.h",
);
const TOKEN_MANIFEST_DIR = join(import.meta.dir, "../../assets/ui-layouts");

function cppString(value: string): string {
  return `"${value.replaceAll("\\", "\\\\").replaceAll('"', '\\"')}"`;
}

function cppArray(name: string, values: readonly string[]): string {
  const items =
    values.length > 0 ? values.map((value) => `\t${cppString(value)},`).join("\n") : "\tnullptr,";
  return `constexpr const char * ${name}[] = {\n${items}\n};\nconstexpr std::size_t ${name}Count = ${values.length};`;
}

function pascalCase(value: string): string {
  return value
    .split(/[^a-zA-Z0-9]+/g)
    .filter(Boolean)
    .map((part) => `${part.charAt(0).toUpperCase()}${part.slice(1)}`)
    .join("");
}

function cppNamedConstants(prefix: string, values: readonly string[]): string {
  return values
    .map((value) => `constexpr const char * ${prefix}${pascalCase(value)} = ${cppString(value)};`)
    .join("\n");
}

function unique(values: readonly string[]): string[] {
  return Array.from(new Set(values));
}

function allSurfaceTokens(key: "components" | "textBindings" | "actions"): string[] {
  return unique(UI_SURFACES.flatMap((surface) => [...UI_SURFACE_TOKENS_BY_SURFACE[surface][key]]));
}

function numericLimits(): string {
  return `constexpr int kSchemaVersion = ${UI_LAYOUT_SCHEMA_VERSION};
constexpr int kMinViewport = ${UI_NUMERIC_LIMITS.viewportMin};
constexpr int kMaxViewport = ${UI_NUMERIC_LIMITS.viewportMax};
constexpr float kMinSize = ${UI_NUMERIC_LIMITS.sizeMin}.0f;
constexpr float kMaxSize = ${UI_NUMERIC_LIMITS.sizeMax}.0f;
constexpr int kMinPadding = ${UI_NUMERIC_LIMITS.paddingMin};
constexpr int kMaxPadding = ${UI_NUMERIC_LIMITS.paddingMax};
constexpr int kMinGap = ${UI_NUMERIC_LIMITS.gapMin};
constexpr int kMaxGap = ${UI_NUMERIC_LIMITS.gapMax};
constexpr int kMinRadius = ${UI_NUMERIC_LIMITS.radiusMin};
constexpr int kMaxRadius = ${UI_NUMERIC_LIMITS.radiusMax};
constexpr int kMinPalette = ${UI_NUMERIC_LIMITS.paletteMin};
constexpr int kMinTextPalette = ${UI_NUMERIC_LIMITS.paletteTextMin};
constexpr int kMaxPalette = ${UI_NUMERIC_LIMITS.paletteMax};
constexpr int kMinImageBank = ${UI_NUMERIC_LIMITS.imageBankMin};
constexpr int kMaxImageBank = ${UI_NUMERIC_LIMITS.imageBankMax};
constexpr int kMinImageIndex = ${UI_NUMERIC_LIMITS.imageIndexMin};
constexpr int kMaxImageIndex = ${UI_NUMERIC_LIMITS.imageIndexMax};
constexpr float kMinFloatingOffset = ${UI_NUMERIC_LIMITS.floatingOffsetMin}.0f;
constexpr float kMaxFloatingOffset = ${UI_NUMERIC_LIMITS.floatingOffsetMax}.0f;
constexpr int kMinFloatingZIndex = ${UI_NUMERIC_LIMITS.floatingZIndexMin};
constexpr int kMaxFloatingZIndex = ${UI_NUMERIC_LIMITS.floatingZIndexMax};`;
}

function styleFieldArrays(): string {
  const arrays = UI_NODE_KINDS.map((kind) =>
    cppArray(`k${pascalCase(kind)}StyleFields`, UI_STYLE_FIELDS_BY_KIND[kind]),
  ).join("\n\n");
  const rows = UI_NODE_KINDS.map((kind) => {
    const name = `k${pascalCase(kind)}StyleFields`;
    return `\t{ ${cppString(kind)}, ${name}, ${name}Count },`;
  }).join("\n");
  return `${arrays}

struct StyleFieldsForKind {
\tconst char * kind;
\tconst char * const * fields;
\tstd::size_t fieldCount;
};

constexpr StyleFieldsForKind kStyleFieldsByKind[] = {
${rows}
};
constexpr std::size_t kStyleFieldsByKindCount = ${UI_NODE_KINDS.length};`;
}

function kindStringListTable(
  tableName: string,
  arrayPrefix: string,
  source: Record<string, readonly string[]>,
): string {
  const arrays = UI_NODE_KINDS.map((kind) =>
    cppArray(`${arrayPrefix}${pascalCase(kind)}`, source[kind] ?? []),
  ).join("\n\n");
  const rows = UI_NODE_KINDS.map((kind) => {
    const name = `${arrayPrefix}${pascalCase(kind)}`;
    return `\t{ ${cppString(kind)}, ${name}, ${name}Count },`;
  }).join("\n");
  return `${arrays}

constexpr StringListForKind ${tableName}[] = {
${rows}
};
constexpr std::size_t ${tableName}Count = ${UI_NODE_KINDS.length};`;
}

function sizeRuleArrays(): string {
  const rows: string[] = [];
  const arrays: string[] = [];
  for (const [kind, rules] of Object.entries(UI_SIZE_RULES_BY_KIND)) {
    for (const [axis, rule] of Object.entries(rules)) {
      const name = `k${pascalCase(kind)}${pascalCase(axis)}AllowedSizeModes`;
      arrays.push(cppArray(name, rule.modes));
      rows.push(
        `\t{ ${cppString(kind)}, ${cppString(axis)}, ${name}, ${name}Count, ${
          rule.allowBounds ? "true" : "false"
        } },`,
      );
    }
  }
  return `${arrays.join("\n\n")}

struct SizeRuleForKind {
\tconst char * kind;
\tconst char * axis;
\tconst char * const * modes;
\tstd::size_t modeCount;
\tbool allowBounds;
};

constexpr SizeRuleForKind kSizeRulesByKind[] = {
${rows.join("\n")}
};
constexpr std::size_t kSizeRulesByKindCount = ${rows.length};`;
}

function surfaceTokenArrays(): string {
  const arrays: string[] = [];
  const rows: string[] = [];
  for (const surface of UI_SURFACES) {
    const prefix = `k${pascalCase(surface)}`;
    const tokens = UI_SURFACE_TOKENS_BY_SURFACE[surface];
    arrays.push(cppArray(`${prefix}Components`, tokens.components));
    arrays.push(cppArray(`${prefix}TextBindings`, tokens.textBindings));
    arrays.push(cppArray(`${prefix}Actions`, tokens.actions));
    rows.push(
      `\t{ kUiSurface${pascalCase(surface)}, ${prefix}Components, ${prefix}ComponentsCount, ${prefix}TextBindings, ${prefix}TextBindingsCount, ${prefix}Actions, ${prefix}ActionsCount },`,
    );
  }
  return `${arrays.join("\n\n")}

struct SurfaceTokens {
\tconst char * surface;
\tconst char * const * components;
\tstd::size_t componentCount;
\tconst char * const * textBindings;
\tstd::size_t textBindingCount;
\tconst char * const * actions;
\tstd::size_t actionCount;
};

constexpr SurfaceTokens kSurfaceTokens[] = {
${rows.join("\n")}
};
constexpr std::size_t kSurfaceTokensCount = ${UI_SURFACES.length};`;
}

function generateHeader(): string {
  return `// Generated by shared/ui-layout/tools/generate-ui-layout-contract.ts.
// Do not edit by hand.

#ifndef SILENCER_NET_UI_LAYOUT_CONTRACT_GENERATED_H
#define SILENCER_NET_UI_LAYOUT_CONTRACT_GENERATED_H

#include <cstddef>

namespace silencer {
namespace net {
namespace ui_layout_contract {

${numericLimits()}

${cppArray("kNodeKinds", UI_NODE_KINDS)}
${cppNamedConstants("kNodeKind", UI_NODE_KINDS)}

${cppArray("kContainerNodeKinds", UI_CONTAINER_NODE_KINDS)}
${cppNamedConstants("kContainerNodeKind", UI_CONTAINER_NODE_KINDS)}

${cppArray("kDocumentFields", UI_DOCUMENT_FIELDS)}

${cppArray("kViewportFields", UI_VIEWPORT_FIELDS)}

${cppArray("kNodeFields", UI_NODE_FIELDS)}

${cppArray("kSizeFields", UI_SIZE_FIELDS)}

${cppArray("kImageFields", UI_IMAGE_FIELDS)}

${cppArray("kFloatingFields", UI_FLOATING_FIELDS)}

${cppArray("kAxes", UI_AXES)}
${cppNamedConstants("kAxis", UI_AXES)}

${cppArray("kAligns", UI_ALIGNS)}
${cppNamedConstants("kAlign", UI_ALIGNS)}

${cppArray("kJustifies", UI_JUSTIFIES)}
${cppNamedConstants("kJustify", UI_JUSTIFIES)}

${cppArray("kSizeModes", UI_SIZE_MODES)}
${cppNamedConstants("kSizeMode", UI_SIZE_MODES)}

${cppArray("kFonts", UI_FONTS)}
${cppNamedConstants("kFont", UI_FONTS)}

${cppArray("kButtonVariants", UI_BUTTON_VARIANTS)}
${cppNamedConstants("kButtonVariant", UI_BUTTON_VARIANTS)}

${cppArray("kButtonSizes", UI_BUTTON_SIZES)}
${cppNamedConstants("kButtonSize", UI_BUTTON_SIZES)}

${cppArray("kImageModes", UI_IMAGE_MODES)}
${cppNamedConstants("kImageMode", UI_IMAGE_MODES)}

${cppArray("kAttachToValues", UI_ATTACH_TO_VALUES)}
${cppNamedConstants("kAttachTo", UI_ATTACH_TO_VALUES)}

${cppArray("kAttachPoints", UI_ATTACH_POINTS)}
${cppNamedConstants("kAttachPoint", UI_ATTACH_POINTS)}

${styleFieldArrays()}

struct StringListForKind {
\tconst char * kind;
\tconst char * const * values;
\tstd::size_t valueCount;
};

${kindStringListTable("kNodeTokenFieldsByKind", "kNodeTokenFields", UI_NODE_TOKEN_FIELDS_BY_KIND)}

${kindStringListTable("kRequiredTokenFieldsByKind", "kRequiredTokenFields", UI_REQUIRED_TOKEN_FIELDS_BY_KIND)}

${kindStringListTable("kForbiddenNodeDecoratorsByKind", "kForbiddenNodeDecorators", UI_FORBIDDEN_NODE_DECORATORS_BY_KIND)}

${sizeRuleArrays()}

${cppArray("kUiSurfaces", UI_SURFACES)}
${cppNamedConstants("kUiSurface", UI_SURFACES)}

${cppNamedConstants("kUiComponent", allSurfaceTokens("components"))}
${cppNamedConstants("kUiTextBinding", allSurfaceTokens("textBindings"))}
${cppNamedConstants("kUiAction", allSurfaceTokens("actions"))}

${surfaceTokenArrays()}

}  // namespace ui_layout_contract
}  // namespace net
}  // namespace silencer

#endif
`;
}

const generated = generateHeader();
function tokenManifestText(surface: (typeof UI_SURFACES)[number]): string {
  return `${JSON.stringify({ surface, ...UI_SURFACE_TOKENS_BY_SURFACE[surface] }, null, 2)}\n`;
}

function tokenManifestPath(surface: (typeof UI_SURFACES)[number]): string {
  return join(TOKEN_MANIFEST_DIR, `${surface}.silencer-ui.tokens.json`);
}

if (process.argv.includes("--check")) {
  const current = existsSync(HEADER_PATH) ? readFileSync(HEADER_PATH, "utf8") : "";
  if (current !== generated) {
    console.error(`UI layout C++ contract is stale: ${HEADER_PATH}`);
    console.error("Run: bun run --filter @silencer/ui-layout generate-contract");
    process.exit(1);
  }
  for (const surface of UI_SURFACES) {
    const path = tokenManifestPath(surface);
    const currentManifest = existsSync(path) ? readFileSync(path, "utf8") : "";
    const generatedManifest = tokenManifestText(surface);
    if (currentManifest !== generatedManifest) {
      console.error(`UI layout token manifest is stale: ${path}`);
      console.error("Run: bun run --filter @silencer/ui-layout generate-contract");
      process.exit(1);
    }
  }
} else {
  writeFileSync(HEADER_PATH, generated);
  for (const surface of UI_SURFACES) {
    writeFileSync(tokenManifestPath(surface), tokenManifestText(surface));
  }
}
