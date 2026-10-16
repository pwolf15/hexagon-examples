#ifndef TEST_CHECK_H
#define TEST_CHECK_H

// minimal on-target test checks (hex_test.h in qualcomm/qemu-hexagon-testing (BSD-3-Clause-clear))

#include <cstdio>
#include <cstdint>
#include <cinttypes>
#include "inc/ulp.h"

static int err = 0;
static const int kMaxReports = 10;

static inline void __check32(int line, uint32_t val, uint32_t expect) {
    if (val != expect) {
        if (err < kMaxReports)
            std::printf("Error at line %d: 0x%08" PRIx32 " != 0x%08" PRIx32 "\n", line, val, expect);
        err++;
    }
}
#define check32(RES, EXP) __check32(__LINE__, (RES), (EXP))


static inline void __check_ulp(int line, float got, float want, int64_t max_ulp) {
    int64_t d = ulp_distance(got, want);
    if (d > max_ulp) {
        if (err < kMaxReports)
            std::printf("ERROR at line %d: %.9g vs %.9g, %lld ULP > %lld\n",
                        line, (double)got, (double)want, (long long)d, (long long)max_ulp);
        err++;
    }
}
#define check_ulp(GOT, WANT, MAX_ULP) __check_ulp(__LINE__, (GOT), (WANT), (MAX_ULP))
#endif // TEST_CHECK_H