/*
 * OpenMP Dual Library Conflict Test
 *
 * Purpose: Demonstrate what happens when both libiomp5.so (Intel)
 *          and libgomp.so (GCC) are linked simultaneously.
 *
 * Compile:
 *   icc -qopenmp -o test_dual_omp dual_omp.c -lgomp
 *
 * Expected behavior:
 *   - Links successfully (both libraries resolve)
 *   - Runtime shows only 2 threads (both print "thread 0")
 *     because the two OpenMP runtimes overwrite each other.
 *
 * Conclusion: Never link both OpenMP libraries simultaneously.
 *             Use libiomp5.so only (it provides GCC ABI compatibility).
 */
#include <omp.h>
#include <stdio.h>

int main(void)
{
    printf("Testing with #pragma omp parallel num_threads(2)\n");
    #pragma omp parallel num_threads(2)
    {
        printf("  Thread %d of %d\n",
               omp_get_thread_num(), omp_get_num_threads());
    }
    return 0;
}
