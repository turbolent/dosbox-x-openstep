/* Deterministic differential fixture: build against upstream with C++14,
 * then compare with the C++98 build and the native OPENSTEP executable. */
#include "dosbox.h"
#include "fpu.h"
#include "fpu/fpu_float80.h"
#include <cassert>

FPU fpu;
unsigned char FPU_ArchitectureType = FPU_ARCHTYPE_387;
static uint64_t digest = UINT64_C(14695981039346656037);
static uint64_t random_state = UINT64_C(0x123456789abcdef0);

static void hash(uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) {
        digest ^= value & 255;
        digest *= UINT64_C(1099511628211);
        value >>= 8;
    }
}

static uint64_t random_bits()
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}

static void integer_result(const float80::IntegerConversionResult& result)
{
    hash((uint64_t)result.value); hash(result.exceptions); hash(result.rounded_up);
}

static void check(FPU_Reg_80 value)
{
    const float80::F32ConversionResult single = float80::convertToF32(value);
    const float80::F64ConversionResult real = float80::convertToF64(value);
    hash(single.value.raw); hash(single.exceptions); hash(single.rounded_up);
    hash(real.value.raw); hash(real.exceptions); hash(real.rounded_up);
    integer_result(float80::convertToI16(value));
    integer_result(float80::convertToI32(value));
    integer_result(float80::convertToI64(value));
    integer_result(float80::convertToI16Truncate(value));
    integer_result(float80::convertToI32Truncate(value));
    integer_result(float80::convertToI64Truncate(value));
    for (unsigned extra = 0; extra < 4; ++extra) {
        FPU_Reg_80 rounded = value;
        float80::round(rounded, extra);
        hash(rounded.raw.l); hash(rounded.raw.h);
    }
}

int main()
{
    const int64_t integers[] = {0, 1, -1, 32767, -32768, INT32_MAX, INT32_MIN,
                                INT64_MAX, INT64_MIN};
    const uint64_t mantissas[] = {0, 1, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000001),
        UINT64_C(0xbfffffffffffffff), UINT64_C(0xc000000000000000), UINT64_MAX};
    const unsigned exponents[] = {0, 1, 0x3bcd, 0x3bfe, 0x3f69, 0x3f81,
        0x3ffe, 0x3fff, 0x400e, 0x401e, 0x403e, 0x407e, 0x43fe, 0x7ffe, 0x7fff};
    for (unsigned mode = 0; mode < 4; ++mode) {
        fpu.cw.init(); fpu.cw.RC = mode;
        for (unsigned i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i) {
            FPU_Reg_80 value = {};
            float80::convertFrom(value, integers[i]);
            const float80::IntegerConversionResult result = float80::convertToI64(value);
            assert(result.value == integers[i] && !result.exceptions);
            check(value);
        }
        for (unsigned sign = 0; sign < 2; ++sign)
            for (unsigned e = 0; e < sizeof(exponents) / sizeof(exponents[0]); ++e)
                for (unsigned m = 0; m < sizeof(mantissas) / sizeof(mantissas[0]); ++m) {
                    FPU_Reg_80 value = {};
                    value.f.mantissa = mantissas[m];
                    value.f.exponent = exponents[e]; value.f.sign = sign;
                    check(value);
                }
        for (unsigned i = 0; i < 4000; ++i) {
            FPU_Reg_80 value = {};
            value.raw.l = random_bits(); value.raw.h = (uint16_t)random_bits();
            check(value);
            FPU_Reg_32 single; single.raw = (uint32_t)random_bits();
            float80::convertFrom(value, single); check(value);
            FPU_Reg_64 real; real.raw = random_bits();
            float80::convertFrom(value, real); check(value);
        }
    }
    printf("float80 conversion digest: %08x%08x\n",
           (unsigned)(digest >> 32), (unsigned)digest);
    /* Upstream e013b8bf8, C++14, the identical fixture on MSYS2 GCC 13.2. */
    assert(digest == UINT64_C(0xc80d2f7eff541609));
    return 0;
}
