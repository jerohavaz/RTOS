FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        git \
        cmake \
        ninja-build \
        gcc-arm-none-eabi \
        binutils-arm-none-eabi \
        libnewlib-arm-none-eabi \
        libnewlib-dev \
        cppcheck \
        clang-format \
    && rm -rf /var/lib/apt/lists/*

RUN arm-none-eabi-gcc --version \
    && arm-none-eabi-objcopy --version \
    && cmake --version \
    && ninja --version \
    && cppcheck --version \
    && clang-format --version

WORKDIR /workspace

ENTRYPOINT []