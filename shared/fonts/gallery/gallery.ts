type Font = {
  family: string;
  className: string;
  emPx: number;
  advancePx: number;
  bank: number;
  ioffset: number;
  spriteCount: number;
};

const FONTS: Font[] = [
  { family: "Silencer Tiny",     className: "text-tiny",     emPx: 5,  advancePx: 4,  bank: 132, ioffset: 34, spriteCount: 89  },
  { family: "Silencer UI",       className: "text-ui",       emPx: 11, advancePx: 6,  bank: 133, ioffset: 33, spriteCount: 154 },
  { family: "Silencer UI Large", className: "text-ui-large", emPx: 13, advancePx: 9,  bank: 134, ioffset: 33, spriteCount: 154 },
  { family: "Silencer Title",    className: "text-title",    emPx: 24, advancePx: 16, bank: 136, ioffset: 33, spriteCount: 154 },
];

const SAMPLES = [
  "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG",
  "the quick brown fox jumps over the lazy dog",
  "0123456789  !?\"'()[]{}<>+-=*/&%$#@",
];

const root = document.documentElement;
const fg     = document.getElementById("fg") as HTMLInputElement;
const bg     = document.getElementById("bg") as HTMLInputElement;
const scale  = document.getElementById("scale") as HTMLInputElement;
const showAll = document.getElementById("show-all") as HTMLInputElement;
const fgHex  = document.getElementById("fg-hex") as HTMLElement;
const bgHex  = document.getElementById("bg-hex") as HTMLElement;
const scaleDisp = document.getElementById("scale-display") as HTMLElement;
const samples = document.getElementById("samples") as HTMLElement;

function sync() {
  root.style.setProperty("--fg", fg.value);
  root.style.setProperty("--bg", bg.value);
  root.style.setProperty("--scale", scale.value);
  fgHex.textContent = fg.value.toUpperCase();
  bgHex.textContent = bg.value.toUpperCase();
  scaleDisp.textContent = `${scale.value}x`;
}

[fg, bg, scale].forEach((el) => el.addEventListener("input", sync));
showAll.addEventListener("change", render);

function el(tag: string, attrs: Record<string, string> = {}, children: (Node | string)[] = []) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
  for (const c of children) node.append(typeof c === "string" ? document.createTextNode(c) : c);
  return node;
}

function render() {
  samples.replaceChildren();
  for (const font of FONTS) {
    const block = el("section", { class: "sample" });
    const head = el("div", { class: "sample-head" }, [
      el("h2", {}, [font.family]),
      el("span", { class: "meta" }, [
        `bank ${font.bank} · em ${font.emPx}px · advance ${font.advancePx}px · ${font.spriteCount} glyphs`,
      ]),
    ]);
    block.append(head);

    for (const text of SAMPLES) {
      const row = el("div", { class: `sample-row ${font.className}`, "data-label": "" }, [text]);
      block.append(row);
    }

    if (showAll.checked) {
      const grid = el("div", { class: "glyph-grid" });
      const start = font.ioffset;
      const end = font.ioffset + font.spriteCount; // exclusive
      for (let cp = start; cp < end; cp++) {
        const cell = el("div", { class: "glyph-cell" });
        cell.append(
          el("div", { class: `glyph ${font.className}` }, [String.fromCharCode(cp)]),
          el("div", { class: "cp" }, [`U+${cp.toString(16).toUpperCase().padStart(4, "0")}`]),
        );
        grid.append(cell);
      }
      block.append(grid);
    }

    samples.append(block);
  }
}

sync();
render();
