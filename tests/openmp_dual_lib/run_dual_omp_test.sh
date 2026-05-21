#!/bin/bash
# OpenMP Dual Library Conflict Test Runner
#
# Compares single-library vs dual-library linking behavior.

set -e

echo "=========================================="
echo "OpenMP Dual Library Conflict Test"
echo "=========================================="
echo ""

# Test 1: libiomp5.so only (correct)
echo "--- Test 1: icc -qopenmp (libiomp5.so only) ---"
echo "Linked libraries:"
ldd test_iomp5_only | grep -E "iomp5|gomp" || true
echo "Run:"
./test_iomp5_only
echo ""

# Test 2: libiomp5.so + libgomp (conflict demonstration)
echo "--- Test 2: icc -qopenmp -lgomp (BOTH libraries) ---"
echo "Linked libraries:"
ldd test_dual_lib | grep -E "iomp5|gomp" || true
echo "Run:"
./test_dual_lib
echo ""

echo "=========================================="
echo "Observation: Test 2 shows both threads as"
echo "'Thread 0' because libgomp overrides"
echo "libiomp5's thread state."
echo "=========================================="
