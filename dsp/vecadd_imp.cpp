#include <hexagon_types.h>

#include <cstdlib>
#include <cstring>

#include "AEEStdErr.h"
#include "HAP_compute_res.h"
#include "HAP_farf.h"
#include "HAP_perf.h"
#include "HAP_vtcm_mgr.h"
#include "hvx_hexagon_protos.h"
#include "inc/chain_k.h"
#include "qurt_event.h"
#include "qurt_hvx.h"
#include "vecadd.h"
#include "worker_pool.h"

extern "C" uint32_t rust_dot_u8(const uint8_t* a, const uint8_t* b, int32_t len);

extern "C" {

void* aligned_alloc(size_t alignment, size_t size) {
  void* p = 0;
  return posix_memalign(&p, alignment, size) ? 0 : p;
}

int vecadd_open(const char* uri, remote_handle64* handle) {
  (void)uri;
  void* p = std::malloc(1);
  if (!p) return AEE_ENOMEMORY;
  *handle = (remote_handle64)p;
  return AEE_SUCCESS;
}

int vecadd_close(remote_handle64 handle) {
  if (handle) std::free((void*)handle);
  return AEE_SUCCESS;
}

int vecadd_get_vtcm(remote_handle64 h, unsigned int* avail, unsigned int* max_page,
                    unsigned int* pages) {
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

constexpr int kLanes = 32;
constexpr int kVecBytes = 128;

inline float bits_to_float(int32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}

inline float hvx_lane(HVX_Vector v, int lane) {
  return bits_to_float(Q6_R_vextract_VR(v, lane * 4));
}

inline float hvx_tree_sum_qf32(HVX_Vector acc_qf32) {
  for (int s = kVecBytes / 2; s >= 4; s >>= 1) {
    HVX_Vector rot = Q6_V_vror_VR(Q6_Vsf_equals_Vqf32(acc_qf32), s);
    acc_qf32 = Q6_Vqf32_vadd_Vqf32Vsf(acc_qf32, rot);
  }
  alignas(128) float lanes[kLanes];
  *reinterpret_cast<HVX_Vector*>(lanes) = Q6_Vsf_equals_Vqf32(acc_qf32);
  return lanes[0];
}

inline int32_t hvx_tree_sum_i32(HVX_Vector acc_u32) {
  for (int s = kVecBytes / 2; s >= 4; s >>= 1) {
    HVX_Vector rot = Q6_V_vror_VR(acc_u32, s);
    acc_u32 = Q6_Vw_vadd_VwVw(acc_u32, rot);
  }
  alignas(128) int32_t lanes[32];
  *reinterpret_cast<HVX_Vector*>(lanes) = acc_u32;
  return lanes[0];
}

inline uint32_t hvx_tree_sum_u32(HVX_Vector acc) {
  for (int s = kVecBytes / 2; s >= 4; s >>= 1) acc = Q6_Vw_vadd_VwVw(acc, Q6_V_vror_VR(acc, s));
  alignas(128) uint32_t lanes[32];
  *reinterpret_cast<HVX_Vector*>(lanes) = acc;
  return lanes[0];
}

int vecadd_sum_vec_scalar(remote_handle64 h, const float* a, int aLen, float* results,
                          int resultsLen, int* debug, int debugLen) {
  // scalar accumulate
  uint64_t cycles_start = HAP_perf_get_pcycles();
  float scalar_sum = 0.0f;
  for (int i = 0; i < aLen; ++i) scalar_sum += a[i];
  uint64_t cycles_end = HAP_perf_get_pcycles();

  debug[0] = (int)(cycles_end - cycles_start);
  results[0] = scalar_sum;

  return 0;
}

int vecadd_sum_vec_hvx(remote_handle64 h, const float* a, int aLen, float* results, int resultsLen,
                       int* debug, int debugLen) {
  // scalar accumulate
  uint64_t cycles_start = HAP_perf_get_pcycles();

  // qf32 accumulate
  const HVX_Vector* va = reinterpret_cast<const HVX_Vector*>(a);
  const int nvec = aLen / kLanes;
  const int tail = aLen % kLanes;

  HVX_Vector vacc = Q6_V_vsplat_R(0);

  for (int i = 0; i < nvec; ++i) vacc = Q6_Vqf32_vadd_Vqf32Vsf(vacc, va[i]);

  if (tail) {  // unaligned load + valign splice
    const float* tp = a + nvec * kLanes;
    HVX_Vector in = *reinterpret_cast<const HVX_UVector*>(tp);
    HVX_Vector pad = Q6_V_vsplat_R(0);
    HVX_Vector t = Q6_V_valign_VVR(in, pad, tail * 4);
    vacc = Q6_Vqf32_vadd_Vqf32Vsf(vacc, t);
  }
  // IEEE final reduction
#ifdef LINEAR_REDUCE
  const HVX_Vector vsf = Q6_Vsf_equals_Vqf32(vacc);
  for (int i = 0; i < kLanes; ++i) vector_sum += hvx_lane(vsf, i);
#else
  float vector_sum = hvx_tree_sum_qf32(vacc);
#endif

  uint64_t cycles_end = HAP_perf_get_pcycles();

  debug[0] = (int)(cycles_end - cycles_start);
  results[0] = vector_sum;

  return 0;
}

int vecadd_sum_vec_hvx_interleaved(remote_handle64 h, const float* a, int aLen, float* results,
                                   int resultsLen, int* debug, int debugLen) {
  uint64_t cycles_start = HAP_perf_get_pcycles();

  // qf32 accumulate
  const HVX_Vector* va = reinterpret_cast<const HVX_Vector*>(a);
  const int nvec = aLen / kLanes;
  const int tail = aLen % kLanes;
  (void)tail;

  HVX_Vector vacc0 = Q6_V_vsplat_R(0);
  HVX_Vector vacc1 = Q6_V_vsplat_R(0);
  for (int i = 0; i + 1 < nvec; i += 2) {
    vacc0 = Q6_Vqf32_vadd_Vqf32Vsf(vacc0, va[i]);
    vacc1 = Q6_Vqf32_vadd_Vqf32Vsf(vacc1, va[i + 1]);
  }
  vacc0 = Q6_Vqf32_vadd_Vqf32Vqf32(vacc0, vacc1);
  float interleaved_sum = hvx_tree_sum_qf32(vacc0);

  uint64_t cycles_end = HAP_perf_get_pcycles();

  results[0] = interleaved_sum;
  debug[0] = (int)(cycles_end - cycles_start);

  return 0;
}

int vecadd_dot_vec_scalar(remote_handle64 h, const uint8_t* a, int aLen, const uint8_t* b, int bLen,
                          unsigned int* result, int* cycles) {
  if (aLen != bLen) return AEE_EBADSIZE;

  int64_t start_cycles = HAP_perf_get_pcycles();
  *result = 0.0f;
  for (int i = 0; i < aLen; ++i) {
    *result += a[i] * b[i];
  }
  int64_t end_cycles = HAP_perf_get_pcycles();
  *cycles = (int)(end_cycles - start_cycles);
  return 0;
}

int vecadd_dot_vec_hvx(remote_handle64 h, const uint8_t* a, int aLen, const uint8_t* b, int bLen,
                       unsigned int* result, int* cycles) {
  if (aLen != bLen) return AEE_EBADSIZE;

  int64_t start_cycles = HAP_perf_get_pcycles();
  *result = 0.0f;
  // for (int i = 0; i < aLen; ++i) {
  //   *result += a[i] * b[i];
  // }
  const HVX_Vector* va = reinterpret_cast<const HVX_Vector*>(a);
  const HVX_Vector* vb = reinterpret_cast<const HVX_Vector*>(b);
  const int nvec = aLen / 128;

  HVX_Vector vacc = Q6_V_vsplat_R(0);
  for (int i = 0; i < nvec; ++i) {
    vacc = Q6_Vuw_vrmpyacc_VuwVubVub(vacc, va[i], vb[i]);
  }

  // final reduce
  alignas(128) int32_t lanes[32];
  *reinterpret_cast<HVX_Vector*>(lanes) = vacc;

  uint32_t sum = 0;
#ifdef LINEAR_REDUCE
  for (int i = 0; i < 32; ++i) {
    sum += lanes[i];
  }
#else
  sum = hvx_tree_sum_u32(vacc);
#endif

  int64_t end_cycles = HAP_perf_get_pcycles();

  *result = sum;
  *cycles = (int)(end_cycles - start_cycles);
  return 0;
}

int vecadd_dot_vec_rust(remote_handle64 h, const uint8_t* a, int aLen, const uint8_t* b, int bLen,
                        unsigned int* result, int* cycles) {
  if (aLen != bLen || (aLen % 128)) return AEE_EBADSIZE;

  uint64_t t0 = HAP_perf_get_pcycles();
  *result = rust_dot_u8(a, b, aLen);  // aLen in bytes == elements for u8
  *cycles = (int)(HAP_perf_get_pcycles() - t0);
  (void)h;
  return 0;
}

// ---------------------------------------------------------------------------
// Variant A — "the habit": chain of CHAIN_K qf32 adds, NORMALIZING BACK TO
// IEEE sf AFTER EVERY STEP. Each step rounds to IEEE float32.
//   c = ((((a + b) + b) + ... ) + b        (CHAIN_K additions of b)
// ---------------------------------------------------------------------------
int vecadd_add_vec(remote_handle64 h, const float* a, int aLen, const float* b, int bLen,
                   float* result, int resultLen) {
  (void)h;
  if (!a || !b || !result) return AEE_EBADPARM;
  if (aLen != bLen || resultLen != aLen) return AEE_EBADPARM;

  int nv = aLen / 32;
  const HVX_Vector* va = (const HVX_Vector*)a;
  const HVX_Vector* vb = (const HVX_Vector*)b;
  HVX_Vector* vc = (HVX_Vector*)result;

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