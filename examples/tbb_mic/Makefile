# Makefile for TBB on MIC examples
# 
# 使用说明:
#   1. 进入 ICC 容器并 source 环境变量:
#      podman exec -it centos7-phi-dev bash
#      source /opt/intel/bin/compilervars.sh intel64
#      source /opt/intel/compilers_and_libraries_2016.0.109/linux/tbb/bin/tbbvars.sh intel64
#      cd /path/to/examples/tbb_mic
#
#   2. 编译所有示例:
#      make all
#
#   3. 运行 MIC 原生示例:
#      make run-mic      (通过 micnativeloadex)
#
#   4. 运行 Host 混合示例:
#      make run-host
#
#   5. 清理:
#      make clean

# 编译器设置
ICPC      = icpc
MMIC      = -mmic
QOFFLOAD  = -qoffload

# 通用选项
CXXFLAGS  = -std=c++11 -O2 -Wall
TBBFLAGS  = -tbb
OMPFLAGS  = -openmp

# MIC 运行时库路径 (用于 micnativeloadex)
MIC_LD_PATH = /home/joey/Work/intel_phi/icc_mic_libs

# ============================================
# Targets
# ============================================

.PHONY: all clean run-mic run-host run-all

all: example1_parallel_for.mic \
     example2_parallel_reduce.mic \
     example3_host_tbb_mic_omp

# ---- MIC 原生 TBB 示例 (纯 MIC 二进制) ----

example1_parallel_for.mic: example1_parallel_for.cpp
	$(ICPC) $(MMIC) $(CXXFLAGS) $(TBBFLAGS) -o $@ $<

example2_parallel_reduce.mic: example2_parallel_reduce.cpp
	$(ICPC) $(MMIC) $(CXXFLAGS) $(TBBFLAGS) -o $@ $<

# ---- Host + MIC 混合示例 (Offload) ----

example3_host_tbb_mic_omp: example3_host_tbb_mic_omp.cpp
	$(ICPC) $(QOFFLOAD) $(CXXFLAGS) $(TBBFLAGS) $(OMPFLAGS) -o $@ $<

# ============================================
# 运行
# ============================================

run-mic: example1_parallel_for.mic example2_parallel_reduce.mic
	@echo "========================================"
	@echo "Running example1_parallel_for.mic"
	@echo "========================================"
	MIC_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./example1_parallel_for.mic
	@echo ""
	@echo "========================================"
	@echo "Running example2_parallel_reduce.mic"
	@echo "========================================"
	MIC_LD_LIBRARY_PATH=$(MIC_LD_PATH) micnativeloadex ./example2_parallel_reduce.mic

run-host: example3_host_tbb_mic_omp
	@echo "========================================"
	@echo "Running example3_host_tbb_mic_omp"
	@echo "========================================"
	MIC_LD_LIBRARY_PATH=$(MIC_LD_PATH) ./example3_host_tbb_mic_omp

run-all: run-mic run-host

# ============================================
# 清理
# ============================================

clean:
	rm -f example1_parallel_for.mic \
	      example2_parallel_reduce.mic \
	      example3_host_tbb_mic_omp
