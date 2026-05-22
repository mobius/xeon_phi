/*
 * Xeon Phi 7120P DGEMM Benchmark (v3 - simple blocked)
 * Compile: icc -std=c99 -mmic -O3 -openmp -restrict -o phi_peak_dgemm.mic phi_peak_dgemm.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <immintrin.h>
#include <omp.h>

#ifndef N
#define N 2048
#endif

static double mysecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1.0e-6;
}

int main() {
    printf("========================================\n");
    printf("Xeon Phi 7120P DGEMM Benchmark (v3)\n");
    printf("Matrix size: %d x %d\n", N, N);
    printf("Threads: %d\n", omp_get_max_threads());
    printf("Theory FP64: ~1.208 TFLOPS\n");
    printf("========================================\n");

    size_t sz = (size_t)N * N * sizeof(double);
    double *A, *B, *C;
    if (posix_memalign((void**)&A, 64, sz) != 0 ||
        posix_memalign((void**)&B, 64, sz) != 0 ||
        posix_memalign((void**)&C, 64, sz) != 0) {
        printf("Memory allocation failed\n");
        return 1;
    }

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            A[i*N + j] = (double)(i + j) / (double)N;
            B[i*N + j] = (double)(i - j + N) / (double)N;
            C[i*N + j] = 0.0;
        }

    double t0 = mysecond();

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            double aik = A[i * N + k];
            __m512d va = _mm512_set1_pd(aik);
            int j = 0;
            for (; j + 31 < N; j += 32) {
                __m512d vc0 = _mm512_load_pd(&C[i * N + j + 0]);
                __m512d vc1 = _mm512_load_pd(&C[i * N + j + 8]);
                __m512d vc2 = _mm512_load_pd(&C[i * N + j + 16]);
                __m512d vc3 = _mm512_load_pd(&C[i * N + j + 24]);
                __m512d vb0 = _mm512_load_pd(&B[k * N + j + 0]);
                __m512d vb1 = _mm512_load_pd(&B[k * N + j + 8]);
                __m512d vb2 = _mm512_load_pd(&B[k * N + j + 16]);
                __m512d vb3 = _mm512_load_pd(&B[k * N + j + 24]);
                vc0 = _mm512_fmadd_pd(va, vb0, vc0);
                vc1 = _mm512_fmadd_pd(va, vb1, vc1);
                vc2 = _mm512_fmadd_pd(va, vb2, vc2);
                vc3 = _mm512_fmadd_pd(va, vb3, vc3);
                _mm512_store_pd(&C[i * N + j + 0],  vc0);
                _mm512_store_pd(&C[i * N + j + 8],  vc1);
                _mm512_store_pd(&C[i * N + j + 16], vc2);
                _mm512_store_pd(&C[i * N + j + 24], vc3);
            }
            for (; j + 7 < N; j += 8) {
                __m512d vc = _mm512_load_pd(&C[i * N + j]);
                __m512d vb = _mm512_load_pd(&B[k * N + j]);
                vc = _mm512_fmadd_pd(va, vb, vc);
                _mm512_store_pd(&C[i * N + j], vc);
            }
            for (; j < N; j++) {
                C[i * N + j] += aik * B[k * N + j];
            }
        }
    }

    double t1 = mysecond();
    double elapsed = t1 - t0;

    double gflops = (2.0 * (double)N * (double)N * (double)N) / (elapsed * 1.0e9);

    printf("Elapsed: %.3f sec\n", elapsed);
    printf("DGEMM GFLOPS: %.2f\n", gflops);
    printf("%% of theory (1.208 TFLOPS): %.1f%%\n", gflops / 1208.0 * 100.0);

    double checksum = 0.0;
    #pragma omp parallel for reduction(+:checksum) schedule(static)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            checksum += C[i*N + j];
    printf("Checksum: %.6e\n", checksum);

    free(A); free(B); free(C);
    return 0;
}
