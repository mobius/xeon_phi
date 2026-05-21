/*
 * ICC/GCC C++ Exception Cross-ABI Test
 *
 * Purpose: Verify that exceptions thrown by GCC-compiled C++ code
 *          can be caught by ICC-compiled C++ code.
 *
 * Build:
 *   g++ -c -o gcc_throw.o gcc_throw.cpp
 *   icpc -c -o icc_catch.o icc_catch.cpp
 *   icpc -o test_cpp_cross gcc_throw.o icc_catch.o
 *
 * Expected: "ICC caught: Exception thrown by GCC-compiled code"
 */
#include <iostream>
#include <stdexcept>

extern void gcc_throw(void);

int main(void)
{
    try {
        gcc_throw();
    } catch (const std::exception& e) {
        std::cout << "ICC caught: " << e.what() << std::endl;
    }
    return 0;
}
