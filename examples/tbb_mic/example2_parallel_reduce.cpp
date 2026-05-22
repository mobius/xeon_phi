/**
 * example2_parallel_reduce.cpp
 * 
 * MIC 纯原生 TBB 示例：parallel_reduce (归约)
 * 
 * 注意：ICC 16.0 MIC 编译器对 C++11 lambda 支持不完整，
 *       因此使用 functor class 替代 lambda。
 * 
 * 编译: icpc -mmic -tbb -O2 -o example2_parallel_reduce.mic example2_parallel_reduce.cpp
 * 运行: micnativeloadex ./example2_parallel_reduce.mic
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
            a[i] = static_cast<double>(i % 1000) / 1000.0;
            b[i] = static_cast<double>((N - i) % 1000) / 1000.0;
        }
    }
};

// ---- 自定义归约体：计算向量的点积 + 最大值 ----
struct DotMaxBody {
    const double* a;
    const double* b;
    double dot;
    double max_val;

    DotMaxBody(const double* a_, const double* b_)
        : a(a_), b(b_), dot(0.0), max_val(0.0) {}

    // 分裂构造函数 (required by TBB)
    DotMaxBody(DotMaxBody& other, tbb::split)
        : a(other.a), b(other.b), dot(0.0), max_val(0.0) {}

    // 运算符()：处理一个范围
    void operator()(const tbb::blocked_range<size_t>& r) {
        double local_dot = dot;
        double local_max = max_val;
        for (size_t i = r.begin(); i != r.end(); ++i) {
            local_dot += a[i] * b[i];
            double val = std::abs(a[i]);
            if (val > local_max) local_max = val;
        }
        dot = local_dot;
        max_val = local_max;
    }

    // 合并结果 (required by TBB)
    void join(const DotMaxBody& other) {
        dot += other.dot;
        if (other.max_val > max_val) max_val = other.max_val;
    }
};

int main(int argc, char* argv[])
{
    const size_t N = 50000000;  // 5000万元素
    std::vector<double> a(N);
    std::vector<double> b(N);

    // 初始化数据
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, N),
        InitFunctor(a.data(), b.data(), N)
    );

    // 使用 parallel_reduce 计算点积和最大值
    DotMaxBody body(a.data(), b.data());
    tbb::parallel_reduce(tbb::blocked_range<size_t>(0, N), body);

    std::cout << "Vector size: " << N << std::endl;
    std::cout << "Dot product: " << body.dot << std::endl;
    std::cout << "Max |a[i]|:  " << body.max_val << std::endl;

    return 0;
}
