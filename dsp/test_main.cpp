#include <cstdio>

#include "dsp/test_check.h"
#include "inc/chain_k.h"
#include "vecadd.h"

static float chain_ref(float a, float b) {
  return (float)((double)a + (double)CHAIN_K * (double)b);
}

int main() {
  const size_t N = 1024, M = 1000;
  alignas(128) static float a[N], b[N], c[N];
  alignas(128) static uint8_t u8a[M], u8b[M];
  (void)u8a, (void)u8b;

  for (int i = 0; i < N; ++i) {
    a[i] = 16777216.f + i;
    b[i] = 0.1f * i;
  }

  remote_handle64 h = 0;
  vecadd_add_vec(h, a, N, b, N, c, N);

  for (int i = 0; i < N; ++i) check_ulp(c[i], chain_ref(a[i], b[i]), CHAIN_K);

  return err;
}