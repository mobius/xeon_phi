#!/bin/bash
# Build script for Xeon Phi 7120P Peak Performance Test Suite

set -e

echo "========================================"
echo "Building Xeon Phi 7120P Peak Test Suite"
echo "========================================"

# Check for ICC
if ! command -v icc &> /dev/null; then
    echo "ERROR: icc not found in PATH"
    echo "Please run:"
    echo "  source /opt/mpss/3.8.6/environment-setup-k1om-mpss-linux"
    echo "  export PATH=/opt/intel/bin:\$PATH"
    exit 1
fi

echo "Compiler: $(icc --version | head -1)"
echo ""

# Clean old builds
rm -f phi_stream_bench.mic phi_peak_fp64.mic phi_peak_fp32.mic phi_peak_dgemm.mic

# Compile each test
echo "[1/4] Building phi_stream_bench.mic ..."
icc -std=c99 -mmic -O3 -openmp -o phi_stream_bench.mic phi_stream_bench.c
echo "      OK"

echo "[2/4] Building phi_peak_fp64.mic ..."
icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp64.mic phi_peak_fp64.c
echo "      OK"

echo "[3/4] Building phi_peak_fp32.mic ..."
icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp32.mic phi_peak_fp32.c
echo "      OK"

echo "[4/4] Building phi_peak_dgemm.mic ..."
icc -std=c99 -mmic -O3 -openmp -o phi_peak_dgemm.mic phi_peak_dgemm.c
echo "      OK"

echo ""
echo "========================================"
echo "All builds successful!"
echo "========================================"
echo ""
echo "Deploy and run:"
echo "  scp phi_*.mic mic0:/tmp/"
echo '  ssh mic0 "cd /tmp && for f in phi_*.mic; do echo === Running \$f ===; ./\$f; done"'
