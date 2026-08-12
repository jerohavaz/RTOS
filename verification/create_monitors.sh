#!/usr/bin/env bash
set -euo pipefail

TESSLA_VERIFY="tessla_verify.py"
TESSLA_JAR="/home/jero/Desktop/tessla.jar"
OUTPUT_DIR="monitors"
BUILD_DIR="build"

MODES=("checks" "violations")

mkdir -p \
    "$OUTPUT_DIR/checks" \
    "$OUTPUT_DIR/violations"

generate_monitor() {
    local name="$1"
    local mode="$2"
    shift 2

    echo "============================================================"
    echo "Generating: $name"
    echo "Mode:       $mode"
    echo "============================================================"

    rm -f \
        "$BUILD_DIR/combined.tessla" \
        "$BUILD_DIR/combined-monitor"

    python3 "$TESSLA_VERIFY" generate "$@" \
        --combined \
        --mode "$mode" \
        --rust \
        --tessla-jar "$TESSLA_JAR"

    local tessla_file="$BUILD_DIR/combined.tessla"
    local monitor_file="$BUILD_DIR/combined-monitor"

    if [[ ! -f "$tessla_file" ]]; then
        echo "ERROR: $tessla_file was not generated for $name ($mode)" >&2
        exit 1
    fi

    if [[ ! -f "$monitor_file" ]]; then
        echo "ERROR: $monitor_file was not generated for $name ($mode)" >&2
        exit 1
    fi

    mv "$tessla_file" \
        "$OUTPUT_DIR/$mode/$name.tessla"

    mv "$monitor_file" \
        "$OUTPUT_DIR/$mode/$name-monitor"

    chmod +x "$OUTPUT_DIR/$mode/$name-monitor"

    echo "[SAVED] $OUTPUT_DIR/$mode/$name.tessla"
    echo "[SAVED] $OUTPUT_DIR/$mode/$name-monitor"
    echo
}

for mode in "${MODES[@]}"; do

    # PROJECT_SCHEDULER
    generate_monitor scheduler "$mode" \
        integrity delay scheduler \
        --max-tasks 5

    # PROJECT_DELAY
    generate_monitor delay "$mode" \
        integrity delay scheduler \
        --max-tasks 3

    # PROJECT_SEMAPHORE
    generate_monitor semaphore "$mode" \
        integrity delay scheduler semaphore \
        --max-tasks 5 \
        --max-semaphores 3

    # PROJECT_MUTEX
    generate_monitor mutex "$mode" \
        integrity delay scheduler semaphore mutex \
        --max-tasks 6 \
        --max-semaphores 2 \
        --max-mutexes 1

    # PROJECT_QUEUE
    generate_monitor queue "$mode" \
        integrity delay scheduler queue \
        --max-tasks 4 \
        --queue 1:1

done

echo "============================================================"
echo "All monitors generated successfully."
echo "============================================================"

if command -v tree >/dev/null 2>&1; then
    tree "$OUTPUT_DIR"
else
    find "$OUTPUT_DIR" -type f | sort
fi