FROM node:22-alpine AS base
WORKDIR /app

FROM base AS deps
COPY package.json .
RUN npm install

FROM base AS builder
ARG NEXT_PUBLIC_API_URL=http://localhost:24080
ARG NEXT_PUBLIC_WS_URL=ws://localhost:24080
ENV NEXT_PUBLIC_API_URL=$NEXT_PUBLIC_API_URL
ENV NEXT_PUBLIC_WS_URL=$NEXT_PUBLIC_WS_URL
COPY --from=deps /app/node_modules ./node_modules
COPY . .
ENV NEXT_TELEMETRY_DISABLED=1
RUN npm run build

FROM base AS runner
ENV NODE_ENV=production
ENV NEXT_TELEMETRY_DISABLED=1
COPY --from=builder /app/.next/standalone ./
COPY --from=builder /app/.next/static ./.next/static
COPY --from=builder /app/public ./public
EXPOSE 24000
ENV PORT=24000
CMD ["node", "server.js"]
