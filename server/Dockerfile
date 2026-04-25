# ── stage 1: Go lobby server ───────────────────────────────────────────────────
FROM golang:1.22-alpine AS go-builder

WORKDIR /build
COPY server/ .
RUN go build -trimpath -ldflags="-s -w" -o zsilencer-lobby .

# ── stage 2: C++ dedicated server (Linux) ─────────────────────────────────────
FROM ubuntu:22.04 AS cpp-builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Check-Valid-Until=false \
            -o Acquire::AllowInsecureRepositories=true update \
 && apt-get install -y --no-install-recommends --allow-unauthenticated \
        build-essential cmake pkg-config \
        libsdl2-dev libsdl2-mixer-dev \
        zlib1g-dev libcurl4-openssl-dev libminizip-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY clients/silencer/CMakeLists.txt clients/silencer/
COPY clients/silencer/cmake/         clients/silencer/cmake/
COPY clients/silencer/src/           clients/silencer/src/
COPY clients/silencer/resources.rc   clients/silencer/
COPY shared/assets/ shared/assets/

RUN cmake -B build -S clients/silencer \
        -DCMAKE_BUILD_TYPE=Release \
        -DSILENCER_LOBBY_HOST="127.0.0.1" \
        -DSILENCER_LOBBY_PORT=15170 \
    && cmake --build build -j"$(nproc)"

# ── stage 3: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Check-Valid-Until=false \
            -o Acquire::AllowInsecureRepositories=true update \
 && apt-get install -y --no-install-recommends --allow-unauthenticated \
        libsdl2-2.0-0 libsdl2-mixer-2.0-0 \
        zlib1g libcurl4 libminizip1 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Game data — read by the dedicated server via GetResDir() → /usr/local/share/silencer
COPY --from=cpp-builder /src/shared/assets/ /usr/local/share/silencer/

# Binaries
COPY --from=go-builder  /build/zsilencer-lobby      /usr/local/bin/zsilencer-lobby
COPY --from=cpp-builder /src/build/silencer         /usr/local/bin/silencer

# lobby.json is persisted here via a named volume.
WORKDIR /data

EXPOSE 15170/tcp
EXPOSE 15170/udp

ENTRYPOINT ["/usr/local/bin/zsilencer-lobby"]
CMD ["-addr", ":15170", "-public-addr", "127.0.0.1", \
     "-db", "/data/lobby.json", \
     "-game-binary", "/usr/local/bin/silencer"]
