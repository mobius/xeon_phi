# Intel Xeon Phi 7120P (Knights Corner) Development

> Rocky Linux 8.10 cross-compilation + benchmarking environment for Xeon Phi 7120P

## Hardware

| Component | Specs |
|-----------|-------|
| Host | Xeon Gold 6252 x2, 192GB, Rocky 8.10 |
| Phi | Xeon Phi 7120P (KNC), 16GB, 61 cores |
| VE | NEC VE 1.0 x3 |

## Toolchain

| Compiler | Path | Capabilities |
|----------|------|---------------|
| ICC 16.0 | /opt/intel/bin/icc -mmic | intrinsics + auto-vec |
| GCC 5.1.1 (SDK) | /opt/mpss/.../k1om-mpss-linux-gcc | inline asm |
| GCC 5.1.1 (self-built) | ~/gcc-5.1.1-knc/install-container/bin/ | inline asm |

## Performance (SAXPY 32M, 61 threads)

| Method | BW | GFLOPS |
|--------|-----|--------|
| GCC x87 scalar | 7.9 GB/s | 0.66 |
| GCC inline asm | 26.1 GB/s | 2.18 |
| ICC intrinsics | 27.3 GB/s | 2.27 |

## Quick Start

```bash
# Enter container
podman exec -it centos7-phi-dev bash

# Setup
source /opt/mpss/3.8.6/environment-setup-k1om-mpss-linux
export PATH=/opt/intel/bin:/opt/mpss/3.8.6/sysroots/x86_64-mpsssdk-linux/usr/bin/k1om-mpss-linux:$PATH

# Build & deploy
icc -std=c99 -mmic -O3 -lpthread kernel.c bench.c -o prog.mic
scp prog.mic mic0:/tmp/ && ssh mic0 /tmp/prog.mic
```

## Structure

```
├── README.md
├── saxpy_kernel.c
├── saxpy_bench.c
├── matmul_bench.c
├── docs/
│   ├── icc-usage.md
│   ├── research/
│   ├── plan/
│   └── impl/
├── Xeon_Phi_7120P_Specific_Assessment.md
├── Xeon_Phi_Addon_Assessment.md
└── ESC4000G4_7120P_Final_Assessment.md
```

## References

- [Aidan Crowther Xeon Phi Blog](https://www.aidancrowther.com/project/xeonphi)
- [apc-llc/gcc-5.1.1-knc](https://github.com/apc-llc/gcc-5.1.1-knc)
- Dockerfile: https://www.aidancrowther.com/files/Dockerfile
