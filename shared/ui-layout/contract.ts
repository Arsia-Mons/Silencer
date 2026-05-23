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
