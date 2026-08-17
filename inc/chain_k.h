#pragma once
// Number of chained qf32 additions: c = a + CHAIN_K * b.
// Shared by host (reference computation) and both DSP kernels.
#define CHAIN_K 1