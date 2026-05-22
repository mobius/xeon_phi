/**
 * 01_pthreads.c
 * 
 * MIC 原生 pthreads 多线程示例
 * 
 * 编译: icc -mmic -O2 -o 01_pthreads.mic 01_pthreads.c -lpthread
 * 运行: SINK_LD_LIBRARY_PATH=... micnativeloadex ./01_pthreads.mic
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define N 100000000
#define NUM_THREADS 244

typedef struct {
    int tid;
    int nthreads;
    double* a;
    double* b;
    double* c;
    size_t n;
} ThreadArgs;

void* worker(void* arg)
{
    ThreadArgs* args = (ThreadArgs*)arg;
    int tid = args->tid;
    int nthreads = args->nthreads;
    size_t chunk = args->n / nthreads;
    size_t start = tid * chunk;
    size_t end = (tid == nthreads - 1) ? args->n : start + chunk;

    size_t i;
    for (i = start; i < end; i++) {
        args->c[i] = args->a[i] + args->b[i];
    }
    return NULL;
}

static double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main(int argc, char* argv[])
{
    double* a = (double*)malloc(N * sizeof(double));
    double* b = (double*)malloc(N * sizeof(double));
    double* c = (double*)malloc(N * sizeof(double));

    // 初始化
    size_t i;
    for (i = 0; i < N; i++) {
        a[i] = (double)i;
        b[i] = (double)(N - i);
    }

    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    double t0 = get_time();

    int t;
    for (t = 0; t < NUM_THREADS; t++) {
        args[t].tid = t;
        args[t].nthreads = NUM_THREADS;
        args[t].a = a;
        args[t].b = b;
        args[t].c = c;
        args[t].n = N;
        pthread_create(&threads[t], NULL, worker, &args[t]);
    }

    for (t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    double t1 = get_time();

    // 验证
    double sum = 0.0;
    for (i = 0; i < N; i++) {
        sum += c[i];
    }

    printf("=== MIC Pthreads Example ===\n");
    printf("Threads:     %d\n", NUM_THREADS);
    printf("Vector size: %d\n", N);
    printf("Time:        %.4f sec\n", t1 - t0);
    printf("Sum:         %.4e (expected %.4e)\n", sum, (double)N * N);
    printf("Verify:      %s\n", (sum == (double)N * N) ? "PASSED" : "FAILED");

    free(a); free(b); free(c);
    return 0;
}
