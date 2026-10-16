// qf32 rounding experiment: c = a + CHAIN_K*b computed two ways on the cDSP.
//   add_vec : chain of qf32 adds, normalized to IEEE sf after EVERY step
//   mul_vec : (repurposed) same chain, accumulator stays in qf32, ONE convert
// Host compares both against a double-precision reference, in ULPs.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "inc/ulp.h"
#include "inc/chain_k.h"
#include "remote.h"
#include "vecadd.h"

#ifndef USE_RPCMEM
#define USE_RPCMEM 1
#endif

#if USE_RPCMEM
#include "rpcmem.h"
#endif

static const int N = 1 << 24; // 64 MB per buffer

using Clock = std::chrono::steady_clock;
static double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

static float *alloc_buf(size_t bytes) {
#if USE_RPCMEM
  return (float *)rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS,
                               bytes);
#else
  void *p = nullptr;
  return posix_memalign(&p, 128, bytes) ? nullptr : (float *)p;
#endif
}

static void free_buf(float *p) {
#if USE_RPCMEM
  if (p)
    rpcmem_free(p);
#else
  free(p);
#endif
}

int main() {
  printf("qf32 chain experiment: c = a + %d*b, N=%d, variant=%s\n", CHAIN_K, N,
         USE_RPCMEM ? "rpcmem" : "malloc");

  remote_rpc_control_unsigned_module data{};
  data.domain = 3;
  data.enable = 1;
  int rc = remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, &data,
                                  sizeof(data));
  printf("remote_session_control rc=%d\n", rc);

  const size_t bytes = (size_t)N * sizeof(float);
  float *sa = alloc_buf(bytes); // input a
  float *sb = alloc_buf(bytes); // input b
  float *sc = alloc_buf(bytes); // result, per-step-convert variant
  float *sd = alloc_buf(bytes); // result, stay-in-qf32 variant
  if (!sa || !sb || !sc || !sd) {
    printf("alloc failed — lower N and retry\n");
    return 1;
  }

  // Values chosen so the exact sum a + K*b = 81*i overflows float's 2^24
  // integer-exact range for most of the array -> rounding actually happens.
  for (int i = 0; i < N; ++i) {
    sa[i] = float(i);
    sb[i] = float(10.0 * i);
  }

  char uri[256];
  snprintf(uri, sizeof(uri), "%s%s", vecadd_URI, "&_dom=cdsp");
  remote_handle64 h = 0;
  int ret = vecadd_open(uri, &h);
  printf("vecadd_open ret=%d\n", ret);
  if (ret != 0)
    return 1;

  // Sanity: what does the DSP say it is? (pages out-param now returns
  // qurt arch_version — build said v75, spec says v68.)
  unsigned int avail = 0, max_page = 0, arch = 0;
  if (vecadd_get_vtcm(h, &avail, &max_page, &arch) == 0)
    printf("DSP reports arch v%u, VTCM avail=%u\n", arch, avail);

  for (int iter = 0; iter < 3; ++iter) {
    auto t0 = Clock::now();
    if ((ret = vecadd_add_vec(h, sa, N, sb, N, sc, N)) != 0) {
      printf("add_vec (per-step convert) failed: %d\n", ret);
      return 1;
    }
    double tA = ms_since(t0);
    t0 = Clock::now();
    if ((ret = vecadd_mul_vec(h, sa, N, sb, N, sd, N)) != 0) {
      printf("mul_vec (stay-in-qf32) failed: %d\n", ret);
      return 1;
    }
    double tB = ms_since(t0);
    printf("iter %d: per-step-convert %7.2f ms | stay-in-qf32 %7.2f ms\n", iter,
           tA, tB);
  }

  // --- ULP microscope -----------------------------------------------------
  // --- jamming-rounding microscope ------------------------------------------
  // Drop-in replacement for the ULP section in host/main.cpp.
  //
  // Two upgrades over the old unsigned check:
  //   1. SIGNED displacement (dsp - correctly-rounded ref), so directional
  //      jamming bias is visible instead of folded into |d|.
  //   2. Trailing-bit-class histogram restricted to lanes whose exact result is
  //      representable in binary32 (discarded fraction == 0) — the population
  //      the slide's claim is about.
  //
  // For a clean single-instruction measurement, set CHAIN_K to 1 in
  // inc/chain_k.h first: then add_vec is one Vqf32_vadd_VsfVsf + convert and
  // mul_vec is the identical single-add path. Prediction from the slide, per
  // instruction form: trailing-class 00 lanes displaced by +/-1 ULP every time,
  // classes 01/10/11 untouched, signed mean over exact lanes = +/-0.25 ULP.
  // With CHAIN_K=8 the per-step jams compound and the histogram blurs — run
  // K=1 for the claim, K=8 for the accumulation story.

  {
    long long signed_sum_all = 0;
    long pos = 0, neg = 0, zero = 0;

    long exact_lanes = 0;
    long long signed_sum_exact = 0;
    long cls_count[4] = {0, 0, 0, 0};
    long cls_moved[4] = {0, 0, 0, 0};
    long long cls_signed[4] = {0, 0, 0, 0};

    for (int i = 0; i < N; ++i) {
      double ref = (double)sa[i] + (double)CHAIN_K * (double)sb[i];
      float rf = (float)ref;                        // correctly-rounded ref
      int64_t d = float_key(sc[i]) - float_key(rf); // SIGNED, in ULPs

      signed_sum_all += d;
      if (d > 0)
        ++pos;
      else if (d < 0)
        ++neg;
      else
        ++zero;

      if ((double)rf == ref) { // exact case: discarded
        ++exact_lanes;         // fraction is zero
        signed_sum_exact += d;
        int32_t bits;
        std::memcpy(&bits, &rf, sizeof bits);
        int cls = bits & 3; // trailing 2 bits
        ++cls_count[cls];
        cls_signed[cls] += d;
        if (d != 0)
          ++cls_moved[cls];
      }
    }

    printf("\nsigned displacement, all %d lanes (dsp - rounded ref, ULPs):\n",
           N);
    printf("  mean %+.4f   pos %ld  neg %ld  zero %ld\n",
           (double)signed_sum_all / N, pos, neg, zero);

    printf("exact-representable results: %ld lanes (%.1f%%), "
           "signed mean %+.4f ULP\n",
           exact_lanes, 100.0 * exact_lanes / N,
           exact_lanes ? (double)signed_sum_exact / exact_lanes : 0.0);

    static const char *cls_name[4] = {"..00", "..01", "..10", "..11"};
    for (int c = 0; c < 4; ++c) {
      if (!cls_count[c])
        continue;
      printf("  trailing %s : %9ld lanes, %9ld moved (%5.1f%%), "
             "signed mean %+.4f ULP\n",
             cls_name[c], cls_count[c], cls_moved[c],
             100.0 * cls_moved[c] / cls_count[c],
             (double)cls_signed[c] / cls_count[c]);
    }
    // Slide prediction @ CHAIN_K=1: "..00" row -> 100% moved, +/-1.0 mean;
    // other rows -> 0% moved. Mean |displacement| over exact lanes = 0.25 ULP.
  }

  // --- jamming demo: x + 0.0 through the existing DSP kernel ---------------
  static const uint32_t probe[8] = {
      0x3F800000, // 1.0      trailing ..00 -> expect +1 ULP
      0x3F800001, // 1+1ulp            ..01 -> unchanged
      0x3F800002, //                   ..10 -> unchanged
      0x3F800003, //                   ..11 -> unchanged
      0x40000000, // 2.0               ..00 -> +1 ULP
      0x3F000000, // 0.5               ..00 -> +1 ULP
      0x42C80000, // 100.0             ..00 -> +1 ULP
      0x42C80001, // 100+1ulp          ..01 -> unchanged
  };
  for (int i = 0; i < 32; ++i) { // one HVX vector, minimum legal N
    memcpy(&sa[i], &probe[i % 8], 4);
    sb[i] = 0.0f;
  }
  vecadd_add_vec(h, sa, 32, sb, 32, sc, 32);

  printf("\njamming demo: x + 0.0 on the cDSP\n");
  for (int i = 0; i < 8; ++i) {
    uint32_t in, out;
    memcpy(&in, &sa[i], 4);
    memcpy(&out, &sc[i], 4);
    printf("  %.9g (0x%08X) + 0 = %.9g (0x%08X)  %s\n", sa[i], in, sc[i], out,
           out == in       ? "unchanged"
           : out == in + 1 ? "+1 ULP  <-- jammed"
                           : "UNEXPECTED");
  }

  // --- minimal Pass 5: cross-binade sums through the existing kernel -------
  // 4 sums that cross UP through 2.0, 4 that cross DOWN through 1.0.
  // All exact by construction; any displacement is the rounding.
  static const struct {
    uint32_t x, y;
    const char *what;
  } p5[12] = {
      // x = 2.0 - (8-r) ULPs of [1,2),  y = +8 ULPs (2^-20): sum = 2.0 + r/2
      // ULPs of [2,4)
      {0x3FFFFFF8u, 0x35800000u,
       "cross-up  -> ..00"}, // r=0: exact = 0x40000000
      {0x3FFFFFFAu, 0x35800000u,
       "cross-up  -> ..01"}, // r=2: exact = 0x40000001
      {0x3FFFFFFCu, 0x35800000u,
       "cross-up  -> ..10"}, // r=4: exact = 0x40000002
      {0x3FFFFFFEu, 0x35800000u,
       "cross-up  -> ..11"}, // r=6: exact = 0x40000003
      // x = 1.0,  y = -m * 2^-24: sum = 1.0 - m ULPs of [0.5,1)  (exact for all
      // m)
      {0x3F800000u, 0xB3800000u,
       "cross-down-> ..11"}, // m=1: exact = 0x3F7FFFFF
      {0x3F800000u, 0xB4000000u,
       "cross-down-> ..10"}, // m=2: exact = 0x3F7FFFFE
      {0x3F800000u, 0xB4400000u,
       "cross-down-> ..01"}, // m=3: exact = 0x3F7FFFFD
      {0x3F800000u, 0xB4800000u,
       "cross-down-> ..00"}, // m=4: exact = 0x3F7FFFFC

      {0x3FFFFFF8u, 0x36000000u, "cross-up  -> ..100"}, // exact = 0x40000004
      {0x3FFFFFFAu, 0x36000000u, "cross-up  -> ..101"}, // exact = 0x40000005
      {0x3FFFFFFCu, 0x36000000u, "cross-up  -> ..110"}, // exact = 0x40000006
      {0x3FFFFFFEu, 0x36000000u, "cross-up  -> ..111"}, // exact = 0x40000007
  };
  for (int i = 0; i < 32; ++i) {
    memcpy(&sa[i], &p5[i % 12].x, 4);
    memcpy(&sb[i], &p5[i % 12].y, 4);
  }
  vecadd_add_vec(h, sa, 32, sb, 32, sc, 32);

  printf("\npass 5 (minimal): sums crossing a binade boundary\n");
  for (int i = 0; i < 12; ++i) {
    double exact = (double)sa[i] + (double)sb[i];
    float ref = (float)exact; // exact by construction
    uint32_t rb, ob;
    memcpy(&rb, &ref, 4);
    memcpy(&ob, &sc[i], 4);
    long long d = (long long)ob - (long long)rb; // encoding steps
    printf("  %s  exact=0x%08X got=0x%08X  %+lld  %s\n", p5[i].what, rb, ob, d,
           d == 0 ? "" : "<-- moved");
  }

  printf("vec accum\n");

  // accumulate a vector
  std::vector<float> a_vec(1024);

  for (int i = 0; i < 1024; ++i) {
    a_vec[i] = (1.0f + i) * i;
  }
  float results[3];

  float result_scalar, result_hvx, result_hvx_interleaved;
  int debug_scalar, debug_hvx, debug_hvx_interleaved;
  vecadd_sum_vec_scalar(h, a_vec.data(), a_vec.size(), &result_scalar, 1, &debug_scalar, 1);
  vecadd_sum_vec_hvx(h, a_vec.data(), a_vec.size(), &result_hvx, 1, &debug_hvx, 1);
  vecadd_sum_vec_hvx_interleaved(h, a_vec.data(), a_vec.size(), &result_hvx_interleaved, 1, &debug_hvx_interleaved, 1);

  double dsum = 0.0;
  for (int i = 0; i < 1024; ++i)
    dsum += (double)a_vec[i];
  float ref = (float)dsum;

  printf("\n\naccum dsp scalar %f hvx: %f interleaved %f host oracle %f\n\ncycles scalar %d cycles "
         "hvx %d cycles hvx interleaved %d\n\n",
         result_scalar, result_hvx, result_hvx_interleaved, ref, debug_scalar, debug_hvx, debug_hvx_interleaved);

  // dot product
  size_t dot_vec_len = 8192;
  std::vector<uint8_t> vec_a_u8(dot_vec_len), vec_b_u8(dot_vec_len);
  for (int i = 0; i < dot_vec_len; ++i) {
    vec_a_u8[i] = i + 1;
    vec_b_u8[i] = i * 2 + 1;
  }
  uint32_t dot_result = 0, dot_result_hvx = 0, dot_result_rust = 0;
  int cycles_dot_scalar = 0, cycles_dot_hvx = 0, cycles_dot_rust;
  vecadd_dot_vec_scalar(h, vec_a_u8.data(), dot_vec_len, vec_b_u8.data(), dot_vec_len, &dot_result, &cycles_dot_scalar);
  vecadd_dot_vec_hvx(h, vec_a_u8.data(), dot_vec_len, vec_b_u8.data(), dot_vec_len, &dot_result_hvx, &cycles_dot_hvx);
  vecadd_dot_vec_rust(h, vec_a_u8.data(), dot_vec_len, vec_b_u8.data(), dot_vec_len, &dot_result_rust, &cycles_dot_rust);

  printf("\n\ndot dsp scalar %lu hvx %lu rust %lu cycles dot scalar %d hvx %d rust %d\n\n", dot_result, dot_result_hvx, dot_result_rust, cycles_dot_scalar, cycles_dot_hvx, cycles_dot_rust);

  vecadd_close(h);
  free_buf(sa);
  free_buf(sb);
  free_buf(sc);
  free_buf(sd);
  return 0;
}