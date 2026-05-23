export const UI_NODE_KINDS = [
  "screen",
  "panel",
  "stack",
  "row",
  "text",
  "button",
  "spacer",
  "component",
] as const;
export const UI_CONTAINER_NODE_KINDS = ["screen", "panel", "stack", "row"] as const;
export const UI_LAYOUT_SCHEMA_VERSION = 1 as const;

export const UI_NUMERIC_LIMITS = {
  viewportMin: 160,
  viewportMax: 4096,
  sizeMin: 0,
  sizeMax: 4096,
  paddingMin: 0,
  paddingMax: 512,
  gapMin: 0,
  gapMax: 512,
  radiusMin: 0,
  radiusMax: 64,
  paletteMin: -1,
  paletteTextMin: 0,
  paletteMax: 255,
  imageBankMin: 0,
  imageBankMax: 255,
  imageIndexMin: 0,
  imageIndexMax: 65535,
  floatingOffsetMin: -4096,
  floatingOffsetMax: 4096,
  floatingZIndexMin: -32768,
  floatingZIndexMax: 32767,
} as const;

export const UI_DOCUMENT_FIELDS = ["schemaVersion", "surface", "viewport", "root"] as const;
export const UI_VIEWPORT_FIELDS = ["width", "height"] as const;
export const UI_NODE_FIELDS = [
  "id",
  "kind",
  "name",
  "text",
  "action",
  "textBinding",
  "component",
  "buttonVariant",
  "buttonSize",
  "image",
  "floating",
  "style",
  "children",
] as const;
export const UI_SIZE_FIELDS = ["mode", "value", "min", "max"] as const;
export const UI_IMAGE_FIELDS = ["bank", "index", "mode"] as const;
export const UI_FLOATING_FIELDS = [
  "attachTo",
  "elementAttach",
  "parentAttach",
  "offsetX",
  "offsetY",
  "zIndex",
  "pointerPassthrough",
] as const;

export const UI_AXES = ["column", "row"] as const;
export const UI_ALIGNS = ["start", "center", "end"] as const;
export const UI_JUSTIFIES = ["start", "center", "end"] as const;
export const UI_SIZE_MODES = ["fit", "grow", "fixed"] as const;
export const UI_FONTS = ["ui", "uiLarge", "title", "tiny", "footer"] as const;
export const UI_BUTTON_VARIANTS = ["oval", "chrome", "text", "ghost"] as const;
export const UI_BUTTON_SIZES = ["sm", "md", "lg", "compact", "auto"] as const;
export const UI_IMAGE_MODES = ["normal", "contain", "stretch"] as const;
export const UI_ATTACH_TO_VALUES = ["parent", "root"] as const;
export const UI_ATTACH_POINTS = [
  "left-top",
  "left-center",
  "left-bottom",
  "center-top",
  "center",
  "center-bottom",
  "right-top",
  "right-center",
  "right-bottom",
] as const;

export const UI_STYLE_FIELDS_BY_KIND = {
  screen: [
    "width",
    "height",
    "direction",
    "align",
    "justify",
    "padding",
    "gap",
    "backgroundPalette",
    "borderPalette",
    "radius",
  ],
  panel: [
    "width",
    "height",
    "direction",
    "align",
    "justify",
    "padding",
    "gap",
    "backgroundPalette",
    "borderPalette",
    "radius",
  ],
  stack: [
    "width",
    "height",
    "direction",
    "align",
    "justify",
    "padding",
    "gap",
    "backgroundPalette",
    "borderPalette",
    "radius",
  ],
  row: [
    "width",
    "height",
    "direction",
    "align",
    "justify",
    "padding",
    "gap",
    "backgroundPalette",
    "borderPalette",
    "radius",
  ],
  text: [
    "width",
    "height",
    "padding",
    "backgroundPalette",
    "borderPalette",
    "textPalette",
    "font",
    "radius",
  ],
  button: ["width", "height", "padding", "textPalette"],
  spacer: ["width", "height"],
  component: ["width", "height"],
} as const satisfies Record<(typeof UI_NODE_KINDS)[number], readonly string[]>;

export const UI_NODE_TOKEN_FIELDS_BY_KIND = {
  screen: [],
  panel: [],
  stack: [],
  row: [],
  text: ["text", "textBinding"],
  button: ["text", "action", "buttonVariant", "buttonSize"],
  spacer: [],
  component: ["component"],
} as const satisfies Record<(typeof UI_NODE_KINDS)[number], readonly string[]>;

export const UI_REQUIRED_TOKEN_FIELDS_BY_KIND = {
  screen: [],
  panel: [],
  stack: [],
  row: [],
  text: [],
  button: [],
  spacer: [],
  component: ["component"],
} as const satisfies Record<(typeof UI_NODE_KINDS)[number], readonly string[]>;

export const UI_FORBIDDEN_NODE_DECORATORS_BY_KIND = {
  screen: [],
  panel: [],
  stack: [],
  row: [],
  text: [],
  button: ["image", "floating"],
  spacer: [],
  component: [],
} as const satisfies Record<(typeof UI_NODE_KINDS)[number], readonly string[]>;

export const UI_SIZE_RULES_BY_KIND = {
  button: {
    width: { modes: ["fit", "fixed"], allowBounds: false },
    height: { modes: ["fit"], allowBounds: false },
  },
} as const;

export const UI_STYLE_DEFAULTS_BY_KIND = {
  screen: {
    width: { mode: "fixed", value: 1280 },
    height: { mode: "fixed", value: 720 },
    direction: "column",
    align: "start",
    justify: "start",
    padding: 24,
    gap: 12,
    backgroundPalette: 0,
  },
  panel: {
    width: { mode: "fixed", value: 320 },
    height: { mode: "fit" },
    direction: "column",
    align: "start",
    justify: "start",
    padding: 14,
    gap: 8,
    backgroundPalette: 74,
    borderPalette: 216,
    radius: 2,
  },
  stack: {
    width: { mode: "grow" },
    height: { mode: "fit" },
    direction: "column",
    align: "start",
    justify: "start",
    padding: 0,
    gap: 8,
  },
  row: {
    width: { mode: "grow" },
    height: { mode: "fit" },
    direction: "row",
    align: "start",
    justify: "start",
    padding: 0,
    gap: 8,
  },
  text: {
    width: { mode: "fit" },
    height: { mode: "fit" },
    font: "uiLarge",
    textPalette: 0,
  },
  button: {
    width: { mode: "fit" },
    height: { mode: "fit" },
    textPalette: 0,
    padding: 10,
  },
  spacer: {
    width: { mode: "grow" },
    height: { mode: "fixed", value: 12 },
  },
  component: {
    width: { mode: "fit" },
    height: { mode: "fit" },
  },
} as const satisfies Record<(typeof UI_NODE_KINDS)[number], object>;

export const UI_SURFACES = ["main-menu", "options"] as const;
export const UI_SURFACE_TOKENS_BY_SURFACE = {
  "main-menu": {
    components: ["main-menu.logo"],
    textBindings: ["client.version"],
    actions: ["main_menu.tutorial", "main_menu.lobby", "main_menu.options", "main_menu.exit"],
  },
  options: {
    components: [],
    textBindings: [],
    actions: ["options.controls", "options.display", "options.audio", "options.back"],
  },
} as const satisfies Record<
  (typeof UI_SURFACES)[number],
  {
    components: readonly string[];
    textBindings: readonly string[];
    actions: readonly string[];
  }
>;
