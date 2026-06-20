FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libpqxx-dev \
    libpq-dev \
    libhiredis-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/sewenew/redis-plus-plus.git /tmp/redis-plus-plus \
    && cd /tmp/redis-plus-plus \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
                -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
                -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/redis-plus-plus

RUN git clone --depth 1 --recurse-submodules https://github.com/jupp0r/prometheus-cpp.git /tmp/prometheus-cpp \
    && cd /tmp/prometheus-cpp \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
                -DENABLE_PUSH=OFF \
                -DENABLE_COMPRESSION=ON \
                -DBUILD_SHARED_LIBS=ON \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/prometheus-cpp

RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git /tmp/crow \
    && cd /tmp/crow \
    && mkdir build && cd build \
    && cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF \
    && make install \
    && rm -rf /tmp/crow

RUN git clone --depth 1 https://github.com/nlohmann/json.git /tmp/json \
    && cd /tmp/json \
    && mkdir build && cd build \
    && cmake .. -DJSON_BuildTests=OFF \
    && make install \
    && rm -rf /tmp/json

RUN git clone --depth 1 https://github.com/chriskohlhoff/asio.git /tmp/asio \
    && cp -r /tmp/asio/asio/include/asio* /usr/local/include/ \
    && rm -rf /tmp/asio

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j"$(nproc)" \
    && strip /app/build/url_shortener

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libpqxx-7* \
    libpq5 \
    libhiredis* \
    libcurl4 \
    libssl3 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/lib/libredis++* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprometheus* /usr/local/lib/
RUN ldconfig

COPY --from=builder /app/build/url_shortener /usr/local/bin/url_shortener

RUN groupadd --system appgroup && useradd --system --gid appgroup appuser
USER appuser

EXPOSE 8080 9090

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

CMD ["url_shortener"]
