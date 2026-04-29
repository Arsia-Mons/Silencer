import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

/** @type {import('next').NextConfig} */
//
// NEXT_PUBLIC_* are inlined into the client bundle at build time. We don't
// bake defaults here — see lib/api.js and lib/socket.js for the runtime
// fallbacks (production: empty → relative URLs; dev: docker-compose build
// args supply the values).
//
// outputFileTracingRoot pins the trace root at the monorepo root so the
// standalone output picks up `@silencer/gas-validation` and any other
// workspace dependencies hoisted into `<root>/node_modules/`.
const nextConfig = {
  output: 'standalone',
  outputFileTracingRoot: path.join(__dirname, '../../'),
};

export default nextConfig;
