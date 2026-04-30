import { join, normalize } from "node:path";

const ROOT = import.meta.dir;
const PORT = Number(process.env.PORT) || 3000;

Bun.serve({
  port: PORT,
  async fetch(req) {
    const url = new URL(req.url);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === "/" || pathname.endsWith("/")) pathname += "index.html";

    const safePath = normalize(join(ROOT, pathname));
    if (!safePath.startsWith(ROOT)) {
      return new Response("Forbidden", { status: 403 });
    }

    const f = Bun.file(safePath);
    if (await f.exists()) {
      return new Response(f);
    }
    return new Response("Not found", { status: 404 });
  },
});

console.log(`Serving web/website on http://localhost:${PORT}`);
