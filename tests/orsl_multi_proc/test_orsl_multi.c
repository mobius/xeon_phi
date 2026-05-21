/*
 * ORSL Multi-Process Offload Test
 *
 * Purpose: Verify that multiple host processes can concurrently offload
 *          to the same MIC device, both with and without ORSL enabled.
 *
 * Compile: icc -qoffload -o test_orsl_multi test_orsl_multi.c
 * Run:     ./run_orsl_test.sh
 *
 * Expected: All processes complete successfully regardless of ORSL setting
 *           (single-card environment does not require ORSL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#pragma offload_attribute(push, target(mic))
int compute(int x) { return x * 2; }
#pragma offload_attribute(pop)

int main(int argc, char **argv)
{
    int id = (argc > 1) ? atoi(argv[1]) : 0;
    int result;

    printf("Process %d starting offload...\n", id);
    fflush(stdout);

    #pragma offload target(mic) in(id) out(result)
    {
        result = compute(id);
    }

    printf("Process %d got result: %d\n", id, result);
    return 0;
}
