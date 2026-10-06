#include <math.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    long double inf = __builtin_infl(), nan = __builtin_nanl("");
    assert(fmodl(10.5L, 3.0L) == 1.5L);
    assert(fmodl(-10.5L, 3.0L) == -1.5L);
    assert(fmodl(10.5L, -3.0L) == 1.5L);
    assert(fmodl(9223372036854775808.0L + 1.0L, 3.0L) == 0.0L);
    assert(fmodl(1.0e300L, 1.0L) == 0.0L);
    assert(fmodl(1.0e-300L, 2.0L) == 1.0e-300L);
    assert(fmodl(1.0L, inf) == 1.0L);
    assert(__builtin_isnan(fmodl(inf, 1.0L)));
    assert(__builtin_isnan(fmodl(1.0L, 0.0L)));
    assert(__builtin_isnan(fmodl(nan, 1.0L)));
    assert(__builtin_signbit(fmodl(-0.0L, 1.0L)));
    assert(truncf(3.9f) == 3.0f && truncf(-3.9f) == -3.0f);
    assert(truncf(0.75f) == 0.0f && truncf(-0.75f) == 0.0f);
    assert(__builtin_signbit(truncf(-0.75f)));
    assert(__builtin_signbit(truncf(-0.0f)));
    assert(truncf(16777216.0f) == 16777216.0f);
    assert(__builtin_isinf(truncf(__builtin_inff())));
    assert(__builtin_isnan(truncf(__builtin_nanf(""))));
    assert(isfinite(0.0) && isfinite(-1.0e300) && isfinite(1.0e300L));
    assert(!isfinite(inf) && !isfinite(-inf) && !isfinite(nan));
    assert(exp2(0.0) == 1.0 && exp2(10.0) == 1024.0);
    assert(exp2(-14.0) == 1.0 / 16384.0);
    assert(fabs(exp2(0.5) - sqrt(2.0)) < 1e-14);
    assert(fabs(log2(1024.0) - 10.0) < 1e-13);
    assert(fabs(log2(1.5) - 0.58496250072115618) < 1e-14);
    puts("OPENSTEP clock, mouse and sound math passed");
    return 0;
}
