#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

#define F (1 << 14)  // 16384 (17.14 형식)

/* 정수 n -> 고정소수점 변환 */
static inline int int_to_fp(int n) {
    return n * F;
}

/* 고정소수점 x -> 정수로 변환 (버림 (-2.9이면 -2, 2.5면 2)) */
static inline int fp_to_int(int x) {
    return x / F;
}

/* 고정소수점 x -> 정수 변환 (가장 가까운 정수로 반올림 (-2.9면 -3, 2.4면 2)) */
static inline int fp_to_int_round(int x) {
    if (x >= 0)
        return (x + F / 2) / F;
    else
        return (x - F / 2) / F;
}

/* 고정소수점끼리 덧셈 (x + y) */
static inline int add_fp(int x, int y) {
    return x + y;
}

/* 고정소수점끼리 뺄셈 (x - y) */
static inline int sub_fp(int x, int y) {
    return x - y;
}

/* 고정소수점 x에 정수 n 더하기 (x + n) */
static inline int add_fp_int(int x, int n) {
    return x + n * F;
}

/* 고정소수점 x에서 정수 n 빼기 (x - n) */
static inline int sub_fp_int(int x, int n) {
    return x - n * F;
}

/* 고정소수점끼리 곱셈 (x * y) */
static inline int mult_fp(int x, int y) {
    return (int)(((int64_t) x) * y / F);
}

/* 고정소수점 x에 정수 n 곱하기 (x * n) */
static inline int mult_fp_int(int x, int n) {
    return x * n;
}

/* 고정소수점끼리 나눗셈 (x / y) */
static inline int div_fp(int x, int y) {
    return (int)(((int64_t) x) * F / y);
}

/* 고정소수점 x를 정수 n으로 나누기 (x / n) */
static inline int div_fp_int(int x, int n) {
    return x / n;
}

#endif /* threads/fixed_point.h */