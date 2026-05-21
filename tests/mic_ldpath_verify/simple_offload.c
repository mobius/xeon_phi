/*
 * MIC Library Path Verification Test
 *
 * Purpose: Verify that offload works after setting MIC_LD_LIBRARY_PATH
 *          to a host-visible directory containing ICC MIC libraries.
 *
 * Background:
 *   ICC is installed inside a podman container, so host cannot directly
 *   access /opt/intel/.../intel64_lin_mic/. The MIC libraries must be
 *   copied from the container to a host-visible path.
 *
 * Compile:
 *   podman exec centos7-phi-dev bash -c \
 *     'source /opt/intel/bin/compilervars.sh intel64; \
 *      icc -qoffload -o /work/tests/mic_ldpath_verify/test_mic_ldpath \
 *          /work/tests/mic_ldpath_verify/simple_offload.c'
 *
 * Run:
 *   export MIC_LD_LIBRARY_PATH=/home/joey/Work/intel_phi/icc_mic_libs
 *   ./test_mic_ldpath
 *
 * Expected: "Result: 42"
 */
#include <stdio.h>

#pragma offload_attribute(push, target(mic))
int compute(void) { return 42; }
#pragma offload_attribute(pop)

int main(void)
{
    int result = 0;

    #pragma offload target(mic) out(result)
    {
        result = compute();
    }

    printf("Result: %d\n", result);
    return (result == 42) ? 0 : 1;
}
