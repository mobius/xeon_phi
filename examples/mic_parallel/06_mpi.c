/**
 * 06_mpi.c
 * 
 * MIC 原生 Intel MPI 多进程示例
 * 
 * 编译: mpiicc -mmic -O2 -o 06_mpi.mic 06_mpi.c
 * 
 * 运行方式 1（Host 启动 MIC 进程）:
 *   mpirun -host mic0 -np 4 ./06_mpi.mic
 * 
 * 运行方式 2（复制到 MIC 后直接运行）:
 *   scp 06_mpi.mic mic0:/tmp/
 *   ssh mic0 "export LD_LIBRARY_PATH=/tmp/mic_libs; mpirun -np 4 /tmp/06_mpi.mic"
 * 
 * 注意：需要 MIC 端有 MPI runtime 库（libmpi.so 等）
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define N 10000000

int main(int argc, char** argv)
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        printf("=== MIC MPI Example ===\n");
        printf("MPI ranks: %d\n", size);
    }

    // 每个 rank 处理一部分向量
    int local_n = N / size;
    int remainder = N % size;
    int start = rank * local_n + (rank < remainder ? rank : remainder);
    int end = start + local_n + (rank < remainder ? 1 : 0);
    int count = end - start;

    double* a = (double*)malloc(count * sizeof(double));
    double* b = (double*)malloc(count * sizeof(double));
    double* c = (double*)malloc(count * sizeof(double));

    int i;
    for (i = 0; i < count; i++) {
        int global_idx = start + i;
        a[i] = (double)global_idx;
        b[i] = (double)(N - global_idx);
        c[i] = a[i] + b[i];
    }

    // 本地求和
    double local_sum = 0.0;
    for (i = 0; i < count; i++) {
        local_sum += c[i];
    }

    // 全局归约
    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double expected = (double)N * N;
        printf("Vector size: %d\n", N);
        printf("Sum:         %.4e\n", global_sum);
        printf("Expected:    %.4e\n", expected);
        printf("Verify:      %s\n", (global_sum == expected) ? "PASSED" : "FAILED");
    }

    free(a); free(b); free(c);
    MPI_Finalize();
    return 0;
}
