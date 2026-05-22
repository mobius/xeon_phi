# Intel Xeon Phi 7120P (Knights Corner) Development

> Rocky Linux 8.10 + MPSS 3.8.6 + ICC 16.0 offload environment for Xeon Phi 7120P

## Hardware

| Component | Specs |
|-----------|-------|
| Host | Xeon Gold 6252 x2, 192GB DDR4, Rocky Linux 8.10 |
| Phi | Xeon Phi 7120P (KNC), 16GB GDDR5, 61 cores @ 1.238 GHz |
| Threads | 244 (4-way SMT) |
| Peak FP64 | ~1.208 TFLOPS |
| Peak FP32 | ~2.416 TFLOPS |
| Memory BW | ~352 GB/s |
| PCIe | Gen3 x16 |

## Environment Status

| Component | Version | Status |
|-----------|---------|--------|
| MPSS | 3.8.6 | ✅ Active (`mpssd` running, mic0 online) |
| ICC | 16.0.0 | ✅ Available in podman container |
| COI library | 3.8.6 | ✅ Device enumeration works (count=1) |
| OpenMP offload | ICC 16.0 | ✅ Verified (result=42, vector add correct) |
| Intel LEO offload | ICC 16.0 | ✅ Verified |
| micnativeloadex | 3.8.6 | ✅ Verified (561 GFLOPS FP64) |
| Intel TBB 4.4 | ICC 16.0 | ✅ Installed & verified (MIC native) |
| Intel MPI | 5.1 | ✅ Available (MIC cross-compile verified) |
| liboffloadmic | GCC 5.3.0 | ❌ Infeasible (emulator-only, GCC-specific) |

## Verified Programming Models

### 1. OpenMP Offload (Recommended)

```c
#pragma omp target map(to: A[0:N], B[0:N]) map(from: C[0:N])
{
    for (int i = 0; i < N; i++) C[i] = A[i] + B[i];
}
```

Compile in container, run on host:
```bash
podman exec centos7-phi-dev bash -c '
    source /opt/intel/bin/compilervars.sh intel64
    icc -qopenmp -qoffload=optional -o myapp myapp.c
'
podman cp centos7-phi-dev:/tmp/myapp ./myapp

export MIC_LD_LIBRARY_PATH="/path/to/intel64_lin_mic/libs"
export OFFLOAD_ENABLE_ORSL=0
./myapp
```

See [Xeon Phi Offload Guide](docs/impl/20260520_231500_xeon_phi_offload_guide.md) for full details.

### 2. Manual KNC Intrinsic (Peak Performance)

```c
#include <immintrin.h>
__m512d va = _mm512_set1_pd(1.0);
__m512d vb = _mm512_set1_pd(2.0);
__m512d vc = _mm512_fmadd_pd(va, vb, vc);
```

Compile and deploy:
```bash
icc -std=c99 -mmic -O3 -openmp -o prog.mic prog.c
scp prog.mic mic0:/tmp/
ssh mic0 /tmp/prog.mic
```

### 3. micnativeloadex (Official Tool)

```bash
micnativeloadex prog.mic -d 0 -t 60
```

### 4. MIC Native Parallel Models (6 verified)

All compiled with `-mmic` and executed on mic0 via `micnativeloadex`:

| Model | File | Threads/Ranks | Result |
|-------|------|--------------|--------|
| POSIX Threads | `examples/mic_parallel/01_pthreads.c` | 244 | ✅ 0.53s, PASSED |
| OpenMP | `examples/mic_parallel/02_openmp.c` | 240 auto | ✅ 0.26s, PASSED |
| Intel Cilk Plus | `examples/mic_parallel/03_cilkplus.cpp` | work-stealing | ✅ PASSED |
| Intel TBB | `examples/mic_parallel/04_tbb.cpp` | work-stealing | ✅ PASSED |
| Intel MKL | `examples/mic_parallel/05_mkl.cpp` | 240 auto | ✅ 45.7 GFLOPS, PASSED |
| Intel MPI | `examples/mic_parallel/06_mpi.c` | 1+ rank | ✅ single-rank PASSED |

See [examples/mic_parallel/README.md](examples/mic_parallel/README.md) for build instructions.

### 5. Host TBB + MIC OpenMP Hybrid

Host uses TBB for data prep, MIC uses OpenMP for compute via `#pragma offload`:

```cpp
// Host: TBB
 tbb::parallel_for(...);

// MIC: OpenMP (TBB cannot be used inside offload blocks)
#pragma offload target(mic) in(a[0:N]) out(c[0:N])
{
    #pragma omp parallel for
    for (int i = 0; i < N; i++) c[i] = ...;
}
```

See [examples/tbb_mic/README.md](examples/tbb_mic/README.md) for full details.

## Peak Performance Benchmarks

| Test | Measured | Theory | Efficiency |
|------|----------|--------|------------|
| FP64 FMA | **575 GFLOPS** | 1,208 GFLOPS | 47.6% |
| FP32 FMA | **1,170 GFLOPS** | 2,416 GFLOPS | 48.4% |
| STREAM Copy | **157 GB/s** | 352 GB/s | 44.7% |
| DGEMM 2048 | **63 GFLOPS** | 1,208 GFLOPS | 5.2% |
| MKL DGEMM 2000 (MIC native) | **45.7 GFLOPS** | 1,208 GFLOPS | 3.8% |

Source: `phi_peak_fp64.c`, `phi_peak_fp32.c`, `phi_stream_bench.c`, `phi_peak_dgemm.c`, `examples/mic_parallel/05_mkl.cpp`

## Project Structure

```
├── README.md                          # This file
├── .gitignore                         # Excludes *.mic binaries, mpss tarballs
│
├── phi_peak_fp64.c                    # FP64 FMA peak benchmark
├── phi_peak_fp32.c                    # FP32 FMA peak benchmark
├── phi_peak_dgemm.c                   # DGEMM baseline benchmark
├── phi_stream_bench.c                 # STREAM bandwidth benchmark
├── Makefile.peak                      # Build automation for benchmarks
├── build_peak_tests.sh                # Build script
│
├── saxpy_bench.c                      # Early SAXPY experiment (historical)
├── saxpy_kernel.c                     # SAXPY kernel (historical)
├── matmul_bench.c                     # Early matmul experiment (historical)
│
├── docs/
│   ├── research/
│   │   ├── 20260520_214838_liboffloadmic_detailed_assessment.md
│   │   ├── 20260520_071200_liboffloadmic_assessment.md
│   │   ├── 20260520_052053_peak_performance_testing_theory.md
│   │   ├── ESC4000G4_7120P_Final_Assessment.md
│   │   ├── Xeon_Phi_7120P_Specific_Assessment.md
│   │   ├── Xeon_Phi_Addon_Assessment.md
│   │   └── 20260522_020600_tbb_offload_inlining_bug.md  # TBB+offload 不可行根因
│   ├── impl/
│   │   ├── 20260520_231500_xeon_phi_offload_guide.md    # ⭐ Main guide
│   │   ├── 20260520_052053_peak_performance_implementation.md
│   │   └── 20260520_055400_peak_performance_verification.md
│   └── plan/
│       └── 20260520_052053_peak_performance_test_plan.md
│
├── examples/                          # Verified runnable examples
│   ├── mic_parallel/                  # 6 parallel programming models on MIC
│   │   ├── 01_pthreads.c              # POSIX Threads
│   │   ├── 02_openmp.c                # OpenMP
│   │   ├── 03_cilkplus.cpp            # Intel Cilk Plus
│   │   ├── 04_tbb.cpp                 # Intel TBB
│   │   ├── 05_mkl.cpp                 # Intel MKL auto-threading
│   │   ├── 06_mpi.c                   # Intel MPI
│   │   ├── Makefile                   # Build & run all examples
│   │   └── README.md                  # Usage guide for all 6 models
│   └── tbb_mic/                       # TBB on MIC usage guide
│       ├── example1_parallel_for.cpp
│       ├── example2_parallel_reduce.cpp
│       ├── example3_host_tbb_mic_omp.cpp
│       ├── Makefile
│       └── README.md
│
├── tests/                             # Best-practice verification tests
│   ├── orsl_multi_proc/               # ORSL multi-process offload test
│   ├── icc_gcc_compat/                # ICC/GCC mixed compilation test
│   ├── openmp_dual_lib/               # OpenMP dual-library conflict demo
│   ├── cpp_cross_abi/                 # C++ cross-compiler exception test
│   ├── mic_ldpath_verify/             # MIC library path verification
│   └── Makefile                       # Build & run all tests
│
├── psxe_install/                      # ICC 16.0 installer (gitignored)
├── mpss-3.8.6-linux.tar               # MPSS 3.8.6 distribution (gitignored)
└── mpss-src-3.8.6.tar                 # MPSS 3.8.6 source (gitignored)
```

## Best-Practice Test Suite

Reproducible tests for ICC/GCC coexistence and offload behavior:

```bash
cd tests
make all    # Build all tests (ICC via podman)
make test   # Run all tests
```

| Test | What it verifies |
|------|------------------|
| `orsl_multi_proc` | Multi-process offload works without ORSL on single-card setup |
| `icc_gcc_compat` | GCC OpenMP objects link correctly into ICC binaries via `libiomp5.so` |
| `openmp_dual_lib` | Linking both `libiomp5.so` and `libgomp.so` causes runtime conflicts |
| `cpp_cross_abi` | C++ exceptions thrown by GCC code are catchable by ICC code |
| `mic_ldpath_verify` | Offload requires `MIC_LD_LIBRARY_PATH` pointing to host-visible ICC MIC libs |


## Container Environment

A pre-configured container with ICC 16.0 + TBB 4.4 is available:

```bash
cd container
./export-import.sh import   # Import from tar.gz
./export-import.sh run      # Start dev environment
```

Or build from scratch (requires PSXE 2016 installer):

```bash
cd container
./build.sh
```

See [container/README.md](container/README.md) for details.
## Key Findings

1. **MPSS 3.8.6 works on Rocky 8.10** — but requires installing `mpss-core`, `mpss-offload`, `libscif0`, `libmicmgmt0` in addition to the basic packages.
2. **COI library is functional** — device enumeration works after complete MPSS installation.
3. **OpenMP offload is the recommended path** — ICC 16.0 supports `#pragma omp target` with full MIC offload.
4. **liboffloadmic is infeasible** — project only implements emulator, hardcoded GCC-specific syntax, no native COI option.
5. **ORSL is not required for single-card setups** — `OFFLOAD_ENABLE_ORSL=1` has no benefit with one 7120P.
6. **Never link both OpenMP libraries** — use `libiomp5.so` only; it provides GCC ABI compatibility.
7. **Container ICC needs MIC lib copy-out** — host cannot see `/opt/intel` inside podman; copy `intel64_lin_mic/` libs to host and set `MIC_LD_LIBRARY_PATH`.
8. **TBB + `#pragma offload` is infeasible on KNC** — ICC 16.0 MIC compiler fails to inline class ctors/dtors in `target(mic)` code, causing link errors for TBB symbols. TBB can only run on MIC via pure native (`-mmic`) binaries, not inside offload blocks.
9. **ICC 16.0 MIC compiler does not support C++11 lambdas** — use functor classes instead of `[&](...){...}` when compiling with `-mmic` for TBB or Cilk Plus code.

## Quick Reference

| Task | Command |
|------|---------|
| Check MPSS status | `systemctl status mpss` |
| Check Phi device | `micinfo` / `micctrl --status` |
| Enter build container | `podman exec -it centos7-phi-dev bash` |
| Setup ICC env | `source /opt/intel/bin/compilervars.sh intel64` |
| Compile for MIC | `icc -mmic -O3 -openmp ...` |
| Compile OpenMP offload | `icc -qopenmp -qoffload=optional ...` |
| Run via micnativeloadex | `micnativeloadex prog.mic -d 0 -t 60` |
| Run on mic0 via SSH | `ssh mic0 /tmp/prog.mic` |
| Run MIC parallel examples | `cd examples/mic_parallel && make all && make run-all` |
| Run TBB examples | `cd examples/tbb_mic && make all && make run-mic` |

## License

Project-specific code is provided as-is for research and benchmarking purposes.
Intel tools (ICC, MPSS) are subject to their respective licenses.
