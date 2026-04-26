FROM oven/bun:1-alpine
RUN apk add --no-cache mongodb-tools
WORKDIR /app
COPY package.json bun.lock ./
RUN bun install --production --frozen-lockfile
COPY src/ src/
EXPOSE 24080
CMD ["bun", "src/index.js"]
