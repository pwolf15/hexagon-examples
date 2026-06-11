#include <stdio.h>
#include <vector>
#include <vecadd.h>
#include "stdlib.h"

int main() {
    constexpr int N = 16;
    std::vector<float> a(N), b(N), out(N);

    for (int i = 0; i < N; ++i) {
        a[i] = float(i);
        b[i] = float(10 * i);
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
        a.data(), N,
        b.data(), N,
        out.data(), N
    );

    if (ret != 0) {
        printf("vecadd_add_vec failed: %d\n", ret);
        return 1;
    }

    for (int i = 0; i < N; ++i) {
        printf("%f + %f = %f\n", a[i], b[i], out[i]);
    }

    // multiply
    ret = vecadd_mul_vec(
        h,
        a.data(), N,
        b.data(), N,
        out.data(), N
    );

    for (int i = 0; i < N; ++i) {
        printf("%f * %f = %f\n", a[i], b[i], out[i]);
    }

    vecadd_close(h);
    
    return 0;
}