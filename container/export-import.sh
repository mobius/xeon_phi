#!/bin/bash
# Container image export / import helper for Intel Phi dev environment
# The exported image includes ICC 16.0 + TBB 4.4 (~3-5GB compressed)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-intel-phi-dev}"
TAG="${TAG:-latest}"
EXPORT_FILE="${EXPORT_FILE:-$PROJECT_ROOT/container/intel-phi-dev.tar.gz}"

usage() {
    cat << EOF
Usage: $(basename "$0") <command>

Commands:
    export      Export the current container to a compressed tar.gz
                Output: $EXPORT_FILE

    import      Import a previously exported tar.gz as a podman image
                Input:  $EXPORT_FILE

    save        Quick save from existing image (no commit)
                Faster if image already exists

    load        Load a saved tar.gz into podman

    run         Run the container with host mounts

Examples:
    $(basename "$0") export      # Save current container
    $(basename "$0") import      # Restore on another machine
    $(basename "$0") run         # Start development environment
EOF
}

cmd_export() {
    echo "=== Exporting container to $EXPORT_FILE ==="
    echo "This may take several minutes (image size ~8GB)..."

    # Commit current container state
    echo "Committing container..."
    podman commit --pause=false -f docker "${IMAGE_NAME}-live" "${IMAGE_NAME}:${TAG}" 2>/dev/null || \
    podman commit --pause=false -f docker centos7-phi-dev "${IMAGE_NAME}:${TAG}"

    # Save and compress
    echo "Saving image..."
    mkdir -p "$(dirname "$EXPORT_FILE")"

    if which pigz >/dev/null 2>&1; then
        podman save "${IMAGE_NAME}:${TAG}" | pigz -p$(nproc) > "$EXPORT_FILE"
    else
        podman save "${IMAGE_NAME}:${TAG}" | gzip > "$EXPORT_FILE"
    fi

    ls -lh "$EXPORT_FILE"
    echo "Export complete."
}

cmd_import() {
    if [ ! -f "$EXPORT_FILE" ]; then
        echo "ERROR: Export file not found: $EXPORT_FILE"
        exit 1
    fi

    echo "=== Importing $EXPORT_FILE ==="
    echo "This may take several minutes..."

    if which pigz >/dev/null 2>&1; then
        pigz -dc "$EXPORT_FILE" | podman load
    else
        gunzip -c "$EXPORT_FILE" | podman load
    fi

    echo "Import complete."
    podman images "${IMAGE_NAME}:${TAG}" --format 'Imported: {{.Repository}}:{{.Tag}} ({{.Size}})'
}

cmd_save() {
    echo "=== Saving existing image ==="
    if ! podman images "${IMAGE_NAME}:${TAG}" --format '{{.ID}}' | grep -q .; then
        echo "ERROR: Image ${IMAGE_NAME}:${TAG} not found"
        echo "Use 'export' to commit and save the current container first."
        exit 1
    fi

    echo "Saving ${IMAGE_NAME}:${TAG} to $EXPORT_FILE..."
    mkdir -p "$(dirname "$EXPORT_FILE")"

    if which pigz >/dev/null 2>&1; then
        podman save "${IMAGE_NAME}:${TAG}" | pigz -p$(nproc) > "$EXPORT_FILE"
    else
        podman save "${IMAGE_NAME}:${TAG}" | gzip > "$EXPORT_FILE"
    fi

    ls -lh "$EXPORT_FILE"
    echo "Save complete."
}

cmd_load() {
    if [ ! -f "$EXPORT_FILE" ]; then
        echo "ERROR: Export file not found: $EXPORT_FILE"
        exit 1
    fi

    echo "=== Loading $EXPORT_FILE ==="
    if which pigz >/dev/null 2>&1; then
        pigz -dc "$EXPORT_FILE" | podman load
    else
        gunzip -c "$EXPORT_FILE" | podman load
    fi
    echo "Load complete."
}

cmd_run() {
    echo "=== Starting Intel Phi dev container ==="

    # Check required host mounts
    if [ ! -d "/opt/mpss/3.8.6" ]; then
        echo "WARNING: /opt/mpss/3.8.6 not found on host"
        echo "  MIC offload will not work without MPSS libraries."
    fi

    if [ ! -d "/usr/linux-k1om-4.7" ]; then
        echo "WARNING: /usr/linux-k1om-4.7 not found on host"
        echo "  K1OM cross-compiler will not be available."
    fi

    # Check if image exists
    if ! podman images "${IMAGE_NAME}:${TAG}" --format '{{.ID}}' | grep -q .; then
        echo "ERROR: Image ${IMAGE_NAME}:${TAG} not found"
        echo "Run './export-import.sh import' or './export-import.sh load' first."
        exit 1
    fi

    podman run -it --rm \
        --name centos7-phi-dev \
        -v /opt/mpss/3.8.6:/opt/mpss/3.8.6:ro \
        -v /usr/linux-k1om-4.7:/usr/linux-k1om-4.7:ro \
        -v "$PROJECT_ROOT:/work:rw" \
        "${IMAGE_NAME}:${TAG}" \
        bash
}

# Main
case "${1:-}" in
    export)  cmd_export ;;
    import)  cmd_import ;;
    save)    cmd_save ;;
    load)    cmd_load ;;
    run)     cmd_run ;;
    *)       usage ;;
esac
