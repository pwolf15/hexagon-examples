#include "AEEStdErr.h"
#include "vecadd.h"
#include <cstdlib>
#include "HAP_compute_res.h"
#include "hvx_hexagon_protos.h"
#include <hexagon_types.h>
#include "worker_pool.h"

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

#include "HAP_vtcm_mgr.h"
#include <stdio.h>
#include "HAP_farf.h"

#include "qurt_event.h"
#include "qurt_hvx.h"

int vecadd_get_vtcm(

    remote_handle64 h,

    unsigned int* avail,

    unsigned int* max_page,

    unsigned int* pages)

{

    (void)h;

    unsigned int a = 0, m = 0, p = 0;

    int rc = HAP_query_avail_VTCM(&a, &m, &p);

    *avail = (uint32)a;

    *max_page = (uint32)m;
        qurt_arch_version_t av;
  qurt_sysenv_get_arch_version(&av);
  *pages = av.arch_version;   // temporary hijack

  *pages = num_hvx128_contexts;

    // request vtcm
    compute_res_attr_t vtcm_res_info;
    unsigned int context_id = 0;
    (void)context_id; (void)vtcm_res_info;
    // context_id = HAP_compute_res_acquire(&vtcm_res_info, 5000);

    // if (context_id == 0) {
    //   rc = -1;
    // }
    // *pages = (uint32)p;

    return rc;

}



void print_vtcm_info() {
  unsigned int avail = 0;

  unsigned int max_page = 0;

  unsigned int pages = 0;

  int rc = HAP_query_avail_VTCM(

      &avail,

      &max_page,

      &pages);




  FARF(ALWAYS, "VTCM rc=%d avail=%u max_page=%u pages=%u\n",

         rc, avail, max_page, pages);
}

int vecadd_add_vec(remote_handle64 h,
                   const float *a, int aLen,
                   const float *b, int bLen,
                   float *result, int resultLen) {
  (void)h;
  if (!a || !b || !result) return AEE_EBADPARM;
  if (aLen != bLen || resultLen != aLen) return AEE_EBADPARM;

  print_vtcm_info();

#define SCALAR 0
#if SCALAR
  for (int i = 0; i < aLen; ++i) result[i] = a[i] + b[i];

#else 


  int n_batches = aLen / 32;
  for (int i = 0; i < n_batches; ++i) {
    HVX_Vector *va = (HVX_Vector*)(a + i * 32);
    HVX_Vector *vb = (HVX_Vector*)(b + i * 32);
    HVX_Vector *vc = (HVX_Vector*)(result + i * 32);
    vc[0] = Q6_Vsf_equals_Vqf32(Q6_Vqf32_vadd_VsfVsf(va[0], vb[0]));
  }
  
#endif

  return AEE_SUCCESS;
}

}