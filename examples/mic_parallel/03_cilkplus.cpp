/**
 * 03_cilkplus.cpp
 * 
 * MIC 原生 Intel Cilk Plus 多线程示例
 * 
 * 编译: icpc -mmic -O2 -o 03_cilkplus.mic 03_cilkplus.cpp
 * 运行: SINK_LD_LIBRARY_PATH=... micnativeloadex ./03_cilkplus.mic
 */

#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <iostream>
#include <cmath>
#include <vector>

#define N 100000000

int main(int argc, char* argv[])
{
    std::vector<double> a(N);
    std::vector<double> b(N);
    std::vector<double> c(N);

    // 串行初始化
    for (int i = 0; i < N; i++) {
        a[i] = (double)i;
        b[i] = (double)(N - i);
    }

    // Cilk Plus 没有内置计时器，省略计时

    // 并行循环
    _Cilk_for (int i = 0; i < N; i++) {
        c[i] = a[i] + b[i];
    }

    // 使用 Cilk reducer 做并行归约
    cilk::reducer_opadd<double> sum(0.0);
    _Cilk_for (int i = 0; i < N; i++) {
        *sum += c[i];
    }

    std::cout << "=== MIC Cilk Plus Example ===" << std::endl;
    std::cout << "Vector size: " << N << std::endl;
    std::cout << "Sum:         " << sum.get_value() << std::endl;
    std::cout << "Expected:    " << (double)N * N << std::endl;
    std::cout << "Verify:      " << (sum.get_value() == (double)N * N ? "PASSED" : "FAILED") << std::endl;

    return 0;
}
