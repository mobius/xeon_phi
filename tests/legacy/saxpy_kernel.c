#include <stddef.h>

void saxpy_vec(double *A, double *B, double s, int n) {
    if (n < 8) { int i; for(i=0;i<n;i++) A[i]=s*A[i]+B[i]; return; }
    int i;
    double *a = A, *b = B;
    __asm__ ("vbroadcastsd %[scalar], %%zmm1" : : [scalar] "m" (s) : "zmm1");
    int end = n - 7;
    for(i = 0; i < end; i += 8) {
        __asm__ __volatile__ (
            "vmovapd 0(%[aptr]), %%zmm0\n\t"
            "vfmadd213pd 0(%[bptr]), %%zmm1, %%zmm0\n\t"
            "vmovapd %%zmm0, 0(%[aptr])"
            :
            : [aptr] "r" (a+i), [bptr] "r" (b+i)
            : "zmm0", "memory");
    }
    for(; i < n; i++)
        A[i] = s * A[i] + B[i];
}
