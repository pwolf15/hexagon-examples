#include "AEEStdErr.h"
#include "vecadd.h"
#include <cstdlib>

extern "C" {



void *aligned_alloc(size_t alignment, size_t size) {

    void *p = 0;

    return posix_memalign(&p, alignment, size) ? 0 : p;

}

int vecadd_open(const char *uri, remote_handle64 *handle) {
  (void)uri;
  void *p = std::malloc(1);
  if (!p) return AEE_ENOMEMORY;
  *handle = (remote_handle64)p;
  return AEE_SUCCESS;
}

int vecadd_close(remote_handle64 handle) {
  if (handle) std::free((void *)handle);
  return AEE_SUCCESS;
}

int vecadd_add_vec(remote_handle64 h,
                   const float *a, int aLen,
                   const float *b, int bLen,
                   float *result, int resultLen) {
  (void)h;
  if (!a || !b || !result) return AEE_EBADPARM;
  if (aLen != bLen || resultLen != aLen) return AEE_EBADPARM;

  for (int i = 0; i < aLen; ++i) result[i] = a[i] + b[i];
  return AEE_SUCCESS;
}

}