/*
 * GCC-compiled C++ exception source
 * Used to test cross-compiler C++ exception ABI compatibility.
 */
#include <stdexcept>

void gcc_throw(void)
{
    throw std::runtime_error("Exception thrown by GCC-compiled code");
}
