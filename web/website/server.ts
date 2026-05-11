import { join, normalize } from "node:path";

const ROOT = import.meta.dir;
const PORT = Number(process.env.PORT) || 3000;

// Dev-only fallback: serve the WASM build artifacts (silencer.js,
// silencer.wasm, silencer.data, silencer.html) out of clients/silencer/build-wasm/
// when requested under /spectate/. Avoids a copy step during iteration
// — the build artifacts can stay where Emscripten emits them and the
// spectator page picks them up live. Production hosting on Cloudflare
// publishes them as static assets under /spectate/ via a build action.
const WASM_BUILD_DIR = normalize(join(
  ROOT, "..", "..", "clients", "silencer", "build-wasm",
));

function isWasmAsset(p: string): boolean {
  if (!p.startsWith("/spectate/")) return false;
  const name = p.slice("/spectate/".length);
  return (
    name === "silencer.js" ||
    name === "silencer.wasm" ||
    name === "silencer.data" ||
    name === "silencer.html" ||
    name === "silencer.symbols"
  );
}

Bun.serve({
  port: PORT,
  async fetch(req) {
    const url = new URL(req.url);
    const t0 = Date.now();
    const log = (status: number) => {
      console.log(`${new Date().toISOString()} ${req.method} ${url.pathname} -> ${status} (${Date.now() - t0}ms)`);
    };
    let pathname = decodeURIComponent(url.pathname);

    // Force trailing slash for directories that we'd otherwise serve an
    // index.html for. Without this the browser resolves relative URLs in
    // the served HTML against the parent dir (e.g. /spectate's data fetch
    // resolves silencer.data → /silencer.data instead of /spectate/...).
    if (pathname === "/spectate") {
      log(301);
      return new Response(null, { status: 301, headers: { Location: "/spectate/" } });
    }

    if (pathname === "/" || pathname.endsWith("/")) pathname += "index.html";

    const safePath = normalize(join(ROOT, pathname));
    if (!safePath.startsWith(ROOT)) {
      return new Response("Forbidden", { status: 403 });
    }

    const f = Bun.file(safePath);
    if (await f.exists()) {
      log(200);
      return new Response(f);
    }

    // Fallback: if the requested path is a directory, serve its index.html.
    const indexPath = normalize(join(safePath, "index.html"));
    if (indexPath.startsWith(ROOT)) {
      const idx = Bun.file(indexPath);
      if (await idx.exists()) {
        log(200);
        return new Response(idx);
      }
    }

    if (isWasmAsset(pathname)) {
      const name = pathname.slice("/spectate/".length);
      const wasmFile = Bun.file(join(WASM_BUILD_DIR, name));
      if (await wasmFile.exists()) {
        log(200);
        return new Response(wasmFile);
      }
      log(404);
      return new Response(
        `${name} not built — run\n  cd clients/silencer && emcmake cmake -S . -B build-wasm && emmake make -j -C build-wasm\nfrom a shell with emsdk activated.`,
        { status: 404 },
      );
    }
    log(404);
    return new Response("Not found", { status: 404 });
  },
});

console.log(`Serving web/website on http://localhost:${PORT}`);
console.log(`Spectator WASM fallback: ${WASM_BUILD_DIR}`);
