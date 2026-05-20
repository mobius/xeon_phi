/*
 * Xeon Phi 7120P FP32 Peak FLOPS Benchmark (Final)
 * Compile: icc -std=c99 -mmic -O3 -openmp -o phi_peak_fp32.mic phi_peak_fp32.c
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
    printf("Xeon Phi 7120P FP32 Peak FLOPS Test\n");
    printf("Threads: %d\n", nthreads);
    printf("Iterations per thread: %d\n", NITER);
    printf("Theory: 61 * 1.238 GHz * 16 * 2 = ~2.416 TFLOPS\n");
    printf("========================================\n");

    const int VLEN = 16;
    float *a, *b;
    if (posix_memalign((void**)&a, 64, VLEN * sizeof(float)) != 0 ||
        posix_memalign((void**)&b, 64, VLEN * sizeof(float)) != 0) {
        printf("alloc failed\n"); return 1;
    }

    for (int i = 0; i < VLEN; i++) {
        a[i] = 1.00001f;
        b[i] = 1.00002f;
    }

    double t0 = mysecond();

    #pragma omp parallel
    {
        __m512 va = _mm512_load_ps(a);
        __m512 vb = _mm512_load_ps(b);

        __m512 s0  = _mm512_set1_ps(1.0f);
        __m512 s1  = _mm512_set1_ps(1.1f);
        __m512 s2  = _mm512_set1_ps(1.2f);
        __m512 s3  = _mm512_set1_ps(1.3f);
        __m512 s4  = _mm512_set1_ps(1.4f);
        __m512 s5  = _mm512_set1_ps(1.5f);
        __m512 s6  = _mm512_set1_ps(1.6f);
        __m512 s7  = _mm512_set1_ps(1.7f);
        __m512 s8  = _mm512_set1_ps(1.8f);
        __m512 s9  = _mm512_set1_ps(1.9f);
        __m512 s10 = _mm512_set1_ps(2.0f);
        __m512 s11 = _mm512_set1_ps(2.1f);
        __m512 s12 = _mm512_set1_ps(2.2f);
        __m512 s13 = _mm512_set1_ps(2.3f);
        __m512 s14 = _mm512_set1_ps(2.4f);
        __m512 s15 = _mm512_set1_ps(2.5f);

        for (int i = 0; i < NITER; i++) {
            s0  = _mm512_fmadd_ps(s0,  va, vb);
            s1  = _mm512_fmadd_ps(s1,  va, vb);
            s2  = _mm512_fmadd_ps(s2,  va, vb);
            s3  = _mm512_fmadd_ps(s3,  va, vb);
            s4  = _mm512_fmadd_ps(s4,  va, vb);
            s5  = _mm512_fmadd_ps(s5,  va, vb);
            s6  = _mm512_fmadd_ps(s6,  va, vb);
            s7  = _mm512_fmadd_ps(s7,  va, vb);
            s8  = _mm512_fmadd_ps(s8,  va, vb);
            s9  = _mm512_fmadd_ps(s9,  va, vb);
            s10 = _mm512_fmadd_ps(s10, va, vb);
            s11 = _mm512_fmadd_ps(s11, va, vb);
            s12 = _mm512_fmadd_ps(s12, va, vb);
            s13 = _mm512_fmadd_ps(s13, va, vb);
            s14 = _mm512_fmadd_ps(s14, va, vb);
            s15 = _mm512_fmadd_ps(s15, va, vb);
        }

        __m512 sum = _mm512_add_ps(s0, s1);
        sum = _mm512_add_ps(sum, s2);
        sum = _mm512_add_ps(sum, s3);
        sum = _mm512_add_ps(sum, s4);
        sum = _mm512_add_ps(sum, s5);
        sum = _mm512_add_ps(sum, s6);
        sum = _mm512_add_ps(sum, s7);
        sum = _mm512_add_ps(sum, s8);
        sum = _mm512_add_ps(sum, s9);
        sum = _mm512_add_ps(sum, s10);
        sum = _mm512_add_ps(sum, s11);
        sum = _mm512_add_ps(sum, s12);
        sum = _mm512_add_ps(sum, s13);
        sum = _mm512_add_ps(sum, s14);
        sum = _mm512_add_ps(sum, s15);

        float result[16] __attribute__((aligned(64)));
        _mm512_store_ps(result, sum);
        if (result[0] == 0.0f) printf("unlikely\n");
    }

    double t1 = mysecond();
    double elapsed = t1 - t0;

    double flops_per_iter = (double)NACC * (double)VLEN * 2.0;
    double total_flops = flops_per_iter * (double)NITER * (double)nthreads;
    double gflops = total_flops / (elapsed * 1.0e9);

    printf("Elapsed: %.3f sec\n", elapsed);
    printf("FP32 GFLOPS: %.2f\n", gflops);
    printf("%% of theory (2.416 TFLOPS): %.1f%%\n", gflops / 2416.0 * 100.0);

    free(a); free(b);
    return 0;
}
