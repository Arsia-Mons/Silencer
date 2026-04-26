/** @type {import('next').NextConfig} */
//
// NEXT_PUBLIC_* are inlined into the client bundle at build time. We don't
// bake defaults here — see lib/api.js and lib/socket.js for the runtime
// fallbacks (production: empty → relative URLs; dev: docker-compose build
// args supply the values).
const nextConfig = {
  output: 'standalone',
};

export default nextConfig;
