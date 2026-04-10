FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg

RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    ninja-build \
    pkg-config \
    tar \
    unzip \
    zip \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt \
    && curl -L --retry 5 --retry-delay 5 https://github.com/microsoft/vcpkg/archive/refs/heads/master.tar.gz -o /tmp/vcpkg.tar.gz \
    && tar -xzf /tmp/vcpkg.tar.gz -C /opt \
    && mv /opt/vcpkg-master ${VCPKG_ROOT} \
    && rm /tmp/vcpkg.tar.gz \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app
COPY . .

RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DBUILD_BENCH=OFF \
    -DBUILD_TESTING=OFF \
    && cmake --build build --target server

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/server /app/server
COPY --from=builder /app/build/vcpkg_installed/x64-linux/lib /opt/job-queue-deps/lib

ENV LD_LIBRARY_PATH=/opt/job-queue-deps/lib

EXPOSE 8080

ENTRYPOINT ["/app/server"]
CMD ["--threads", "4", "--max_queue", "100", "--timeout", "5", "--job_timeout", "150", "--keep_alive_s", "300"]
