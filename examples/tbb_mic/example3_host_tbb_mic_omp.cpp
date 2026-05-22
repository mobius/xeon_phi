/**
 * example3_host_tbb_mic_omp.cpp
 * 
 * Host TBB + MIC OpenMP 混合模式示例
 * 
 * 说明：
 *   - Host 端（Rocky Linux）使用 TBB 进行数据预处理/后处理
 *   - MIC 端（Xeon Phi 7120P）使用 OpenMP 进行大规模并行计算
 *   - 通过 #pragma offload 传输数据
 * 
 * 注意：
 *   - ICC 16.0 MIC 编译器对 C++11 lambda 支持不完整，使用 functor
 *   - #pragma offload 数组切片需要使用原生指针语法
 * 
 * 编译: icpc -qoffload -tbb -openmp -O2 -o example3_host_tbb_mic_omp example3_host_tbb_mic_omp.cpp
 * 运行: MIC_LD_LIBRARY_PATH=/path/to/mic/libs ./example3_host_tbb_mic_omp
 */

#include <tbb/tbb.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <omp.h>

// MIC 端计算函数：使用 OpenMP（不能使用 TBB!）
__attribute__((target(mic)))
void mic_compute(double* __restrict__ a,
                 double* __restrict__ b,
                 double* __restrict__ c,
                 size_t N)
{
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; ++i) {
        c[i] = std::sqrt(a[i] * a[i] + b[i] * b[i]);
    }
}

// ---- Host 端 TBB Functor：初始化数据 ----
struct HostInitFunctor {
    double* a;
    double* b;
    size_t  N;

    HostInitFunctor(double* a_, double* b_, size_t N_)
        : a(a_), b(b_), N(N_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            a[i] = static_cast<double>(i);
            b[i] = static_cast<double>(N - i);
        }
    }
};

// ---- Host 端 TBB Functor：验证求和 ----
struct HostSumFunctor {
    const double* c;

    HostSumFunctor(const double* c_) : c(c_) {}

    void operator()(const tbb::blocked_range<size_t>& r, double local_sum) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            local_sum += c[i];
        }
        // 注意：parallel_reduce 的 body 需要返回局部和
        // 这里使用外部变量存储结果
    }
};

int main(int argc, char* argv[])
{
    const size_t N = 100000000;  // 1亿元素

    // 使用原生指针（#pragma offload 需要）
    double* a = new double[N];
    double* b = new double[N];
    double* c = new double[N];

    std::cout << "========================================" << std::endl;
    std::cout << "Host TBB + MIC OpenMP 混合模式示例" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Vector size: " << N << std::endl;

    // ---- Phase 1: Host 端 TBB 初始化 ----
    std::cout << "\n[Phase 1] Host TBB 初始化数据..." << std::endl;
    tbb::tick_count t0 = tbb::tick_count::now();

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        HostInitFunctor(a, b, N)
    );

    tbb::tick_count t1 = tbb::tick_count::now();
    std::cout << "Host init time: " << (t1 - t0).seconds() << " sec" << std::endl;

    // ---- Phase 2: 通过 Offload 在 MIC 上计算 ----
    std::cout << "\n[Phase 2] Offload 到 MIC (OpenMP)..." << std::endl;

    double offload_time = 0.0;
    #pragma offload target(mic) \
        in(a[0:N], b[0:N]) \
        out(c[0:N]) \
        out(offload_time)
    {
        double t_start = omp_get_wtime();
        mic_compute(a, b, c, N);
        offload_time = omp_get_wtime() - t_start;
    }

    std::cout << "MIC compute time: " << offload_time << " sec" << std::endl;
    std::cout << "MIC bandwidth (read+write): " 
              << (3.0 * N * sizeof(double) / offload_time / 1e9) 
              << " GB/s" << std::endl;

    // ---- Phase 3: Host 端串行验证结果 ----
    std::cout << "\n[Phase 3] Host 验证结果..." << std::endl;
    double sum = 0.0;
    for (size_t i = 0; i < N; ++i) {
        sum += c[i];
    }
    std::cout << "Sum of c[i] = " << sum << std::endl;

    // 简单正确性检查（使用相对误差，大数需要宽松容忍度）
    bool ok = true;
    for (size_t i = 0; i < 1000 && i < N; ++i) {
        double expected = std::sqrt(a[i] * a[i] + b[i] * b[i]);
        double rel_err = std::abs(c[i] - expected) / expected;
        if (rel_err > 1e-12) {
            ok = false;
            std::cerr << "Mismatch at i=" << i << ": got " << c[i] 
                      << ", expected " << expected 
                      << ", rel_err=" << rel_err << std::endl;
            break;
        }
    }
    std::cout << "\n验证结果: " << (ok ? "PASSED" : "FAILED") << std::endl;

    delete[] a;
    delete[] b;
    delete[] c;

    return ok ? 0 : 1;
}
