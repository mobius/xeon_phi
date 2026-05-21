# Intel Phi Development Container

Pre-configured container image with ICC 16.0 + TBB 4.4 for Xeon Phi 7120P (KNC) development.

## Quick Start

### Option 1: Import Pre-Built Image (Fastest)

If you have the exported `intel-phi-dev.tar.gz`:

```bash
cd container
./export-import.sh import   # or ./export-import.sh load
./export-import.sh run
```

Inside the container:
```bash
source /opt/intel/bin/compilervars.sh intel64
icc --version              # ICC 16.0
icpc -std=c++11 -tbb ...   # TBB 4.4 available
```

### Option 2: Build from Scratch

Requires the PSXE 2016 installer in `../psxe_install/`:

```bash
cd container
./build.sh
```

This will:
1. Build CentOS 7 base image
2. Install ICC 16.0 from `psxe_install/`
3. Install TBB 4.4 RPMs
4. Tag as `intel-phi-dev:latest`

### Option 3: Save Current Container

Export your currently running container:

```bash
cd container
./export-import.sh export
# Produces: ../container/intel-phi-dev.tar.gz
```

## Container Layout

```
/opt/intel/                          # ICC 16.0 + TBB 4.4 (in image)
├── bin/compilervars.sh
├── compilers_and_libraries_2016.0.109/
│   ├── linux/bin/intel64/icc
│   ├── linux/compiler/lib/intel64_lin_mic/   # MIC runtime
│   └── linux/tbb/lib/intel64_lin_mic/        # MIC TBB

/opt/mpss/3.8.6/                     # Bind-mounted from host (ro)
/usr/linux-k1om-4.7/                 # K1OM cross-compiler (ro)
/work/                               # Project directory (rw)
```

## Host Requirements

| Host Path | Mount Point | Purpose |
|-----------|-------------|---------|
| `/opt/mpss/3.8.6` | `/opt/mpss/3.8.6` | MPSS libraries (MIC runtime) |
| `/usr/linux-k1om-4.7` | `/usr/linux-k1om-4.7` | K1OM cross-compiler toolchain |
| project root | `/work` | Source code and build outputs |

## ICC + TBB Usage Inside Container

```bash
# Setup environment
source /opt/intel/bin/compilervars.sh intel64
source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64

# Host binary with TBB
icpc -std=c++11 -tbb -o app app.cpp -ltbb

# MIC binary with TBB
icpc -std=c++11 -mmic -tbb -o app.mic app.cpp -ltbb

# MIC offload (TBB only on host side recommended)
icpc -std=c++11 -qoffload -tbb -o app app.cpp -ltbb
```

## Export File Details

| Property | Value |
|----------|-------|
| Format | Docker image tar.gz |
| Size | ~3-5 GB compressed (varies) |
| Contents | CentOS 7 + ICC 16.0 + TBB 4.4 + dev tools |
| Excludes | MPSS, K1OM toolchain (bind-mounted at runtime) |

## License Notice

The exported image contains Intel proprietary software (ICC, TBB, MKL). It is intended for personal/organizational use only and must not be redistributed publicly. The `Dockerfile` and `build.sh` script do not contain Intel binaries; they only document the installation process.
