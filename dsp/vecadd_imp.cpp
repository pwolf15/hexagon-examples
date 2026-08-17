#include "AEEStdErr.h"
#include "HAP_compute_res.h"
#include "HAP_farf.h"
#include "HAP_vtcm_mgr.h"
#include "hvx_hexagon_protos.h"
#include "qurt_event.h"
#include "qurt_hvx.h"
#include "vecadd.h"
#include "worker_pool.h"
#include <cstdlib>
#include <hexagon_types.h>

#include "inc/chain_k.h"

extern "C" {

void *aligned_alloc(size_t alignment, size_t size) {
  void *p = 0;
  return posix_memalign(&p, alignment, size) ? 0 : p;
}

int vecadd_open(const char *uri, remote_handle64 *handle) {
  (void)uri;
  void *p = std::malloc(1);
  if (!p)
    return AEE_ENOMEMORY;
  *handle = (remote_handle64)p;
  return AEE_SUCCESS;
}

int vecadd_close(remote_handle64 handle) {
  if (handle)
    std::free((void *)handle);
  return AEE_SUCCESS;
}

int vecadd_get_vtcm(remote_handle64 h, unsigned int *avail,
                    unsigned int *max_page, unsigned int *pages) {
  (void)h;
  unsigned int a = 0, m = 0, p = 0;
  int rc = HAP_query_avail_VTCM(&a, &m, &p);
  *avail = (uint32)a;
  *max_page = (uint32)m;

  // Report the real arch version this time (build says v75, spec says v68 —
  // let the silicon settle it).
  qurt_arch_version_t av;
  qurt_sysenv_get_arch_version(&av);
  *pages = av.arch_version;

  return rc;
}

int vecadd_sum_vec(remote_handle64 h, const float *a, int aLen, float *results,
                   int resultsLen) {

  // scalar accumulate
  results[0] = 0.0f;
  for (int i = 0; i < 1024; ++i) {
    results[0] += a[i];
  }

  // vector accumulate
  const HVX_Vector *va = (const HVX_Vector *)a;
  HVX_Vector vresult = Q6_V_vsplat_R(0x0);
  results[1] = 0.0f;
  for (int i = 0; i < 32; ++i) {
    vresult = Q6_Vqf32_vadd_Vqf32Vsf(vresult, va[i]);
  }
  (void)va;

  HVX_Vector vresultsf = Q6_Vsf_equals_Vqf32(vresult);
  for (int i = 0; i < 32; ++i) {
    union {
      int32_t i;
      float f;
    } u;
    u.i = Q6_R_vextract_VR(vresultsf, i << 2);
    results[1] += u.f;
  }

  return 0;
}

// ---------------------------------------------------------------------------
// Variant A — "the habit": chain of CHAIN_K qf32 adds, NORMALIZING BACK TO
// IEEE sf AFTER EVERY STEP. Each step rounds to IEEE float32.
//   c = ((((a + b) + b) + ... ) + b        (CHAIN_K additions of b)
// ---------------------------------------------------------------------------
int vecadd_add_vec(remote_handle64 h, const float *a, int aLen, const float *b,
                   int bLen, float *result, int resultLen) {
  (void)h;
  if (!a || !b || !result)
    return AEE_EBADPARM;
  if (aLen != bLen || resultLen != aLen)
    return AEE_EBADPARM;

  int nv = aLen / 32;
  const HVX_Vector *va = (const HVX_Vector *)a;
  const HVX_Vector *vb = (const HVX_Vector *)b;
  HVX_Vector *vc = (HVX_Vector *)result;

  for (int i = 0; i < nv; ++i) {
    HVX_Vector acc = va[i];
    for (int k = 0; k < CHAIN_K; ++k) {
      // qf32 add, then immediately convert back to sf: one IEEE rounding
      // per step, CHAIN_K roundings total.
      acc = Q6_Vsf_equals_Vqf32(Q6_Vqf32_vadd_VsfVsf(acc, vb[i]));
    }
    vc[i] = acc;
  }

  return AEE_SUCCESS;
}
}