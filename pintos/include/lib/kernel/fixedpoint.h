//
// Created by jungle on 26. 10. 12..
//

#ifndef SW_AI_W07_FIXEDPOINT_H
#define SW_AI_W07_FIXEDPOINT_H

#define FXQ14_ONE 16384
#include <stdint.h>

typedef signed int fxq14;

static inline fxq14 int_to_fxq14 (int x) { x *= FXQ14_ONE; return x; }
static inline int fxq14_to_int(fxq14 x) { return (x / FXQ14_ONE); }

static inline fxq14 fxq14_int_add(fxq14 x, int y) { x += int_to_fxq14(y); return x; }
static inline fxq14 fxq14_add(fxq14 x, fxq14 y) { x += y; return x; }

static inline fxq14 fxq14_int_sub(fxq14 x, int y) { x -= int_to_fxq14(y); return x; }
static inline fxq14 fxq14_sub(fxq14 x, fxq14 y) { x -= y; return x; }

static inline fxq14 fxq14_int_mul(fxq14 x, int y) { int64_t fixed_mul = ((int64_t) x * (int64_t)int_to_fxq14(y)) / FXQ14_ONE; return (fxq14) fixed_mul; }
static inline fxq14 fxq14_mul(fxq14 x, fxq14 y) { int64_t fixed_mul = ((int64_t)x * (int64_t)y) / FXQ14_ONE; return  (fxq14) fixed_mul; }

static inline fxq14 fxq14_int_div(fxq14 x, int y) { int64_t fixed_div = ((int64_t)x * FXQ14_ONE) / int_to_fxq14(y); return (fxq14) fixed_div; }
static inline fxq14 fxq14_div(fxq14 x, fxq14 y) { int64_t fixed_div = ((int64_t)x * FXQ14_ONE) / y ; return (fxq14) fixed_div; }


#endif //SW_AI_W07_FIXEDPOINT_H
