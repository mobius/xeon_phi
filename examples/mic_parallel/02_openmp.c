/**
 * 02_openmp.c
 * 
 * MIC 原生 OpenMP 多线程示例
 * 
 * 编译: icc -mmic -openmp -O2 -o 02_openmp.mic 02_openmp.c
 * 运行: SINK_LD_LIBRARY_PATH=... micnativeloadex ./02_openmp.mic
 */

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 100000000

int main(int argc, char* argv[])
{
    double* a = (double*)malloc(N * sizeof(double));
    double* b = (double*)malloc(N * sizeof(double));
    double* c = (double*)malloc(N * sizeof(double));

    // 串行初始化
    int i;
    for (i = 0; i < N; i++) {
        a[i] = (double)i;
        b[i] = (double)(N - i);
    }

    int nthreads = omp_get_max_threads();
    printf("OpenMP max threads: %d\n", nthreads);

    double t0 = omp_get_wtime();

    // OpenMP 并行循环
    #pragma omp parallel for schedule(static)
    for (i = 0; i < N; i++) {
        c[i] = a[i] + b[i];
    }

    double t1 = omp_get_wtime();

    // 归约验证
    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum) schedule(static)
    for (i = 0; i < N; i++) {
        sum += c[i];
    }

    printf("=== MIC OpenMP Example ===\n");
    printf("Threads:     %d\n", nthreads);
    printf("Vector size: %d\n", N);
    printf("Time:        %.4f sec\n", t1 - t0);
    printf("Sum:         %.4e (expected %.4e)\n", sum, (double)N * N);
    printf("Verify:      %s\n", (sum == (double)N * N) ? "PASSED" : "FAILED");

    free(a); free(b); free(c);
    return 0;
}
