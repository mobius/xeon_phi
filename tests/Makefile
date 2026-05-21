# Xeon Phi Best-Practice Test Suite Makefile
#
# Build approach: copy sources into container /tmp, compile there,
# then copy binaries back to host. This avoids SELinux permission
# issues with the /work bind mount inside the container.
#
# Prerequisites:
#   - podman container 'centos7-phi-dev' with ICC 16.0 installed
#   - MIC_LD_LIBRARY_PATH pointing to host-visible ICC MIC libraries
#     (see ../icc_mic_libs/ or copy from container)

CONTAINER = centos7-phi-dev
ICC_VARS  = source /opt/intel/bin/compilervars.sh intel64 2>/dev/null

# Helper: compile C source inside container and copy binary back
# Usage: $(call ICC_BUILD,host_src_path,host_bin_path,extra_flags)
define ICC_BUILD
	podman cp $(1) $(CONTAINER):/tmp/$(notdir $(1))
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icc $(3) -o /tmp/$(notdir $(2)) /tmp/$(notdir $(1))'
	podman cp $(CONTAINER):/tmp/$(notdir $(2)) $(2)
	chmod +x $(2)
endef

# Helper: compile C++ source inside container and copy binary back
define ICC_BUILD_CPP
	podman cp $(1) $(CONTAINER):/tmp/$(notdir $(1))
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icpc $(3) -o /tmp/$(notdir $(2)) /tmp/$(notdir $(1))'
	podman cp $(CONTAINER):/tmp/$(notdir $(2)) $(2)
	chmod +x $(2)
endef

.PHONY: all orsl icc-gcc dual-omp cpp-cross mic-ldpath clean test

all: orsl icc-gcc dual-omp cpp-cross mic-ldpath

# --- ORSL multi-process offload test ---
orsl: orsl_multi_proc/test_orsl_multi

orsl_multi_proc/test_orsl_multi: orsl_multi_proc/test_orsl_multi.c
	$(call ICC_BUILD,$<,$@,-qoffload)

# --- ICC/GCC mixed compilation test ---
icc-gcc: icc_gcc_compat/test_icc_gcc_mix

icc_gcc_compat/icc_gcc_func.o: icc_gcc_compat/icc_gcc_func.c
	podman cp $< $(CONTAINER):/tmp/$(notdir $<)
	podman exec $(CONTAINER) bash -c 'gcc -fopenmp -c -o /tmp/$(notdir $@) /tmp/$(notdir $<)'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@

icc_gcc_compat/icc_main.o: icc_gcc_compat/icc_main.c
	podman cp $< $(CONTAINER):/tmp/$(notdir $<)
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icc -qopenmp -c -o /tmp/$(notdir $@) /tmp/$(notdir $<)'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@

icc_gcc_compat/test_icc_gcc_mix: icc_gcc_compat/icc_main.o icc_gcc_compat/icc_gcc_func.o
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icc -qopenmp -o /tmp/$(notdir $@) /tmp/icc_main.o /tmp/icc_gcc_func.o'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@
	chmod +x $@

# --- OpenMP dual-library conflict test ---
dual-omp: openmp_dual_lib/test_dual_lib openmp_dual_lib/test_iomp5_only

openmp_dual_lib/test_dual_lib: openmp_dual_lib/dual_omp.c
	$(call ICC_BUILD,$<,$@,-qopenmp -lgomp)

openmp_dual_lib/test_iomp5_only: openmp_dual_lib/dual_omp.c
	$(call ICC_BUILD,$<,$@,-qopenmp)

# --- C++ cross-compiler exception ABI test ---
cpp-cross: cpp_cross_abi/test_cpp_cross

cpp_cross_abi/gcc_throw.o: cpp_cross_abi/gcc_throw.cpp
	podman cp $< $(CONTAINER):/tmp/$(notdir $<)
	podman exec $(CONTAINER) bash -c 'g++ -c -o /tmp/$(notdir $@) /tmp/$(notdir $<)'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@

cpp_cross_abi/icc_catch.o: cpp_cross_abi/icc_catch.cpp
	podman cp $< $(CONTAINER):/tmp/$(notdir $<)
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icpc -c -o /tmp/$(notdir $@) /tmp/$(notdir $<)'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@

cpp_cross_abi/test_cpp_cross: cpp_cross_abi/gcc_throw.o cpp_cross_abi/icc_catch.o
	podman exec $(CONTAINER) bash -c '$(ICC_VARS); icpc -o /tmp/$(notdir $@) /tmp/gcc_throw.o /tmp/icc_catch.o'
	podman cp $(CONTAINER):/tmp/$(notdir $@) $@
	chmod +x $@

# --- MIC library path verification ---
mic-ldpath: mic_ldpath_verify/test_mic_ldpath

mic_ldpath_verify/test_mic_ldpath: mic_ldpath_verify/simple_offload.c
	$(call ICC_BUILD,$<,$@,-qoffload)

# --- Run all tests ---
test: all
	@echo ""
	@echo "=========================================="
	@echo "Running ORSL multi-process test"
	@echo "=========================================="
	cd orsl_multi_proc && bash run_orsl_test.sh
	@echo ""
	@echo "=========================================="
	@echo "Running ICC/GCC mixed compilation test"
	@echo "=========================================="
	cd icc_gcc_compat && ./test_icc_gcc_mix
	@echo ""
	@echo "=========================================="
	@echo "Running OpenMP dual-library conflict test"
	@echo "=========================================="
	cd openmp_dual_lib && bash run_dual_omp_test.sh
	@echo ""
	@echo "=========================================="
	@echo "Running C++ cross-ABI exception test"
	@echo "=========================================="
	cd cpp_cross_abi && ./test_cpp_cross
	@echo ""
	@echo "=========================================="
	@echo "Running MIC library path verification"
	@echo "=========================================="
	cd mic_ldpath_verify && bash run_mic_ldpath_test.sh

clean:
	rm -f orsl_multi_proc/test_orsl_multi
	rm -f icc_gcc_compat/*.o icc_gcc_compat/test_icc_gcc_mix
	rm -f openmp_dual_lib/test_dual_lib openmp_dual_lib/test_iomp5_only
	rm -f cpp_cross_abi/*.o cpp_cross_abi/test_cpp_cross
	rm -f mic_ldpath_verify/test_mic_ldpath
