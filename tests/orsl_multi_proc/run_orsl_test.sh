#!/bin/bash
# ORSL Multi-Process Offload Test Runner
#
# Tests concurrent offload with OFFLOAD_ENABLE_ORSL={0,1}
# Requires MIC_LD_LIBRARY_PATH to point to ICC MIC libraries on host.

set -e

BINARY="./test_orsl_multi"
NPROC=4

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found. Compile with:"
    echo "  podman exec centos7-phi-dev bash -c 'source /opt/intel/bin/compilervars.sh intel64; icc -qoffload -o /work/tests/orsl_multi_proc/test_orsl_multi /work/tests/orsl_multi_proc/test_orsl_multi.c'"
    exit 1
fi

# Ensure MIC_LD_LIBRARY_PATH is set
if [ -z "$MIC_LD_LIBRARY_PATH" ]; then
    echo "Warning: MIC_LD_LIBRARY_PATH not set. Trying default..."
    export MIC_LD_LIBRARY_PATH="$(dirname $0)/../../icc_mic_libs"
fi

echo "=========================================="
echo "ORSL Multi-Process Offload Test"
echo "MIC_LD_LIBRARY_PATH=$MIC_LD_LIBRARY_PATH"
echo "=========================================="
echo ""

# Test 1: Without ORSL (default)
echo "--- Test 1: OFFLOAD_ENABLE_ORSL=0 (default) ---"
for i in $(seq 0 $((NPROC-1))); do
    OFFLOAD_ENABLE_ORSL=0 "$BINARY" $i &
    pids[$i]=$!
done
for i in $(seq 0 $((NPROC-1))); do
    wait ${pids[$i]}
done
echo "Test 1 PASSED"
echo ""

# Test 2: With ORSL enabled
echo "--- Test 2: OFFLOAD_ENABLE_ORSL=1 ---"
for i in $(seq 0 $((NPROC-1))); do
    OFFLOAD_ENABLE_ORSL=1 "$BINARY" $i &
    pids[$i]=$!
done
for i in $(seq 0 $((NPROC-1))); do
    wait ${pids[$i]}
done
echo "Test 2 PASSED"
echo ""

echo "=========================================="
echo "All ORSL tests passed successfully."
echo "Conclusion: ORSL is not required in"
echo "single-card environments."
echo "=========================================="
