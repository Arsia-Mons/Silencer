// Static file server for the stub e2e. Usage: bun serve.ts <dir> <port>
const dir = process.argv[2];
const port = Number(process.argv[3]);
Bun.serve({
  port,
  async fetch(req) {
    const path = new URL(req.url).pathname;
    const file = Bun.file(`${dir}${path}`);
    return (await file.exists())
      ? new Response(file)
      : new Response("not found", { status: 404 });
  },
});
console.log(`serving ${dir} on :${port}`);
