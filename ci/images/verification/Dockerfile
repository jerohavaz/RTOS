FROM python:3.12-slim

ARG TESSLA_URL=https://git.tessla.io/tessla/tessla/-/jobs/1872/artifacts/raw/target/scala-3.3.1/tessla-assembly-2.1.0.jar

ENV TESSLA_JAR=/opt/tessla/tessla.jar
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        cargo \
        curl \
        openjdk-21-jre-headless \
        rustc \
    && mkdir -p /opt/tessla \
    && curl \
        --fail \
        --location \
        --retry 3 \
        --output "${TESSLA_JAR}" \
        "${TESSLA_URL}" \
    && test -s "${TESSLA_JAR}" \
    && java -jar "${TESSLA_JAR}" --version \
    && rustc --version \
    && cargo --version \
    && python3 --version \
    && apt-get purge -y --auto-remove curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

ENTRYPOINT []