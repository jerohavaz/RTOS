FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG DOXYGEN_VERSION=1.17.0

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        wget \
        graphviz \
    && rm -rf /var/lib/apt/lists/*

RUN release_tag="Release_${DOXYGEN_VERSION//./_}" \
    && wget \
        --quiet \
        "https://github.com/doxygen/doxygen/releases/download/${release_tag}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz" \
        -O /tmp/doxygen.tar.gz \
    && mkdir -p /tmp/doxygen \
    && tar \
        -xzf /tmp/doxygen.tar.gz \
        --strip-components=1 \
        -C /tmp/doxygen \
    && install \
        -m 755 \
        /tmp/doxygen/bin/doxygen \
        /usr/local/bin/doxygen \
    && rm -rf \
        /tmp/doxygen \
        /tmp/doxygen.tar.gz

RUN doxygen --version \
    && dot -V

WORKDIR /workspace

ENTRYPOINT []