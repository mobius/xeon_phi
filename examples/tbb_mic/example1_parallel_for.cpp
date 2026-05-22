/**
 * example1_parallel_for.cpp
 * 
 * MIC 纯原生 TBB 示例：parallel_for
 * 
 * 注意：ICC 16.0 MIC 编译器对 C++11 lambda 支持不完整，
 *       因此使用 functor class 替代 lambda。
 * 
 * 编译: icpc -mmic -tbb -O2 -o example1_parallel_for.mic example1_parallel_for.cpp
 * 运行: micnativeloadex ./example1_parallel_for.mic
 */

#include <tbb/tbb.h>
#include <iostream>
#include <vector>
#include <cmath>

// ---- Functor for initialization ----
struct InitFunctor {
    double* a;
    double* b;
    size_t  N;

    InitFunctor(double* a_, double* b_, size_t N_) : a(a_), b(b_), N(N_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            a[i] = static_cast<double>(i);
            b[i] = static_cast<double>(N - i);
        }
    }
};

// ---- Functor for computation ----
struct ComputeFunctor {
    const double* a;
    const double* b;
    double*       c;

    ComputeFunctor(const double* a_, const double* b_, double* c_)
        : a(a_), b(b_), c(c_) {}

    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = std::sqrt(a[i] * a[i] + b[i] * b[i]);
        }
    }
};

int main(int argc, char* argv[])
{
    const size_t N = 100000000;  // 1亿元素
    std::vector<double> a(N);
    std::vector<double> b(N);
    std::vector<double> c(N);

    // 初始化
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        InitFunctor(a.data(), b.data(), N)
    );

    // 计算: c[i] = sqrt(a[i]*a[i] + b[i]*b[i])
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        ComputeFunctor(a.data(), b.data(), c.data())
    );

    // 验证结果
    double sum = 0.0;
    for (size_t i = 0; i < N; ++i) {
        sum += c[i];
    }

    std::cout << "N = " << N << std::endl;
    std::cout << "Sum of c[i] = " << sum << std::endl;

    return 0;
}
