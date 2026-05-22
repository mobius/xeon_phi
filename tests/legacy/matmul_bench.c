#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define N 2048
#define NUM_THREADS 244

typedef struct { int tid, start, end; double *A, *B, *C; } thread_arg;

void* matmul_thread(void* arg) {
    thread_arg* a = (thread_arg*)arg;
    int i,j,k;
    for(i=a->start; i<a->end; i++)
        for(k=0; k<N; k++) {
            double aik = a->A[i*N + k];
            for(j=0; j<N; j++)
                a->C[i*N + j] += aik * a->B[k*N + j];
        }
    return NULL;
}

int main() {
    printf("Matrix Mul 2048x2048, %d threads\n", NUM_THREADS);
    double *A = (double*)aligned_alloc(64, N*N*sizeof(double));
    double *B = (double*)aligned_alloc(64, N*N*sizeof(double));
    double *C = (double*)aligned_alloc(64, N*N*sizeof(double));
    memset(C, 0, N*N*sizeof(double));
    for(int i=0; i<N*N; i++) { A[i]=1.0; B[i]=1.0; }

    pthread_t th[NUM_THREADS];
    thread_arg args[NUM_THREADS];
    int rows_per = N / NUM_THREADS;
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    for(int t=0; t<NUM_THREADS; t++) {
        args[t].tid = t;
        args[t].start = t * rows_per;
        args[t].end = (t==NUM_THREADS-1) ? N : (t+1)*rows_per;
        args[t].A = A; args[t].B = B; args[t].C = C;
        pthread_create(&th[t], NULL, matmul_thread, &args[t]);
    }
    for(int t=0; t<NUM_THREADS; t++) pthread_join(th[t], NULL);

    gettimeofday(&tv2, NULL);
    double elapsed = (tv2.tv_sec - tv1.tv_sec) + (tv2.tv_usec - tv1.tv_usec)/1e6;
    double gflops = (2.0 * N * N * N) / (elapsed * 1e9);
    printf("Elapsed: %.3f s, GFLOPS: %.2f\n", elapsed, gflops);
    printf("Check: C[0]=%.1f C[%d]=%.1f\n", C[0], N*N-1, C[N*N-1]);
    free(A); free(B); free(C);
    return 0;
}
