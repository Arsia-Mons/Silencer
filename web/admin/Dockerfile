FROM oven/bun:1-alpine AS base
WORKDIR /app

# Build context is the repo root (workspace lockfile lives there).
# Stage 1: install workspace deps once, scoped via --filter so only
# the admin-web subtree (+ its workspace deps like
# @silencer/gas-validation) is materialized.
FROM base AS deps
COPY package.json bun.lock ./
COPY clients/cli/package.json            ./clients/cli/
COPY clients/lobby-sdk/ts/package.json   ./clients/lobby-sdk/ts/
COPY clients/tui/package.json            ./clients/tui/
COPY services/admin-api/package.json     ./services/admin-api/
COPY shared/fonts/package.json           ./shared/fonts/
COPY shared/gas-validation/package.json  ./shared/gas-validation/
COPY web/admin/package.json              ./web/admin/
COPY web/website/package.json            ./web/website/
RUN bun install --frozen-lockfile --filter ./web/admin

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
COPY --from=deps /app /app
COPY shared/gas-validation ./shared/gas-validation
COPY web/admin             ./web/admin
WORKDIR /app/web/admin
ENV NEXT_TELEMETRY_DISABLED=1
RUN bun run build

FROM base AS runner
ENV NODE_ENV=production
ENV NEXT_TELEMETRY_DISABLED=1
COPY --from=builder /app/web/admin/.next/standalone ./
COPY --from=builder /app/web/admin/.next/static ./web/admin/.next/static
COPY --from=builder /app/web/admin/public ./web/admin/public
EXPOSE 24000
ENV PORT=24000
WORKDIR /app/web/admin
CMD ["bun", "server.js"]
