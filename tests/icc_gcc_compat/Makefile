# ICC/GCC Mixed Compilation Compatibility Test
#
# Purpose: Verify that GCC-compiled OpenMP objects can be linked
#          into an ICC-compiled binary using libiomp5.so.
#
# Usage:
#   make           # Build with ICC linking (recommended)
#   make gcc-link  # Build with GCC linking (expected to fail)
#   make test      # Run ICC-linked binary

CC_ICC  = icc -qopenmp
CC_GCC  = gcc -fopenmp

.PHONY: all clean test gcc-link

all: test_icc_gcc_mix

icc_gcc_func.o: icc_gcc_func.c
	$(CC_GCC) -c -o $@ $<

icc_main.o: icc_main.c
	$(CC_ICC) -c -o $@ $<

# Recommended: ICC links everything -> only libiomp5.so
test_icc_gcc_mix: icc_main.o icc_gcc_func.o
	$(CC_ICC) -o $@ $^
	@echo ""
	@echo "Linked libraries:"
	@ldd $@ | grep -E "iomp5|gomp" || true

# GCC linking ICC objects: expected to FAIL (missing __kmpc_* symbols)
gcc-link: icc_main.o icc_gcc_func.o
	-$(CC_GCC) -o test_gcc_link $^
	@echo ""
	@echo "GCC link expected to fail due to missing Intel OpenMP symbols."

test: test_icc_gcc_mix
	@echo ""
	@echo "=== Running ICC-linked mixed binary ==="
	./test_icc_gcc_mix

clean:
	rm -f *.o test_icc_gcc_mix test_gcc_link
