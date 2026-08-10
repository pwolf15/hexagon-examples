#include <stdio.h>
#include <vector>
#include <vecadd.h>
#include "stdlib.h"

#include <cstdio>
using std::snprintf;

int main() {
#define N 128000000
    static float __attribute__((aligned(128))) sa[N], sb[N], sc[N];


    for (int i = 0; i < N; ++i) {
        sa[i] = float(i);
        sb[i] = float(10 * i);
    }



    setenv("ADSP_LIBRARY_PATH", "/data/local/tmp", 1);

    remote_rpc_control_unsigned_module data{};
    data.domain = 3;
    data.enable = 1;

    int rc = remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, &data, sizeof(data));
    printf("remote_session_control rc=%d\n", rc);

    char uri[256];
    std::snprintf(uri, sizeof(uri), "%s%s", vecadd_URI, "&_dom=cdsp");


    remote_handle64 h = 0;
    int ret = vecadd_open(uri, &h);
    printf("vecadd_open ret=%llu h=%llu\n", ret, (unsigned long long)h);
    ret = vecadd_add_vec(
        h,
        sa, N,
        sb, N,
        sc, N
    );

    if (ret != 0) {
        printf("vecadd_add_vec failed: %d\n", ret);
        return 1;
    }

#define PRINT 1
#if PRINT
    for (int i = 0; i < 100; ++i) {
        printf("%f + %f = %f\n", sa[i], sb[i], sc[i]);
    }
#endif

#if MULTIPLY
    // multiply
    ret = vecadd_mul_vec(
        h,
        sa, N,
        sb, N,
        sc, N
    );

    unsigned int avail = 0, max_page = 0, max_pages = 0;
    ret = vecadd_get_vtcm(h, &avail, &max_page, &max_pages);

    printf("memory aavail %d max_page %d num hvx contexts %d\n", avail, max_page, max_pages);

#endif

#if MULTIPLY && PRINT
    for (int i = 0; i < N; ++i) {
        printf("%f * %f = %f\n", sa[i], sb[i], sc[i]);
    }
#endif

    vecadd_close(h);
    
    return 0;
}