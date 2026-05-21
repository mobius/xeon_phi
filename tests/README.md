# Xeon Phi Best-Practice Test Suite

This directory contains reproducible tests for the ICC/GCC coexistence
and offload best practices documented in the project.

## Quick Start

```bash
# Build all tests (ICC runs inside podman container)
make -C tests all

# Run all tests
make -C tests test
```

## Test Inventory

### 1. ORSL Multi-Process Offload (`orsl_multi_proc/`)
**Purpose**: Verify multiple host processes can concurrently offload
to mic0, with and without ORSL enabled.

**Key finding**: ORSL is not required in single-card environments.

```bash
cd tests/orsl_multi_proc
bash run_orsl_test.sh
```

### 2. ICC/GCC Mixed Compilation (`icc_gcc_compat/`)
**Purpose**: Verify GCC-compiled OpenMP objects link correctly into
ICC binaries via `libiomp5.so`.

**Key finding**: Always link with ICC (`libiomp5.so` provides GCC ABI
compatibility). GCC linking ICC objects fails (missing `__kmpc_*`).

```bash
cd tests/icc_gcc_compat
make test
```

### 3. OpenMP Dual-Library Conflict (`openmp_dual_lib/`)
**Purpose**: Demonstrate the conflict when both `libiomp5.so` and
`libgomp.so` are loaded.

**Key finding**: Never link both simultaneously. Use `libiomp5.so` only.

```bash
cd tests/openmp_dual_lib
bash run_dual_omp_test.sh
```

### 4. C++ Cross-Compiler Exception ABI (`cpp_cross_abi/`)
**Purpose**: Verify exceptions thrown by GCC-compiled C++ code can be
caught by ICC-compiled code.

**Key finding**: C++ exception ABI is compatible between ICC 16.0 and
GCC 4.8.5 on CentOS 7.

```bash
cd tests/cpp_cross_abi
make test
```

### 5. MIC Library Path Verification (`mic_ldpath_verify/`)
**Purpose**: Verify offload works when `MIC_LD_LIBRARY_PATH` points to
ICC MIC libraries copied from the container to a host-visible directory.

**Key finding**: Host cannot access `/opt/intel` inside the container;
libraries must be copied out and `MIC_LD_LIBRARY_PATH` set accordingly.

```bash
cd tests/mic_ldpath_verify
bash run_mic_ldpath_test.sh
```

## Environment Prerequisites

| Requirement | Details |
|-------------|---------|
| Container | `centos7-phi-dev` with ICC 16.0 |
| MIC device | `mic0` online (`micctrl --status`) |
| MIC libraries | Copied to `../icc_mic_libs/` (see `run_mic_ldpath_test.sh` for copy commands) |
| `MIC_LD_LIBRARY_PATH` | Points to host-visible ICC MIC library directory |

## Container-to-Host Library Copy

Since ICC lives inside a podman container, its MIC libraries are not
visible on the host. Copy them before running offload tests:

```bash
mkdir -p icc_mic_libs
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/liboffload.so.5  icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libiomp5.so      icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libcilkrts.so.5  icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libimf.so         icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libsvml.so        icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libintlc.so.5      icc_mic_libs/
podman cp centos7-phi-dev:/opt/intel/compilers_and_libraries_2016.0.109/linux/compiler/lib/intel64_lin_mic/libirng.so         icc_mic_libs/

export MIC_LD_LIBRARY_PATH="$(pwd)/icc_mic_libs"
```
