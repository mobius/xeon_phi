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

## Peak Performance Benchmarks

| Test | Measured | Theory | Efficiency |
|------|----------|--------|------------|
| FP64 FMA | **575 GFLOPS** | 1,208 GFLOPS | 47.6% |
| FP32 FMA | **1,170 GFLOPS** | 2,416 GFLOPS | 48.4% |
| STREAM Copy | **157 GB/s** | 352 GB/s | 44.7% |
| DGEMM 2048 | **63 GFLOPS** | 1,208 GFLOPS | 5.2% |

Source: `phi_peak_fp64.c`, `phi_peak_fp32.c`, `phi_stream_bench.c`, `phi_peak_dgemm.c`

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
│   │   └── Xeon_Phi_Addon_Assessment.md
│   ├── impl/
│   │   ├── 20260520_231500_xeon_phi_offload_guide.md    # ⭐ Main guide
│   │   ├── 20260520_052053_peak_performance_implementation.md
│   │   └── 20260520_055400_peak_performance_verification.md
│   └── plan/
│       └── 20260520_052053_peak_performance_test_plan.md
│
├── psxe_install/                      # ICC 16.0 installer (gitignored)
├── mpss-3.8.6-linux.tar               # MPSS 3.8.6 distribution (gitignored)
└── mpss-src-3.8.6.tar                 # MPSS 3.8.6 source (gitignored)
```

## Key Findings

1. **MPSS 3.8.6 works on Rocky 8.10** — but requires installing `mpss-core`, `mpss-offload`, `libscif0`, `libmicmgmt0` in addition to the basic packages.
2. **COI library is functional** — device enumeration works after complete MPSS installation.
3. **OpenMP offload is the recommended path** — ICC 16.0 supports `#pragma omp target` with full MIC offload.
4. **liboffloadmic is infeasible** — project only implements emulator, hardcoded GCC-specific syntax, no native COI option.

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

## License

Project-specific code is provided as-is for research and benchmarking purposes.
Intel tools (ICC, MPSS) are subject to their respective licenses.
