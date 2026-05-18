#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

OUT_DIR="$(mktemp -d)"
SMALL_INSPECT="$OUT_DIR/inspect-640x480.json"
LARGE_INSPECT="$OUT_DIR/inspect-960x720.json"
WIDE_SHORT_INSPECT="$OUT_DIR/inspect-1920x480.json"

check_layout() {
  local width="$1" height="$2" inspect_out="$3" screenshot_out="$4"
  cli --port "$PORT" resize --w "$width" --h "$height" >/dev/null
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  cli --port "$PORT" inspect > "$inspect_out"
  cli --port "$PORT" screenshot --out "$screenshot_out" >/dev/null

  bun -e '
  import { inflateSync } from "node:zlib";

  async function readPng(path) {
    const bytes = new Uint8Array(await Bun.file(path).arrayBuffer());
    const u32 = (offset) =>
      ((bytes[offset] << 24) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
    let width = 0;
    let height = 0;
    let bitDepth = 0;
    let colorType = 0;
    const idat = [];
    for (let offset = 8; offset < bytes.length;) {
      const length = u32(offset);
      const type = String.fromCharCode(...bytes.slice(offset + 4, offset + 8));
      if (type === "IHDR") {
        width = u32(offset + 8);
        height = u32(offset + 12);
        bitDepth = bytes[offset + 16];
        colorType = bytes[offset + 17];
      } else if (type === "IDAT") {
        idat.push(bytes.slice(offset + 8, offset + 8 + length));
      }
      offset += length + 12;
    }
    const bytesPerPixel = colorType === 2 ? 3 : colorType === 6 ? 4 : 0;
    if (bitDepth !== 8 || bytesPerPixel === 0) {
      throw new Error(`unsupported PNG format: bitDepth=${bitDepth} colorType=${colorType}`);
    }
    const raw = inflateSync(Buffer.concat(idat.map((chunk) => Buffer.from(chunk))));
    const stride = width * bytesPerPixel;
    const pixels = new Uint8Array(width * height * bytesPerPixel);
    const prev = new Uint8Array(stride);
    const cur = new Uint8Array(stride);
    let rawOffset = 0;
    const paeth = (a, b, c) => {
      const p = a + b - c;
      const pa = Math.abs(p - a);
      const pb = Math.abs(p - b);
      const pc = Math.abs(p - c);
      return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
    };
    for (let y = 0; y < height; y++) {
      const filter = raw[rawOffset++];
      for (let x = 0; x < stride; x++) {
        const value = raw[rawOffset++];
        const left = x >= bytesPerPixel ? cur[x - bytesPerPixel] : 0;
        const up = prev[x];
        const upLeft = x >= bytesPerPixel ? prev[x - bytesPerPixel] : 0;
        cur[x] = filter === 0
          ? value
          : filter === 1
            ? (value + left) & 255
            : filter === 2
              ? (value + up) & 255
              : filter === 3
                ? (value + Math.floor((left + up) / 2)) & 255
                : (value + paeth(left, up, upLeft)) & 255;
      }
      pixels.set(cur, y * stride);
      prev.set(cur);
    }
    return { width, height, bytesPerPixel, pixels };
  }

  function countGreenPixels(png, rect) {
    let count = 0;
    for (let y = rect.y1; y < rect.y2; y++) {
      for (let x = rect.x1; x < rect.x2; x++) {
        const offset = (y * png.width + x) * png.bytesPerPixel;
        const r = png.pixels[offset];
        const g = png.pixels[offset + 1];
        const b = png.pixels[offset + 2];
        if (g > 50 && g > r + 20 && g > b + 20) count++;
      }
    }
    return count;
  }

  const inspectPath = process.argv[1];
  const screenshotPath = process.argv[2];
  const viewportW = Number(process.argv[3]);
  const viewportH = Number(process.argv[4]);

  const png = await readPng(screenshotPath);
  const pngW = png.width;
  const pngH = png.height;
  if (pngW !== viewportW || pngH !== viewportH) {
    console.error(`screenshot size mismatch: expected ${viewportW}x${viewportH}, got ${pngW}x${pngH}`);
    process.exit(1);
  }
  const footerGreenPixels = countGreenPixels(png, {
    x1: 8,
    y1: Math.max(0, viewportH - 24),
    x2: Math.min(220, viewportW),
    y2: viewportH,
  });
  if (footerGreenPixels < 80) {
    console.error(`version footer is not visible at the bottom-left: greenPixels=${footerGreenPixels}`);
    process.exit(1);
  }

  const data = JSON.parse(await Bun.file(inspectPath).text());
  const expected = [
    { label: "Tutorial" },
    { label: "Connect To Lobby" },
    { label: "Options" },
    { label: "Exit" },
  ];
  const labels = new Set(expected.map((x) => x.label));
  const buttons = (data.widgets ?? []).filter((w) =>
    w.source === "clay" && w.kind === "button" && labels.has(w.label)
  );
  if (buttons.length !== expected.length) {
    console.error(`expected ${expected.length} main-menu buttons, got ${buttons.length}: ${JSON.stringify(buttons)}`);
    process.exit(1);
  }
  for (let i = 0; i < expected.length; i++) {
    const b = buttons[i];
    const e = expected[i];
    if (b.label !== e.label) {
      console.error(`button order mismatch at ${i}: expected ${e.label}, got ${b.label}`);
      process.exit(1);
    }
    if (b.w !== 196 || b.h !== 33) {
      console.error(`unexpected Oval/Md primitive bounds for ${b.label}: ${b.w}x${b.h}`);
      process.exit(1);
    }
    if (b.x < 0 || b.y < 0 || b.x + b.w > viewportW || b.y + b.h > viewportH) {
      console.error(`button out of bounds in ${viewportW}x${viewportH}: ${JSON.stringify(b)}`);
      process.exit(1);
    }
  }

  const [tutorial, connect, options, exit] = buttons;
  const step = 40;
  const staggerChecks = [
    [connect.x - tutorial.x, step, "Connect should be one step right of Tutorial"],
    [connect.x - options.x, step, "Connect should be one step right of Options"],
    [tutorial.x - exit.x, step, "Exit should be one step left of Tutorial"],
    [options.x - tutorial.x, 0, "Tutorial and Options should share x offset"],
  ];
  for (const [actual, expectedValue, message] of staggerChecks) {
    if (actual !== expectedValue) {
      console.error(`${message}: expected ${expectedValue}, got ${actual}`);
      process.exit(1);
    }
  }

  const gaps = [
    connect.y - (tutorial.y + tutorial.h),
    options.y - (connect.y + connect.h),
    exit.y - (options.y + options.h),
  ];
  if (!gaps.every((gap) => gap === 34)) {
    console.error(`vertical gaps should preserve the legacy 34px rhythm: ${gaps.join(",")}`);
    process.exit(1);
  }
  if (viewportW === 640 && viewportH === 480) {
    const legacyBoxes = [
      [tutorial, 350, 154],
      [connect, 390, 221],
      [options, 350, 288],
      [exit, 310, 355],
    ];
    for (const [button, x, y] of legacyBoxes) {
      if (button.x !== x || button.y !== y) {
        console.error(`legacy 640x480 placement mismatch for ${button.label}: expected ${x},${y}, got ${button.x},${button.y}`);
        process.exit(1);
      }
    }
  }
  ' "$inspect_out" "$screenshot_out" "$width" "$height"
}

check_layout 640 480 "$SMALL_INSPECT" "$OUT_DIR/main-640x480.png"
check_layout 960 720 "$LARGE_INSPECT" "$OUT_DIR/main-960x720.png"
check_layout 1920 480 "$WIDE_SHORT_INSPECT" "$OUT_DIR/main-1920x480.png"

bun -e '
const small = JSON.parse(await Bun.file(process.argv[1]).text()).widgets;
const large = JSON.parse(await Bun.file(process.argv[2]).text()).widgets;
const wideShort = JSON.parse(await Bun.file(process.argv[3]).text()).widgets;
const widget = (widgets, label) => widgets.find((w) => w.label === label && w.kind === "button");
const smallConnect = widget(small, "Connect To Lobby");
const largeConnect = widget(large, "Connect To Lobby");
const wideShortConnect = widget(wideShort, "Connect To Lobby");
const smallTutorial = widget(small, "Tutorial");
const largeTutorial = widget(large, "Tutorial");
const smallOptions = widget(small, "Options");
const smallExit = widget(small, "Exit");
const wideShortTutorial = widget(wideShort, "Tutorial");
const wideShortOptions = widget(wideShort, "Options");
const wideShortExit = widget(wideShort, "Exit");
if (!smallConnect || !largeConnect || !wideShortConnect || !smallTutorial || !largeTutorial ||
    !smallOptions || !smallExit || !wideShortTutorial || !wideShortOptions || !wideShortExit) {
  console.error("missing comparison widgets");
  process.exit(1);
}
if (largeConnect.x <= smallConnect.x + 100) {
  console.error(`large layout did not move horizontally with viewport: small=${smallConnect.x}, large=${largeConnect.x}`);
  process.exit(1);
}
if (largeTutorial.y <= smallTutorial.y + 50) {
  console.error(`large layout did not stay vertically centered in viewport: small=${smallTutorial.y}, large=${largeTutorial.y}`);
  process.exit(1);
}

const wideShortShift = (1920 - 640) / 2;
const wideShortExpected = [
  [wideShortTutorial, smallTutorial.x + wideShortShift, smallTutorial.y],
  [wideShortConnect, smallConnect.x + wideShortShift, smallConnect.y],
  [wideShortOptions, smallOptions.x + wideShortShift, smallOptions.y],
  [wideShortExit, smallExit.x + wideShortShift, smallExit.y],
];
for (const [button, expectedX, expectedY] of wideShortExpected) {
  if (button.x !== expectedX || button.y !== expectedY) {
    console.error(`wide-short centered composition mismatch for ${button.label}: expected ${expectedX},${expectedY}, got ${button.x},${button.y}`);
    process.exit(1);
  }
}
const farRightInset = 1920 - (wideShortConnect.x + wideShortConnect.w);
if (farRightInset < 500) {
  console.error(`wide-short button stack is still pinned near the far right edge: rightInset=${farRightInset}`);
  process.exit(1);
}
' "$SMALL_INSPECT" "$LARGE_INSPECT" "$WIDE_SHORT_INSPECT"

echo "PASS 21_main_menu_layout ($OUT_DIR)"
