/*
 * GCC-compiled function with OpenMP
 * Used to test ICC/GCC object mixing.
 */
#include <omp.h>
#include <stdio.h>

void gcc_func(void)
{
    #pragma omp parallel num_threads(4)
    {
        printf("  [GCC func] thread %d/%d\n",
               omp_get_thread_num(), omp_get_num_threads());
    }
}
