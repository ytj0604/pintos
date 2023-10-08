#ifndef FIXED_POINTS_H
#define FIXED_POINTS_H

#include <stdint.h>

//declarations
int int2fp(int);
int fp2int_round0(int);
int fp2int_roundnear(int);
int add_fp_fp(int, int);
int sub_fp_fp(int, int);
int add_fp_int(int, int);
int sub_fp_int(int, int);
int mul_fp_fp(int, int);
int mul_fp_int(int, int);
int div_fp_fp(int, int);
int div_fp_int(int, int);

#endif /* threads/fixed-point.h */