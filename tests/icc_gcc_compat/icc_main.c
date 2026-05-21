/*
 * ICC-compiled main calling GCC-compiled OpenMP function
 *
 * Compile & link with ICC:
 *   icc -qopenmp -c -o icc_main.o icc_main.c
 *   icc -qopenmp -o test_icc_gcc_mix icc_main.o icc_gcc_func.o
 *
 * Compile gcc_func.o with GCC:
 *   gcc -fopenmp -c -o icc_gcc_func.o icc_gcc_func.c
 *
 * Expected: libiomp5.so handles both ICC and GCC OpenMP regions.
 */
#include <stdio.h>

extern void gcc_func(void);

int main(void)
{
    printf("ICC main starting...\n");
    gcc_func();
    printf("ICC main done.\n");
    return 0;
}
