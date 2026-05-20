/*
 * Xeon Phi 7120P STREAM-like Memory Bandwidth Benchmark (v3)
 * Compile: icc -std=c99 -mmic -O3 -openmp -restrict -o phi_stream_bench.mic phi_stream_bench.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <omp.h>

#ifndef STREAM_ARRAY_SIZE
#define STREAM_ARRAY_SIZE   (64 * 1024 * 1024)
#endif

#ifndef NTIMES
#define NTIMES  40
#endif

#ifndef OFFSET
#define OFFSET  0
#endif

#define HLINE "------------------------------------------------------------\n"

static const char *label[4] = {"COPY  ", "SCALE ", "ADD   ", "TRIAD "};
static double bytes[4] = {
    2 * sizeof(double) * STREAM_ARRAY_SIZE,
    2 * sizeof(double) * STREAM_ARRAY_SIZE,
    3 * sizeof(double) * STREAM_ARRAY_SIZE,
    3 * sizeof(double) * STREAM_ARRAY_SIZE
};

static double mysecond() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1.0e-6;
}

int main() {
    int k;
    double times[4][NTIMES];
    double avgtime[4] = {0}, maxtime[4] = {0}, mintime[4];
    double * restrict a;
    double * restrict b;
    double * restrict c;

    printf(HLINE);
    printf("Xeon Phi 7120P STREAM-like Bandwidth Benchmark (v3)\n");
    printf("Array size = %d (%.1f MB per array)\n",
           STREAM_ARRAY_SIZE,
           (double)STREAM_ARRAY_SIZE * sizeof(double) / (1024*1024));
    printf("Total memory required = %.1f MB\n",
           3.0 * STREAM_ARRAY_SIZE * sizeof(double) / (1024*1024));
    printf("Number of threads = %d\n", omp_get_max_threads());
    printf(HLINE);

    if (posix_memalign((void**)&a, 64, (STREAM_ARRAY_SIZE + OFFSET) * sizeof(double)) != 0 ||
        posix_memalign((void**)&b, 64, (STREAM_ARRAY_SIZE + OFFSET) * sizeof(double)) != 0 ||
        posix_memalign((void**)&c, 64, (STREAM_ARRAY_SIZE + OFFSET) * sizeof(double)) != 0) {
        printf("Allocation failed\n");
        return 1;
    }

    #pragma omp parallel for schedule(static)
    for (int j = 0; j < STREAM_ARRAY_SIZE; j++) {
        a[j] = 1.0;
        b[j] = 2.0;
        c[j] = 0.0;
    }

    double scalar = 3.0;

    for (k = 0; k < NTIMES; k++) {
        times[0][k] = mysecond();
        #pragma omp parallel for schedule(static)
        for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
            c[j] = a[j];
        times[0][k] = mysecond() - times[0][k];

        times[1][k] = mysecond();
        #pragma omp parallel for schedule(static)
        for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
            b[j] = scalar * c[j];
        times[1][k] = mysecond() - times[1][k];

        times[2][k] = mysecond();
        #pragma omp parallel for schedule(static)
        for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
            c[j] = a[j] + b[j];
        times[2][k] = mysecond() - times[2][k];

        times[3][k] = mysecond();
        #pragma omp parallel for schedule(static)
        for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
            a[j] = b[j] + scalar * c[j];
        times[3][k] = mysecond() - times[3][k];
    }

    for (int j = 0; j < 4; j++) mintime[j] = times[j][1];

    for (k = 1; k < NTIMES; k++) {
        for (int j = 0; j < 4; j++) {
            avgtime[j] += times[j][k];
            if (times[j][k] < mintime[j]) mintime[j] = times[j][k];
            if (times[j][k] > maxtime[j]) maxtime[j] = times[j][k];
        }
    }

    for (int j = 0; j < 4; j++) {
        avgtime[j] /= (double)(NTIMES - 1);
    }

    printf("Function  BestRate(MB/s)  AvgTime(sec)  MinTime(sec)  MaxTime(sec)\n");
    for (int j = 0; j < 4; j++) {
        printf("%s%12.1f      %10.6f    %10.6f    %10.6f\n",
               label[j],
               1.0E-06 * bytes[j] / mintime[j],
               avgtime[j],
               mintime[j],
               maxtime[j]);
    }
    printf(HLINE);

    printf("\n=== Bandwidth Summary (GB/s) ===\n");
    for (int j = 0; j < 4; j++) {
        printf("%s Best: %.2f GB/s\n", label[j], 1.0E-09 * bytes[j] / mintime[j]);
    }
    printf("Theory: 352 GB/s\n");
    printf(HLINE);

    double asum = 0.0, bsum = 0.0, csum = 0.0;
    #pragma omp parallel for reduction(+:asum,bsum,csum) schedule(static)
    for (int j = 0; j < STREAM_ARRAY_SIZE; j++) {
        asum += a[j];
        bsum += b[j];
        csum += c[j];
    }
    printf("Checksums: a=%.3e b=%.3e c=%.3e\n", asum, bsum, csum);

    free(a); free(b); free(c);
    return 0;
}
