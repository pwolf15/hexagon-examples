#pragma once
#include <cstdint>
#include <string>

// map float bit patterns onto a monotonic integer line so the distance
// between two floats in ULPs is a subtraction. Positive floats keep their 
// bit patter; negatives are reflected below INT32_MIN so ordering holds
// across zero. +0 and -0 are 1 apart; NAN gives meaningless values
static inline int64_t float_key(float f) {
    int32_t i;
    std::memcpy(&i, &f, sizeof i);
    return i >= 0 ? (int64_t)i : (int64_t)INT32_MIN - i;
}

static inline int64_t ulp_distance(float a, float b) {
    int64_t d = float_key(a) - float_key(b);
    return d < 0 ? -d : d;
}