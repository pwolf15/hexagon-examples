#include "AEEStdErr.h"
#include "vecadd.h"
#include <cstdlib>

extern "C" {
int vecadd_mul_vec(remote_handle64 h,
                   const float *a, int aLen,
                   const float *b, int bLen,
                   float *result, int resultLen) {
  (void)h;
  if (!a || !b || !result) return AEE_EBADPARM;
  if (aLen != bLen || resultLen != aLen) return AEE_EBADPARM;

  for (int i = 0; i < aLen; ++i) result[i] = a[i] * b[i];
  return AEE_SUCCESS;
}
}