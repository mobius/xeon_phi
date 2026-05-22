# Makefile for MIC Parallel Examples
#
# 使用说明:
#   1. 进入 ICC 容器:
#      podman exec -it centos7-phi-dev bash
#      source /opt/intel/bin/compilervars.sh intel64
#      source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64
#
#   2. 编译所有示例:
#      make all
#
#   3. 运行单个示例:
#      make run-pthreads
#      make run-openmp
#      make run-cilkplus
#      make run-tbb
#      make run-mkl
#      make run-mpi
#
#   4. 运行所有示例:
#      make run-all
#
#   5. 清理:
#      make clean

ICPC      = icpc
ICC       = icc
MPICC     = mpiicc
MMIC      = -mmic
CXXFLAGS  = -std=c++11 -O2 -Wall
CFLAGS    = -O2 -Wall

# MIC 运行时库路径
MIC_LD_PATH = /home/joey/Work/intel_phi/icc_mic_libs

.PHONY: all clean run-all \
        run-pthreads run-openmp run-cilkplus run-tbb run-mkl run-mpi

all: 01_pthreads.mic 02_openmp.mic 03_cilkplus.mic 04_tbb.mic 05_mkl.mic 06_mpi.mic

# ============================================
# 01: POSIX Threads
# ============================================
01_pthreads.mic: 01_pthreads.c
	$(ICC) $(MMIC) $(CFLAGS) -o $@ $< -lpthread

run-pthreads: 01_pthreads.mic
	SINK_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./01_pthreads.mic

# ============================================
# 02: OpenMP
# ============================================
02_openmp.mic: 02_openmp.c
	$(ICC) $(MMIC) $(CFLAGS) -openmp -o $@ $<

run-openmp: 02_openmp.mic
	SINK_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./02_openmp.mic

# ============================================
# 03: Intel Cilk Plus
# ============================================
03_cilkplus.mic: 03_cilkplus.cpp
	$(ICPC) $(MMIC) $(CXXFLAGS) -o $@ $<

run-cilkplus: 03_cilkplus.mic
	SINK_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./03_cilkplus.mic

# ============================================
# 04: Intel TBB
# ============================================
04_tbb.mic: 04_tbb.cpp
	$(ICPC) $(MMIC) $(CXXFLAGS) -tbb -o $@ $<

run-tbb: 04_tbb.mic
	SINK_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./04_tbb.mic

# ============================================
# 05: Intel MKL (threaded mode)
# ============================================
05_mkl.mic: 05_mkl.cpp
	$(ICPC) $(MMIC) $(CXXFLAGS) -o $@ $< \
		-L/opt/intel/compilers_and_libraries_2016.0.109/linux/mkl/lib/mic \
		-lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5

run-mkl: 05_mkl.mic
	SINK_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./05_mkl.mic

# ============================================
# 06: Intel MPI
# ============================================
06_mpi.mic: 06_mpi.c
	$(MPICC) $(MMIC) $(CFLAGS) -o $@ $<

run-mpi: 06_mpi.mic
	@echo "MPI 运行方式:"
	@echo "  方式1: mpirun -host mic0 -np 4 ./06_mpi.mic"
	@echo "  方式2: scp 06_mpi.mic mic0:/tmp/ && ssh mic0 \"mpirun -np 4 /tmp/06_mpi.mic\""
	@echo "注意: 需要确保 MIC 端有 MPI runtime 库"

# ============================================
# Run all
# ============================================

run-all: run-pthreads run-openmp run-cilkplus run-tbb run-mkl

# ============================================
# Clean
# ============================================

clean:
	rm -f *.mic
