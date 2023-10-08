#include "threads/fixed-point.h"

const int f = 1 << 14;

int int2fp(int n) {
    return n * f;
}

int fp2int_round0(int x) {
    return x / f;
}

int fp2int_roundnear(int x) {
    return x >= 0 ? (x+f/2)/f : (x-f/2)/f;
}

int add_fp_fp(int x, int y) {
    return x+y;
}

int sub_fp_fp(int x, int y) {
    return x - y;
}

int add_fp_int(int x, int n) {
    return x + n*f;
}

int sub_fp_int(int x, int n) {
    return x - n*f;
}

int mul_fp_fp(int x, int y) {
    return ((int64_t)x)*y / f;
}

int mul_fp_int(int x, int n) {
    return x * n;
}

int div_fp_fp(int x, int y) {
    return ((int64_t)x)*f/y;
}

int div_fp_int(int x, int n) {
    return x / n;
}