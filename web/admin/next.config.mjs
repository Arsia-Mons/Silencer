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
// experimental.outputFileTracingRoot pins the trace root at the monorepo
// root so the standalone output picks up `@silencer/gas-validation` and
// any other workspace dependencies hoisted into `<root>/node_modules/`.
// (Promoted to top-level in Next 15+; we're on 14.2 so it lives under
// experimental — top-level emits an "Unrecognized key" warning and is
// silently ignored.)
const nextConfig = {
  output: 'standalone',
  experimental: {
    outputFileTracingRoot: path.join(__dirname, '../../'),
  },
};

export default nextConfig;
