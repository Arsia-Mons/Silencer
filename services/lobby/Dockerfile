# ── stage 1: build SDL3 + SDL3_mixer from source ─────────────────────────────
# Ubuntu 22.04 ships SDL2 only; SDL3 must be built from source.
# We disable all optional audio decoders — the dedicated server doesn't need them.
FROM ubuntu:22.04 AS sdl3-builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Check-Valid-Until=false \
            -o Acquire::AllowInsecureRepositories=true update \
 && apt-get install -y --no-install-recommends --allow-unauthenticated \
        build-essential cmake git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

RUN git clone --depth=1 https://github.com/libsdl-org/SDL.git /sdl && \
    cmake -B /sdl/build -S /sdl \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF \
        -DSDL_UNIX_CONSOLE_BUILD=ON \
        -DCMAKE_INSTALL_PREFIX=/opt/sdl3 && \
    cmake --build /sdl/build -j"$(nproc)" && \
    cmake --install /sdl/build && \
    rm -rf /sdl

RUN git clone --depth=1 https://github.com/libsdl-org/SDL_mixer.git /sdlmix && \
    cmake -B /sdlmix/build -S /sdlmix \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL3_DIR=/opt/sdl3/lib/cmake/SDL3 \
        -DCMAKE_INSTALL_PREFIX=/opt/sdl3 \
        -DSDL3MIXER_MP3=OFF \
        -DSDL3MIXER_MOD=OFF \
        -DSDL3MIXER_MIDI=OFF \
        -DSDL3MIXER_OPUS=OFF \
        -DSDL3MIXER_FLAC=OFF && \
    cmake --build /sdlmix/build -j"$(nproc)" && \
    cmake --install /sdlmix/build && \
    rm -rf /sdlmix

# ── stage 2: Go lobby server ──────────────────────────────────────────────────
FROM golang:1.22-alpine AS go-builder

WORKDIR /build
COPY services/lobby/ .
RUN go build -trimpath -ldflags="-s -w" -o silencer-lobby .

# ── stage 3: C++ dedicated server (Linux) ─────────────────────────────────────
FROM ubuntu:22.04 AS cpp-builder

ENV DEBIAN_FRONTEND=noninteractive
COPY --from=sdl3-builder /opt/sdl3 /opt/sdl3

RUN apt-get -o Acquire::Check-Valid-Until=false \
            -o Acquire::AllowInsecureRepositories=true update \
 && apt-get install -y --no-install-recommends --allow-unauthenticated \
        build-essential cmake pkg-config \
        zlib1g-dev libcurl4-openssl-dev libminizip-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY clients/silencer/CMakeLists.txt clients/silencer/
COPY clients/silencer/cmake/         clients/silencer/cmake/
COPY clients/silencer/src/           clients/silencer/src/
COPY clients/silencer/shaders/       clients/silencer/shaders/
COPY shared/assets/ shared/assets/

RUN cmake -B build -S clients/silencer \
        -DCMAKE_BUILD_TYPE=Release \
        -DSILENCER_LOBBY_HOST="127.0.0.1" \
        -DSILENCER_LOBBY_PORT=15170 \
        -DSDL3_DIR=/opt/sdl3/lib/cmake/SDL3 \
        -DSDL3_mixer_DIR=/opt/sdl3/lib/cmake/SDL3_mixer \
    && cmake --build build -j"$(nproc)"

# ── stage 4: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Check-Valid-Until=false \
            -o Acquire::AllowInsecureRepositories=true update \
 && apt-get install -y --no-install-recommends --allow-unauthenticated \
        zlib1g libcurl4 libminizip1 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# SDL3 shared libs (built from source; not in Ubuntu 22.04 apt)
COPY --from=sdl3-builder /opt/sdl3/lib /opt/sdl3/lib
RUN echo /opt/sdl3/lib > /etc/ld.so.conf.d/sdl3.conf && ldconfig

# Game data — read by the dedicated server via GetResDir() → /usr/local/share/silencer
COPY --from=cpp-builder /src/shared/assets/ /usr/local/share/silencer/

# Binaries
COPY --from=go-builder  /build/silencer-lobby      /usr/local/bin/silencer-lobby
COPY --from=cpp-builder /src/build/silencer        /usr/local/bin/silencer

# lobby.json is persisted here via a named volume.
WORKDIR /data

EXPOSE 15170/tcp
EXPOSE 15170/udp

ENTRYPOINT ["/usr/local/bin/silencer-lobby"]
CMD ["-addr", ":15170", "-public-addr", "127.0.0.1", \
     "-db", "/data/lobby.json", \
     "-game-binary", "/usr/local/bin/silencer"]
