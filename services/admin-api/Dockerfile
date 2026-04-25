FROM node:22-alpine
RUN apk add --no-cache mongodb-tools
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev
COPY src/ src/
EXPOSE 24080
CMD ["node", "src/index.js"]
