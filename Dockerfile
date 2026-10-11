FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    bc bison flex libssl-dev libelf-dev python3 perl patch git curl ca-certificates \
    xz-utils zstd cpio kmod device-tree-compiler u-boot-tools squashfs-tools \
    mtd-utils busybox-static && rm -rf /var/lib/apt/lists/*
WORKDIR /src
