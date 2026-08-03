// Bundles the TS frontend into ../dist for Tauri to serve. No dev server or
// bundler framework — Bun.build handles TS, and we copy the static shell.
import { cp, mkdir, rm } from "node:fs/promises";

const OUT = new URL("../dist/", import.meta.url);
const SRC = new URL("../src/", import.meta.url);

await rm(OUT, { recursive: true, force: true });
await mkdir(OUT, { recursive: true });

const result = await Bun.build({
  entrypoints: [Bun.fileURLToPath(new URL("main.ts", SRC))],
  outdir: Bun.fileURLToPath(OUT),
  target: "browser",
  minify: false,
  sourcemap: "none",
});

if (!result.success) {
  for (const log of result.logs) console.error(log);
  process.exit(1);
}

await cp(new URL("index.html", SRC), new URL("index.html", OUT));
await cp(new URL("styles.css", SRC), new URL("styles.css", OUT));

console.log("frontend built -> dist/");
