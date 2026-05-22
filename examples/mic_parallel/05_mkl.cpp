/**
 * 05_mkl.cpp
 * 
 * MIC 原生 Intel MKL 自动多线程示例
 * 使用 MKL DGEMM（矩阵乘法），MKL 内部自动并行
 * 
 * 编译: icpc -mmic -O2 -o 05_mkl.mic 05_mkl.cpp \
 *          -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5
 * 
 * 运行: SINK_LD_LIBRARY_PATH=... micnativeloadex ./05_mkl.mic
 * 
 * 注意：使用 -lmkl_intel_thread（threaded 模式），MKL 会自动使用多线程
 *       也可用 -lmkl_sequential（单线程模式），但这里展示自动并行
 */

#include <mkl.h>
#include <iostream>
#include <vector>
#include <cmath>

int main(int argc, char* argv[])
{
    const int M = 2000;
    const int N = 2000;
    const int K = 2000;

    std::vector<double> A(M * K, 1.0);
    std::vector<double> B(K * N, 2.0);
    std::vector<double> C(M * N, 0.0);

    // 查询 MKL 线程数
    int mkl_threads = mkl_get_max_threads();
    std::cout << "MKL max threads: " << mkl_threads << std::endl;

    double alpha = 1.0;
    double beta = 0.0;

    double t0 = dsecnd();

    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M, N, K, alpha, A.data(), K, B.data(), N, beta, C.data(), N);

    double t1 = dsecnd();

    // 验证：所有元素应为 2.0 * K = 4000.0
    double expected = 2.0 * K;
    bool ok = true;
    for (int i = 0; i < M && ok; i++) {
        for (int j = 0; j < N && ok; j++) {
            if (std::abs(C[i * N + j] - expected) > 1e-10) {
                ok = false;
            }
        }
    }

    double gflops = 2.0 * M * N * K / (t1 - t0) / 1e9;

    std::cout << "=== MIC MKL Auto-Threading Example ===" << std::endl;
    std::cout << "Matrix:      " << M << "x" << K << " * " << K << "x" << N << std::endl;
    std::cout << "Time:        " << (t1 - t0) << " sec" << std::endl;
    std::cout << "GFLOPS:      " << gflops << std::endl;
    std::cout << "C[0][0]:     " << C[0] << " (expected " << expected << ")" << std::endl;
    std::cout << "Verify:      " << (ok ? "PASSED" : "FAILED") << std::endl;

    return ok ? 0 : 1;
}
