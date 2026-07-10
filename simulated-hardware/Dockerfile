# syntax=docker/dockerfile:1
FROM node:20-alpine

WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --omit=dev

COPY daemon.js interactive-client.js ./

ENV ROCKETBOX_LISTEN_HOST=0.0.0.0
ENV ROCKETBOX_TCP_PORT=1772
ENV ROCKETBOX_WS_PORT=1773

EXPOSE 1772 1773

CMD ["node", "daemon.js"]
