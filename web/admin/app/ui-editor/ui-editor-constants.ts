import {
  type UiAlign,
  type UiAxis,
  type UiFont,
  type UiJustify,
  type UiNodeKind,
  type UiSizeMode,
} from "../../lib/ui-layout";

export const STORAGE_KEY = "silencer-ui-editor-document-v1";

export const KIND_LABELS: Record<UiNodeKind, string> = {
  screen: "SCREEN",
  panel: "PANEL",
  stack: "STACK",
  row: "ROW",
  text: "TEXT",
  button: "BUTTON",
  spacer: "SPACER",
  component: "COMPONENT",
};

export const PRESETS = [
  { name: "1280 x 720", width: 1280, height: 720, zoom: 0.72 },
  { name: "960 x 540", width: 960, height: 540, zoom: 0.92 },
  { name: "640 x 480", width: 640, height: 480, zoom: 1 },
];

export const SIZE_MODES: UiSizeMode[] = ["fit", "grow", "fixed"];
export const AXES: UiAxis[] = ["column", "row"];
export const ALIGNS: UiAlign[] = ["start", "center", "end"];
export const JUSTIFIES: UiJustify[] = ["start", "center", "end"];
export const FONTS: UiFont[] = ["ui", "uiLarge", "title", "tiny", "footer"];
