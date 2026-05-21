#!/bin/bash
# Build the Intel Phi development container from scratch.
# Requires PSXE 2016 installer mounted at ./psxe_install/

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-intel-phi-dev}"
TAG="${TAG:-latest}"

# Check prerequisites
echo "=== Checking prerequisites ==="

if [ ! -d "$PROJECT_ROOT/psxe_install" ]; then
    echo "ERROR: PSXE 2016 installer not found at $PROJECT_ROOT/psxe_install/"
    echo ""
    echo "Please place the Intel Parallel Studio XE 2016 installer there:"
    echo "  $PROJECT_ROOT/psxe_install/"
    echo "  ├── install.sh"
    echo "  ├── rpm/"
    echo "  └── ..."
    exit 1
fi

if [ ! -f "$PROJECT_ROOT/psxe_install/install.sh" ]; then
    echo "ERROR: $PROJECT_ROOT/psxe_install/install.sh not found"
    exit 1
fi

if [ ! -f "$PROJECT_ROOT/psxe_install/silent.cfg" ]; then
    echo "WARNING: silent.cfg not found. Creating a minimal one..."
    cat > "$PROJECT_ROOT/psxe_install/silent.cfg" << 'EOF'
# Minimal silent config for PSXE 2016 C++ compiler + TBB
ACCEPT_EULA=accept
CONTINUE_WITH_OPTIONAL_ERROR=yes
PSET_INSTALL_DIR=/opt/intel
CONTINUE_WITH_INSTALLDIR_OVERWRITE=yes
COMPONENTS=ALL
EOF
fi

echo "PSXE installer: OK"
echo "Image name:     $IMAGE_NAME:$TAG"
echo ""

# Step 1: Build base image
echo "=== Step 1: Building base image ==="
podman build -t "${IMAGE_NAME}-base:${TAG}" -f "$SCRIPT_DIR/Dockerfile" "$PROJECT_ROOT"

# Step 2: Run temporary container to install ICC
echo ""
echo "=== Step 2: Installing ICC 16.0 into container ==="
CONTAINER_ID=$(podman run -d \
    -v "$PROJECT_ROOT/psxe_install:/psxe_install:ro" \
    "${IMAGE_NAME}-base:${TAG}" \
    bash -c '
        cd /psxe_install
        ./install.sh --silent silent.cfg --user-mode --tmp-dir /tmp
        echo "ICC install exit: $?"
    ')

echo "Installer running in container: $CONTAINER_ID"
podman logs -f "$CONTAINER_ID"

# Step 3: Commit installed container
echo ""
echo "=== Step 3: Committing installed container ==="
podman commit -f docker "$CONTAINER_ID" "${IMAGE_NAME}:${TAG}"
podman rm "$CONTAINER_ID"

# Step 4: Install TBB RPMs
echo ""
echo "=== Step 4: Installing TBB 4.4 ==="
CONTAINER_ID=$(podman run -d \
    -v "$PROJECT_ROOT/psxe_install/rpm:/rpm:ro" \
    "${IMAGE_NAME}:${TAG}" \
    bash -c '
        cd /rpm
        rpm -ivh --force \
            intel-tbb-common-*.rpm \
            intel-tbb-libs-*.rpm \
            intel-tbb-ps-common-*.rpm \
            intel-tbb-devel-*.rpm \
            2>/dev/null || true
        echo "TBB install done"
    ')

podman logs "$CONTAINER_ID"
podman commit -f docker "$CONTAINER_ID" "${IMAGE_NAME}:${TAG}"
podman rm "$CONTAINER_ID"

# Step 5: Verify
echo ""
echo "=== Step 5: Verification ==="
podman run --rm "${IMAGE_NAME}:${TAG}" bash -c '
    source /opt/intel/bin/compilervars.sh intel64 2>/dev/null
    icc --version | head -1
    echo "TBB:"
    ls /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/lib/intel64_lin_mic/libtbb.so 2>/dev/null && echo "  MIC TBB OK" || echo "  MIC TBB MISSING"
'

echo ""
echo "=========================================="
echo "Build complete: ${IMAGE_NAME}:${TAG}"
echo ""
echo "To run:"
echo "  podman run -it --rm \\"
echo "    -v /opt/mpss/3.8.6:/opt/mpss/3.8.6:ro \\"
echo "    -v /usr/linux-k1om-4.7:/usr/linux-k1om-4.7:ro \\"
echo "    -v \$(pwd):/work:rw \\"
echo "    ${IMAGE_NAME}:${TAG}"
echo "=========================================="
