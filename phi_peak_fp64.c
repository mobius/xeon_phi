/*
 * Xeon Phi 7120P FP64 Peak FLOPS Benchmark (Final)
 * Compile: icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp64.mic phi_peak_fp64.c
 *
 * Best config: 244 threads, 16 accumulators, ~527 GFLOPS measured
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <immintrin.h>
#include <omp.h>

#ifndef NITER
#define NITER   (4 * 1024 * 1024)
#endif

#define NACC    16

static double mysecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1.0e-6;
}

int main() {
    int nthreads = omp_get_max_threads();
    printf("========================================\n");
    printf("Xeon Phi 7120P FP64 Peak FLOPS Test\n");
    printf("Threads: %d\n", nthreads);
    printf("Iterations per thread: %d\n", NITER);
    printf("Theory: 61 * 1.238 GHz * 8 * 2 = ~1.208 TFLOPS\n");
    printf("========================================\n");

    const int VLEN = 8;
    double *a, *b;
    if (posix_memalign((void**)&a, 64, VLEN * sizeof(double)) != 0 ||
        posix_memalign((void**)&b, 64, VLEN * sizeof(double)) != 0) {
        printf("alloc failed\n"); return 1;
    }

    for (int i = 0; i < VLEN; i++) {
        a[i] = 1.00001;
        b[i] = 1.00002;
    }

    double t0 = mysecond();

    #pragma omp parallel
    {
        __m512d va = _mm512_load_pd(a);
        __m512d vb = _mm512_load_pd(b);

        __m512d s0  = _mm512_set1_pd(1.0);
        __m512d s1  = _mm512_set1_pd(1.1);
        __m512d s2  = _mm512_set1_pd(1.2);
        __m512d s3  = _mm512_set1_pd(1.3);
        __m512d s4  = _mm512_set1_pd(1.4);
        __m512d s5  = _mm512_set1_pd(1.5);
        __m512d s6  = _mm512_set1_pd(1.6);
        __m512d s7  = _mm512_set1_pd(1.7);
        __m512d s8  = _mm512_set1_pd(1.8);
        __m512d s9  = _mm512_set1_pd(1.9);
        __m512d s10 = _mm512_set1_pd(2.0);
        __m512d s11 = _mm512_set1_pd(2.1);
        __m512d s12 = _mm512_set1_pd(2.2);
        __m512d s13 = _mm512_set1_pd(2.3);
        __m512d s14 = _mm512_set1_pd(2.4);
        __m512d s15 = _mm512_set1_pd(2.5);

        for (int i = 0; i < NITER; i++) {
            s0  = _mm512_fmadd_pd(s0,  va, vb);
            s1  = _mm512_fmadd_pd(s1,  va, vb);
            s2  = _mm512_fmadd_pd(s2,  va, vb);
            s3  = _mm512_fmadd_pd(s3,  va, vb);
            s4  = _mm512_fmadd_pd(s4,  va, vb);
            s5  = _mm512_fmadd_pd(s5,  va, vb);
            s6  = _mm512_fmadd_pd(s6,  va, vb);
            s7  = _mm512_fmadd_pd(s7,  va, vb);
            s8  = _mm512_fmadd_pd(s8,  va, vb);
            s9  = _mm512_fmadd_pd(s9,  va, vb);
            s10 = _mm512_fmadd_pd(s10, va, vb);
            s11 = _mm512_fmadd_pd(s11, va, vb);
            s12 = _mm512_fmadd_pd(s12, va, vb);
            s13 = _mm512_fmadd_pd(s13, va, vb);
            s14 = _mm512_fmadd_pd(s14, va, vb);
            s15 = _mm512_fmadd_pd(s15, va, vb);
        }

        __m512d sum = _mm512_add_pd(s0, s1);
        sum = _mm512_add_pd(sum, s2);
        sum = _mm512_add_pd(sum, s3);
        sum = _mm512_add_pd(sum, s4);
        sum = _mm512_add_pd(sum, s5);
        sum = _mm512_add_pd(sum, s6);
        sum = _mm512_add_pd(sum, s7);
        sum = _mm512_add_pd(sum, s8);
        sum = _mm512_add_pd(sum, s9);
        sum = _mm512_add_pd(sum, s10);
        sum = _mm512_add_pd(sum, s11);
        sum = _mm512_add_pd(sum, s12);
        sum = _mm512_add_pd(sum, s13);
        sum = _mm512_add_pd(sum, s14);
        sum = _mm512_add_pd(sum, s15);

        double result[8] __attribute__((aligned(64)));
        _mm512_store_pd(result, sum);
        if (result[0] == 0.0) printf("unlikely\n");
    }

    double t1 = mysecond();
    double elapsed = t1 - t0;

    double flops_per_iter = (double)NACC * (double)VLEN * 2.0;
    double total_flops = flops_per_iter * (double)NITER * (double)nthreads;
    double gflops = total_flops / (elapsed * 1.0e9);

    printf("Elapsed: %.3f sec\n", elapsed);
    printf("FP64 GFLOPS: %.2f\n", gflops);
    printf("%% of theory (1.208 TFLOPS): %.1f%%\n", gflops / 1208.0 * 100.0);

    free(a); free(b);
    return 0;
}
