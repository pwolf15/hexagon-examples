// dsp/sim_stubs.cpp — standalone hexagon-sim build only.
// Provides the QuRT/HAP symbols the kernels call; the sim's --timing
// report is the cycle count, so these can return trivial values.
#include <cstdint>

#include "HAP_perf.h"
#include "HAP_vtcm_mgr.h"
#include "qurt.h"

extern "C" {

int HAP_query_avail_VTCM(unsigned int* avail, unsigned int* max_page, unsigned int* pages) {
  *avail = 0;
  *max_page = 0;
  *pages = 0;
  return 0;
}

int qurt_sysenv_get_arch_version(qurt_arch_version_t* v) {
  v->arch_version = 0x68;
  return 0;
}

}  // extern "C"