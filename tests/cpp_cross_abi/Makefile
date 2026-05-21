# C++ Cross-Compiler Exception ABI Test
#
# Purpose: Verify ICC can catch exceptions thrown by GCC-compiled code.
#
# Usage: make && make test

CXX_ICC = icpc
CXX_GCC = g++

.PHONY: all clean test

all: test_cpp_cross

gcc_throw.o: gcc_throw.cpp
	$(CXX_GCC) -c -o $@ $<

icc_catch.o: icc_catch.cpp
	$(CXX_ICC) -c -o $@ $<

test_cpp_cross: gcc_throw.o icc_catch.o
	$(CXX_ICC) -o $@ $^

test: test_cpp_cross
	@echo ""
	@echo "=== Running C++ cross-compiler exception test ==="
	./test_cpp_cross

clean:
	rm -f *.o test_cpp_cross
