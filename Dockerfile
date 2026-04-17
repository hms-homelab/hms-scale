# HMS-Scale - Multi-stage Docker build
# Produces ~80MB final image with C++ runtime + Angular Web UI

# =============================================================================
# Stage 1: Angular UI Builder
# =============================================================================
FROM node:22-slim AS ui-builder

WORKDIR /ui
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npx ng build --configuration production

# =============================================================================
# Stage 2: C++ Builder
# =============================================================================
FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    git \
    libcurl4-openssl-dev \
    libpq-dev \
    libpqxx-dev \
    libssl-dev \
    libjsoncpp-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    nlohmann-json3-dev \
    libspdlog-dev \
    libdrogon-dev \
    uuid-dev libbrotli-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt VERSION ./
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir build && cd build && \
    cmake -DBUILD_TESTS=OFF -DBUILD_WITH_WEB=ON .. && \
    make -j$(nproc) && \
    strip hms_colada

# =============================================================================
# Stage 3: Runtime
# =============================================================================
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libcurl4t64 \
    libpq5 \
    libpqxx-7.10 \
    libssl3 \
    libjsoncpp26 \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    libspdlog1.15 \
    libfmt10 \
    libdrogon1t64 \
    libtrantor1 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -r -u 1000 -m -s /bin/bash colada

COPY --from=builder /build/build/hms_colada /usr/local/bin/hms_colada
RUN chmod +x /usr/local/bin/hms_colada

COPY --from=ui-builder /ui/dist/browser/ /home/colada/static/browser/

RUN mkdir -p /home/colada/.hms-colada/models && \
    chown -R colada:colada /home/colada

USER colada
WORKDIR /home/colada

ENV WEB_PORT=8889
ENV STATIC_DIR=/home/colada/static/browser

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:${WEB_PORT}/health || exit 1

EXPOSE 8889

ENTRYPOINT ["/usr/local/bin/hms_colada"]
