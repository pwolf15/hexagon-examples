#include "AEEStdErr.h"
#include "hvx_hexagon_protos.h"
#include "vecadd.h"
#include <hexagon_types.h>

#include "inc/chain_k.h"

// ---------------------------------------------------------------------------
// TEMPORARILY REPURPOSED for the qf32 experiment: this is NOT a multiply.
//
// Variant B — "the design intent": same chain c = a + CHAIN_K * b, but the
// accumulator STAYS IN qf32 for the whole chain and is converted to IEEE sf
// exactly once at the end. Intermediate results live in qf32's redundant,
// unnormalized representation; only one IEEE rounding total.
// ---------------------------------------------------------------------------
extern "C" {
int vecadd_mul_vec(remote_handle64 h, const float *a, int aLen, const float *b,
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
    // First add takes two sf inputs and produces qf32...
    HVX_Vector acc = Q6_Vqf32_vadd_VsfVsf(va[i], vb[i]);
    // ...then stay in qf32: mixed-mode add (qf32 accumulator + sf operand).
    for (int k = 1; k < CHAIN_K; ++k) {
      acc = Q6_Vqf32_vadd_Vqf32Vsf(acc, vb[i]);
    }
    // Single conversion back to IEEE at the very end.
    vc[i] = Q6_Vsf_equals_Vqf32(acc);
  }

  return AEE_SUCCESS;
}
}