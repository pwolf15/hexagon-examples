#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <float.h>
#include <limits>
#include "inc/chain_k.h"
#include "vecadd.h"


bool CHECK_ULP(float a, float b, float eps = FLT_EPSILON, float abs_th = FLT_MIN) {
    assert(std::numeric_limits<float>::epsilon() <= eps);
    assert(eps < 1.f);

    if (a == b) return true;

    auto diff = abs(a-b);
    auto norm = std::min((abs(a) + abs(b)), std::numeric_limits<float>::max());
    return diff < std::max(abs_th, eps * norm);
}

static float chain_ref(float a, float b) {
    return (float)((double) a + (double)CHAIN_K * (double)b);
}

int main() {

    const size_t N = 1000, M = 1000;
    alignas(128) static float a[N], b[N], c[N];
    alignas(128) static uint8_t u8a[M], u8b[M];
    (void)u8a, (void)u8b;

    for (int i = 0; i < N; ++i) {
        a[i] = (float)i;
        b[i] = (float)i * 2.0;
    }

    remote_handle64 h;
    vecadd_add_vec(h, a, N, b, N, c, N);

    for (int i = 0; i < N; ++i)
        CHECK_ULP(c[i], chain_ref(a[i], b[i]), CHAIN_K);

    return 0;
}