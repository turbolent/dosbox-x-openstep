#ifndef DOSBOX_OPENSTEP_MATH_H
#define DOSBOX_OPENSTEP_MATH_H
#include_next <math.h>
#if defined(NeXT)
/* OPENSTEP libm predates the C99 float entry points. */
static inline float sqrtf(float x) { return (float)sqrt((double)x); }
static inline float powf(float x, float y) { return (float)pow((double)x, (double)y); }
static inline double exp2(double x) { return pow(2.0, x); }
static inline double log2(double x) { return log(x) * 1.4426950408889634074; }
static inline float truncf(float x)
{
    if (x > -1.0f && x < 1.0f) return x * 0.0f;
    return (float)(x < 0 ? ceil((double)x) : floor((double)x));
}
/* Keep PIC's extended precision instead of narrowing its clock to double. */
static inline long double fmodl(long double x, long double y)
{
    long double result;
    if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
    if (__builtin_isinf(x) || y == 0) { long double zero = 0; return zero / zero; }
    if (__builtin_isinf(y)) return x;
    __asm__ __volatile__("1: fprem\n\tfnstsw %%ax\n\ttest $0x400,%%ax\n\tjnz 1b"
        : "=t" (result) : "0" (x), "u" (y) : "ax", "cc");
    return result;
}
#ifndef isnan
#define isnan(x) __builtin_isnan(x)
#endif
#ifndef isinf
#define isinf(x) __builtin_isinf(x)
#endif
#ifndef isfinite
static inline int openstep_isfinite(long double value)
{
    return !__builtin_isnan(value) && !__builtin_isinf(value);
}
#define isfinite(x) openstep_isfinite(x)
#endif
#endif
#endif
