#!/bin/bash
# MIC Library Path Verification Test Runner
#
# Verifies that MIC_LD_LIBRARY_PATH points to valid ICC MIC libraries
# copied from the container to a host-accessible directory.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MIC_LIBS="${PROJECT_ROOT}/icc_mic_libs"
BINARY="${SCRIPT_DIR}/test_mic_ldpath"

echo "=========================================="
echo "MIC Library Path Verification Test"
echo "=========================================="
echo ""

# Check MIC library directory
if [ ! -d "$MIC_LIBS" ]; then
    echo "Error: MIC library directory not found: $MIC_LIBS"
    echo ""
    echo "To create it, run from project root:"
    echo "  mkdir -p icc_mic_libs"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/liboffload.so.5 icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libiomp5.so      icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libcilkrts.so.5  icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libimf.so         icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libsvml.so        icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libintlc.so.5      icc_mic_libs/"
    echo "  podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libirng.so         icc_mic_libs/"
    exit 1
fi

echo "MIC libraries found in: $MIC_LIBS"
ls -la "$MIC_LIBS"
echo ""

# Check binary
if [ ! -x "$BINARY" ]; then
    echo "Error: Binary not found: $BINARY"
    echo "Compile with:"
    echo "  podman exec centos7-phi-dev bash -c 'source /opt/intel/bin/compilervars.sh intel64; icc -qoffload -o /work/tests/mic_ldpath_verify/test_mic_ldpath /work/tests/mic_ldpath_verify/simple_offload.c'"
    exit 1
fi

# Run test
export MIC_LD_LIBRARY_PATH="$MIC_LIBS"
echo "MIC_LD_LIBRARY_PATH=$MIC_LD_LIBRARY_PATH"
echo ""
echo "--- Running offload test ---"
"$BINARY"

RET=$?
if [ $RET -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "MIC library path verification PASSED"
    echo "=========================================="
else
    echo ""
    echo "=========================================="
    echo "MIC library path verification FAILED (exit=$RET)"
    echo "=========================================="
    exit 1
fi
