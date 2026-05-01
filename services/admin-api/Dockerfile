FROM oven/bun:1-alpine
RUN apk add --no-cache mongodb-tools
WORKDIR /app

# Build context is the repo root (workspace lockfile lives there).
# Manifests for every workspace are needed so `bun install` can resolve
# the workspace graph; --filter ./services/admin-api scopes the dep set.
COPY package.json bun.lock ./
COPY clients/cli/package.json            ./clients/cli/
COPY clients/lobby-sdk/ts/package.json   ./clients/lobby-sdk/ts/
COPY services/admin-api/package.json     ./services/admin-api/
COPY shared/fonts/package.json           ./shared/fonts/
COPY shared/gas-validation/package.json  ./shared/gas-validation/
COPY web/admin/package.json              ./web/admin/
COPY web/website/package.json            ./web/website/
RUN bun install --production --frozen-lockfile --filter ./services/admin-api

COPY services/admin-api/src ./services/admin-api/src
WORKDIR /app/services/admin-api
EXPOSE 24080
CMD ["bun", "src/index.js"]
