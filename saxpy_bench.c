#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define N (32*1024*1024)
#define NUM_THREADS 61

extern void saxpy_vec(double *A, double *B, double s, int n);

typedef struct { double *A, *B; double s; int n; } thread_arg;

void* saxpy_thread(void* arg) {
    thread_arg* a = (thread_arg*)arg;
    saxpy_vec(a->A, a->B, a->s, a->n);
    return NULL;
}

int main() {
    printf("SAXPY ICC %dM elements, %d threads\n", N/(1024*1024), NUM_THREADS);
    double *A, *B;
    int ret = posix_memalign((void**)&A, 64, N*sizeof(double));
    ret |= posix_memalign((void**)&B, 64, N*sizeof(double));
    if (ret) { printf("alloc failed\n"); return 1; }
    int i;
    for(i=0; i<N; i++) { A[i]=1.0; B[i]=2.0; }

    pthread_t th[NUM_THREADS];
    thread_arg args[NUM_THREADS];
    int per = N / NUM_THREADS;
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    int t;
    for(t=0; t<NUM_THREADS; t++) {
        args[t].A = A + t*per;
        args[t].B = B + t*per;
        args[t].s = 2.0;
        args[t].n = (t==NUM_THREADS-1) ? N - t*per : per;
        pthread_create(&th[t], NULL, saxpy_thread, &args[t]);
    }
    for(t=0; t<NUM_THREADS; t++) pthread_join(th[t], NULL);

    gettimeofday(&tv2, NULL);
    double elapsed = (tv2.tv_sec - tv1.tv_sec) + (tv2.tv_usec - tv1.tv_usec)/1e6;
    double gbytes = (3.0 * N * sizeof(double)) / (elapsed * 1e9);
    double gflops = (2.0 * N) / (elapsed * 1e9);
    printf("Elapsed: %.3f s, BW: %.2f GB/s, GFLOPS: %.2f\n", elapsed, gbytes, gflops);
    printf("Check: A[0]=%.1f A[%d]=%.1f\n", A[0], N-1, A[N-1]);
    free(A); free(B);
    return 0;
}
