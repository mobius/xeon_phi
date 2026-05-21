# Intel Xeon Phi 7120P (KNC) Development Environment
# Based on CentOS 7 with ICC 16.0, TBB 4.4, and MPSS 3.8.6 bindings
#
# BUILD (requires PSXE 2016 installer mounted at /psxe_install):
#   podman build -t intel-phi-dev -f container/Dockerfile .
#
# RUN (requires host MPSS and k1om toolchain mounted):
#   podman run -it --rm \
#     -v /opt/mpss/3.8.6:/opt/mpss/3.8.6:ro \
#     -v /usr/linux-k1om-4.7:/usr/linux-k1om-4.7:ro \
#     -v $(pwd):/work:rw \
#     intel-phi-dev:latest

FROM quay.io/centos/centos:7

LABEL maintainer="phi-dev" \
      description="ICC 16.0 + TBB 4.4 + MPSS 3.8.6 dev env for Xeon Phi 7120P" \
      version="1.0"

# Prevent tzdata interactive prompt
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install base development tools
RUN yum update -y && \
    yum groupinstall -y "Development Tools" && \
    yum install -y \
        gcc gcc-c++ \
        gcc-gfortran \
        glibc-devel \
        glibc-headers \
        kernel-headers \
        make cmake \
        wget curl \
        vim nano \
        gdb strace \
        libstdc++-devel \
        libgcc \
        numactl-devel \
        && \
    yum clean all && \
    rm -rf /var/cache/yum

# Create mount points for host resources
# These will be bind-mounted at runtime
RUN mkdir -p /opt/mpss/3.8.6 /usr/linux-k1om-4.7 /work

# Note: ICC 16.0 installation is performed at build time if PSXE installer
# is available, or the image can be imported from a pre-built tar.
# See container/README.md for both approaches.

# --- Option A: Pre-installed ICC layer (for imported images) ---
# If /opt/intel already exists (from committed image), set up environment.
# This is a no-op for fresh builds but ensures compatibility.

# --- Option B: Install from mounted PSXE (for fresh builds) ---
# The build.sh script handles this by running install.sh inside the container.

# Set up default shell environment for ICC
# These will be active when the container starts
ENV INTEL_COMPILER_PATH=/opt/intel/compilers_and_libraries_2016.0.109/linux
ENV PATH=$INTEL_COMPILER_PATH/bin/intel64:$PATH
ENV LD_LIBRARY_PATH=$INTEL_COMPILER_PATH/compiler/lib/intel64:$LD_LIBRARY_PATH

# Default working directory
WORKDIR /work

# Start with bash
CMD ["/bin/bash"]
