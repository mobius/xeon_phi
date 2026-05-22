/**
 * 04_tbb.cpp
 * 
 * MIC 原生 Intel TBB 多线程示例
 * 
 * 编译: icpc -mmic -tbb -O2 -o 04_tbb.mic 04_tbb.cpp
 * 运行: SINK_LD_LIBRARY_PATH=... micnativeloadex ./04_tbb.mic
 */

#include <tbb/tbb.h>
#include <iostream>
#include <vector>

#define N 100000000

struct InitFunctor {
    double* a;
    double* b;
    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            a[i] = (double)i;
            b[i] = (double)(N - i);
        }
    }
};

struct AddFunctor {
    const double* a;
    const double* b;
    double* c;
    void operator()(const tbb::blocked_range<size_t>& r) const {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            c[i] = a[i] + b[i];
        }
    }
};

int main(int argc, char* argv[])
{
    std::vector<double> a(N);
    std::vector<double> b(N);
    std::vector<double> c(N);

    tbb::parallel_for(tbb::blocked_range<size_t>(0, N), InitFunctor{a.data(), b.data()});
    tbb::parallel_for(tbb::blocked_range<size_t>(0, N), AddFunctor{a.data(), b.data(), c.data()});

    double sum = tbb::parallel_reduce(
        tbb::blocked_range<size_t>(0, N),
        0.0,
        [&](const tbb::blocked_range<size_t>& r, double local_sum) -> double {
            for (size_t i = r.begin(); i != r.end(); ++i) local_sum += c[i];
            return local_sum;
        },
        std::plus<double>()
    );

    std::cout << "=== MIC TBB Example ===" << std::endl;
    std::cout << "Vector size: " << N << std::endl;
    std::cout << "Sum:         " << sum << std::endl;
    std::cout << "Expected:    " << (double)N * N << std::endl;
    std::cout << "Verify:      " << (sum == (double)N * N ? "PASSED" : "FAILED") << std::endl;

    return 0;
}
