#include <cstdio>
#include "vecadd.h"

int main() {

#define N 32
    float a[N] = {0};
    float b[N] = {0};
    float c[N] = { 0 };

    remote_handle64 h;
    vecadd_add_vec(h, a, N, b, N, c, N);

    for (int i = 0; i < N; ++i) {
        a[i] = (float)i;
        b[i] = (float)i * 2.0;
    }

    for(int i = 0; i < N; i++)
        printf("%f\n", c[i]);

    return 0;
}