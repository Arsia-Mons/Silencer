FROM oven/bun:1-alpine AS base
WORKDIR /app

FROM base AS deps
COPY package.json bun.lock ./
RUN bun install --frozen-lockfile

FROM base AS builder
# Build-time public env vars baked into the client bundle.
# Production deploys pass none of these — empty values cause lib/api.js +
# lib/socket.js to fall back to relative URLs, which Cloudflare Tunnel
# routes to admin-api on the same box. docker-compose passes localhost
# values for local dev where the browser hits admin-api on its own port.
ARG NEXT_PUBLIC_API_URL=
ARG NEXT_PUBLIC_WS_URL=
ARG NEXT_PUBLIC_MAP_API_URL=
ENV NEXT_PUBLIC_API_URL=$NEXT_PUBLIC_API_URL
ENV NEXT_PUBLIC_WS_URL=$NEXT_PUBLIC_WS_URL
ENV NEXT_PUBLIC_MAP_API_URL=$NEXT_PUBLIC_MAP_API_URL
COPY --from=deps /app/node_modules ./node_modules
COPY . .
ENV NEXT_TELEMETRY_DISABLED=1
RUN bun run build

FROM base AS runner
ENV NODE_ENV=production
ENV NEXT_TELEMETRY_DISABLED=1
COPY --from=builder /app/.next/standalone ./
COPY --from=builder /app/.next/static ./.next/static
COPY --from=builder /app/public ./public
EXPOSE 24000
ENV PORT=24000
CMD ["bun", "server.js"]
