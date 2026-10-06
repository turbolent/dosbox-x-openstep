#include "dosbox.h"
#if C_FPU

#include <limits>

#include "fpu.h"
#include "fpu_float80.h"

const FPU_Reg_64 FPU_QNaN_64 = {{
    0x0008000000000000ULL,  // quiet-NaN bit
    0x7FFU,                    // all-one exponent
    1U                         // sign bit
}};

const FPU_Reg_80 FPU_QNaN_80 = {{
    0xC000000000000000ULL,  // integer bit and quiet-NaN bit
    0x7FFFU,                   // all-one exponent
    1U                         // sign bit
}};

namespace float80 {

const unsigned int ExpBias = 16383U;
const FPU_Reg_80::raw_t const1 = {
    0x8000000000000000ULL,
    ExpBias
};
const FPU_Reg_80::raw_t PI = {
    0xC90FDAA22168C234ULL,
    0x4000U
};
const uint8_t PI_Extra2 = 3;
const FPU_Reg_80::raw_t L2T = {
    0xD49A784BCD1B8AFEULL,
    0x4000U
};
const uint8_t L2T_Extra2 = 1;
const FPU_Reg_80::raw_t L2E = {
    0xB8AA3B295C17F0BBULL,
    0x3FFFU
};
const uint8_t L2E_Extra2 = 2;
const FPU_Reg_80::raw_t LG2 = {
    0x9A209A84FBCFF798ULL,
    0x3FFDU
};
const uint8_t LG2_Extra2 = 3;
const FPU_Reg_80::raw_t LN2 = {
    0xB17217F7D1CF79ABULL,
    0x3FFEU
};
const uint8_t LN2_Extra2 = 3;

namespace {

const uint64_t IntegerBit = 0x8000000000000000ULL;
const uint64_t QuietNaNBit = 0x4000000000000000ULL;

void convertFromIEEE(FPU_Reg_80& result,
                     uint64_t fraction,
                     uint16_t source_exponent,
                     bool sign,
                     unsigned int fraction_bits,
                     int source_bias,
                     uint16_t max_exponent)
{
    const unsigned int shift = 63U - fraction_bits;
    result.f.sign = sign;

    if (source_exponent == max_exponent) {
        // Map the source quiet-NaN bit to bit 62 of the extended significand.
        result.f.exponent = 0x7FFFU;
        result.f.mantissa = IntegerBit | (fraction << shift);
        return;
    }

    if (source_exponent != 0) {
        result.f.exponent = static_cast<uint16_t>(
                static_cast<int>(ExpBias) + static_cast<int>(source_exponent) - source_bias);
        result.f.mantissa = IntegerBit | (fraction << shift);
        return;
    }

    if (fraction == 0) {
        result.f.exponent = 0;
        result.f.mantissa = 0;
        return;
    }

    // A binary32 or binary64 subnormal is normal in the 80-bit format.
    uint64_t mantissa = fraction << shift;
    int exponent = 1 - source_bias;
    while (!(mantissa & IntegerBit)) {
        mantissa <<= 1;
        --exponent;
    }

    result.f.exponent = static_cast<uint16_t>(static_cast<int>(ExpBias) + exponent);
    result.f.mantissa = mantissa;
}

} // namespace

void setQuietBit(FPU_Reg_80& val)
{
    val.f.mantissa |= QuietNaNBit;
}

namespace {

struct IEEEFormat {
    unsigned int fraction_bits;
    unsigned int exponent_bits;
    int exponent_bias;
    uint16_t max_exponent;
};

struct IEEEConversionResult {
    IEEEConversionResult() : raw(0), exceptions(0), rounded_up(false) {}
    uint64_t raw;
    uint16_t exceptions;
    bool rounded_up;
};

static void setIEEE(IEEEConversionResult& conversion, const IEEEFormat& format,
                    bool sign, uint16_t exponent, uint64_t fraction)
{
    const uint64_t fraction_mask = (1ULL << format.fraction_bits) - 1;
    const unsigned int sign_shift = format.fraction_bits + format.exponent_bits;
    conversion.raw = (static_cast<uint64_t>(sign) << sign_shift) |
                     (static_cast<uint64_t>(exponent) << format.fraction_bits) |
                     (fraction & fraction_mask);
}

static uint64_t roundIEEE(IEEEConversionResult& conversion, bool sign,
                          uint64_t value, unsigned int shift)
{
    const uint64_t extended_integer_bit = 0x8000000000000000ULL;
        const uint64_t truncated = shift < 64 ? value >> shift : 0;
        bool inexact = false;
        bool round_up = false;

        if (shift < 64) {
            const uint64_t half = 1ULL << (shift - 1);
            const uint64_t remainder = value & ((half << 1) - 1);
            inexact = remainder != 0;
            if (fpu.cw.roundMode() == FPUControlWord::Nearest)
                round_up = remainder > half || (remainder == half && (truncated & 1));
        } else {
            inexact = value != 0;
            if (fpu.cw.roundMode() == FPUControlWord::Nearest && shift == 64)
                round_up = value > extended_integer_bit;
        }

        if (inexact) {
            conversion.exceptions |= FPU_EX_PRECISION;
            if (fpu.cw.roundMode() == FPUControlWord::Down)
                round_up = sign;
            else if (fpu.cw.roundMode() == FPUControlWord::Up)
                round_up = !sign;
        }
        conversion.rounded_up = round_up;
        return truncated + static_cast<uint64_t>(round_up);
}

static IEEEConversionResult overflowIEEE(IEEEConversionResult& conversion,
                                         const IEEEFormat& format, bool sign)
{
    const uint64_t fraction_mask = (1ULL << format.fraction_bits) - 1;
        const FPUControlWord::RoundMode round_mode = fpu.cw.roundMode();
        const bool to_infinity = round_mode == FPUControlWord::Nearest ||
                                 (round_mode == FPUControlWord::Up && !sign) ||
                                 (round_mode == FPUControlWord::Down && sign);
        setIEEE(conversion, format, sign, to_infinity ? format.max_exponent
                               : static_cast<uint16_t>(format.max_exponent - 1U),
                   to_infinity ? 0 : fraction_mask);
        conversion.exceptions |= FPU_EX_OVERFLOW | FPU_EX_PRECISION;
        return conversion;
}

IEEEConversionResult convertToIEEE(const FPU_Reg_80& val, const IEEEFormat& format)
{
    const uint64_t extended_integer_bit = 0x8000000000000000ULL;

    const uint64_t quiet_nan_bit = 1ULL << (format.fraction_bits - 1);
    const bool sign = static_cast<bool>(val.f.sign);
    const uint16_t exponent80 = val.f.exponent;
    uint64_t significand = val.f.mantissa;
    IEEEConversionResult conversion;



    if (exponent80 == 0x7FFFU) {
        const uint64_t fraction = significand == extended_integer_bit
                                      ? 0
                                      : ((significand >> (63U - format.fraction_bits)) |
                                         quiet_nan_bit);
        setIEEE(conversion, format, sign, format.max_exponent, fraction);
        return conversion;
    }

    if (significand == 0) {
        setIEEE(conversion, format, sign, 0, 0);
        return conversion;
    }

    // An 80-bit subnormal uses an exponent of 1 - bias rather than -bias.
    int exponent = static_cast<int>(exponent80 ? exponent80 : 1) -
                   static_cast<int>(ExpBias);
    while (!(significand & extended_integer_bit)) {
        significand <<= 1;
        --exponent;
    }





    const int max_normal_exponent = static_cast<int>(format.max_exponent - 1U) -
                                     format.exponent_bias;
    const int min_normal_exponent = 1 - format.exponent_bias;
    if (exponent > max_normal_exponent)
        return overflowIEEE(conversion, format, sign);

    if (exponent >= min_normal_exponent) {
        uint64_t rounded = roundIEEE(conversion, sign, significand, 63U - format.fraction_bits);
        if (rounded == (1ULL << (format.fraction_bits + 1U))) {
            rounded >>= 1;
            if (++exponent > max_normal_exponent)
                return overflowIEEE(conversion, format, sign);
        }
        setIEEE(conversion, format, sign, static_cast<uint16_t>(exponent + format.exponent_bias), rounded);
        return conversion;
    }

    const uint64_t fraction = roundIEEE(conversion, sign,
            significand,
            static_cast<unsigned int>(64 - format.exponent_bias -
                                      static_cast<int>(format.fraction_bits) - exponent));
    if (fraction == (1ULL << format.fraction_bits)) {
        setIEEE(conversion, format, sign, 1, 0);
    } else {
        setIEEE(conversion, format, sign, 0, fraction);
        if (conversion.exceptions & FPU_EX_PRECISION)
            conversion.exceptions |= FPU_EX_UNDERFLOW;
    }
    return conversion;
}

} // namespace

F32ConversionResult convertToF32(const FPU_Reg_80& val)
{
    const IEEEFormat format = {23, 8, 127, 0xFFU};
    const IEEEConversionResult conversion = convertToIEEE(val, format);
    F32ConversionResult result;
    result.value.raw = static_cast<uint32_t>(conversion.raw);
    result.exceptions = conversion.exceptions;
    result.rounded_up = conversion.rounded_up;
    return result;
}

F64ConversionResult convertToF64(const FPU_Reg_80& val)
{
    const IEEEFormat format = {52, 11, 1023, 0x7FFU};
    const IEEEConversionResult conversion = convertToIEEE(val, format);
    F64ConversionResult result;
    result.value.raw = conversion.raw;
    result.exceptions = conversion.exceptions;
    result.rounded_up = conversion.rounded_up;
    return result;
}

namespace {

static IntegerConversionResult invalidInteger(int64_t minimum)
{
    IntegerConversionResult result;
    result.value = minimum;
    result.exceptions = FPU_EX_INVALID;
    return result;
}

IntegerConversionResult convertToInteger(const FPU_Reg_80& val,
                                          unsigned int target_bits,
                                          FPUControlWord::RoundMode round_mode)
{
    const uint64_t extended_integer_bit = 0x8000000000000000ULL;

    IntegerConversionResult conversion;
    const bool sign = static_cast<bool>(val.f.sign);
    const uint16_t exponent80 = val.f.exponent;
    uint64_t significand = val.f.mantissa;
    const int64_t target_min = target_bits == 63
                                    ? std::numeric_limits<int64_t>::min()
                                    : -(1LL << target_bits);



    if (exponent80 == 0x7FFFU)
        return invalidInteger(target_min);

    if (significand == 0)
        return conversion;

    // An 80-bit subnormal uses an exponent of 1 - bias rather than -bias.
    int exponent = static_cast<int>(exponent80 ? exponent80 : 1) -
                   static_cast<int>(ExpBias);
    while (!(significand & extended_integer_bit)) {
        significand <<= 1;
        --exponent;
    }

    if (exponent > static_cast<int>(target_bits) ||
        (exponent == static_cast<int>(target_bits) &&
         (significand != extended_integer_bit || !sign))) {
        return invalidInteger(target_min);
    }

    const unsigned int shift = exponent < static_cast<int>(target_bits)
                               ? static_cast<unsigned int>(63 - exponent)
                               : 0U;
    uint64_t magnitude = shift < 64 ? significand >> shift : 0;
    bool inexact = false;
    bool round_up = false;

    if (shift != 0) {
        if (shift < 64) {
            const uint64_t half = 1ULL << (shift - 1);
            const uint64_t remainder = significand & ((half << 1) - 1);
            inexact = remainder != 0;
            if (round_mode == FPUControlWord::Nearest) {
                round_up = remainder > half ||
                           (remainder == half && (magnitude & 1));
            }
        } else {
            inexact = true;
            if (round_mode == FPUControlWord::Nearest && shift == 64)
                round_up = significand > extended_integer_bit;
        }
    }

    if (inexact) {
        if (round_mode == FPUControlWord::Down)
            round_up = sign;
        else if (round_mode == FPUControlWord::Up)
            round_up = !sign;
        magnitude += static_cast<uint64_t>(round_up);
    }

    const uint64_t target_limit = 1ULL << target_bits;
    if (magnitude > target_limit || (magnitude == target_limit && !sign)) {
        return invalidInteger(target_min);
    }

    conversion.rounded_up = round_up;
    if (inexact)
        conversion.exceptions = FPU_EX_PRECISION;

    if (sign) {
        conversion.value = magnitude == target_limit
                                   ? target_min
                                   : -static_cast<int64_t>(magnitude);
    } else {
        conversion.value = static_cast<int64_t>(magnitude);
    }
    return conversion;
}

} // namespace

IntegerConversionResult convertToI16(const FPU_Reg_80& val)
{
    return convertToInteger(val, 15, fpu.cw.roundMode());
}

IntegerConversionResult convertToI16Truncate(const FPU_Reg_80& val)
{
    return convertToInteger(val, 15, FPUControlWord::Chop);
}

IntegerConversionResult convertToI32(const FPU_Reg_80& val)
{
    return convertToInteger(val, 31, fpu.cw.roundMode());
}

IntegerConversionResult convertToI32Truncate(const FPU_Reg_80& val)
{
    return convertToInteger(val, 31, FPUControlWord::Chop);
}

IntegerConversionResult convertToI64(const FPU_Reg_80& val)
{
    return convertToInteger(val, 63, fpu.cw.roundMode());
}

IntegerConversionResult convertToI64Truncate(const FPU_Reg_80& val)
{
    return convertToInteger(val, 63, FPUControlWord::Chop);
}

double convertToDouble(const FPU_Reg_80& val)
{
    return convertToF64(val).value.v;
}

void convertFrom(FPU_Reg_80& result, int64_t value)
{
    result.f.mantissa = 0;
    result.f.exponent = 0;
    result.f.sign = 0;

    if (value == 0)
        return;

    const bool sign = value < 0;
    uint64_t magnitude = static_cast<uint64_t>(value);
    if (sign)
        magnitude = 0ULL - magnitude;

    unsigned int shift = 0;
    while (!(magnitude & 0x8000000000000000ULL)) {
        magnitude <<= 1;
        ++shift;
    }

    result.f.mantissa = magnitude;
    result.f.exponent = static_cast<uint16_t>(ExpBias + 63U - shift);
    result.f.sign = sign;
}

void convertFrom(FPU_Reg_80& result, const FPU_Reg_32& value)
{
    convertFromIEEE(result,
                    value.f.mantissa,
                    static_cast<uint16_t>(value.f.exponent),
                    static_cast<bool>(value.f.sign),
                    23,
                    127,
                    0xFFU);
}

void convertFrom(FPU_Reg_80& result, const FPU_Reg_64& value)
{
    convertFromIEEE(result,
                    value.f.mantissa,
                    static_cast<uint16_t>(value.f.exponent),
                    static_cast<bool>(value.f.sign),
                    52,
                    1023,
                    0x7FFU);
}

void round(FPU_Reg_80& val, uint8_t extra_two_bits)
{
    const FPUControlWord::RoundMode round_mode = FPU_ArchitectureType >= FPU_ARCHTYPE_387
                                    ? fpu.cw.roundMode()
                                    : FPUControlWord::Nearest;
    const uint64_t remainder = extra_two_bits & 0x3u;
    bool round_up = false;

    switch (round_mode) {
    case FPUControlWord::Nearest:
        round_up = remainder > 2 || (remainder == 2 && (val.f.mantissa & 1));
        break;
    case FPUControlWord::Down:
        round_up = remainder != 0 && val.f.sign;
        break;
    case FPUControlWord::Up:
        round_up = remainder != 0 && !val.f.sign;
        break;
    case FPUControlWord::Chop:
        break;
    }

    if (round_up && ++val.f.mantissa == 0) {
        val.f.mantissa = 0x8000000000000000ULL;
        ++val.f.exponent;
    }
}

} // namespace float80

#endif
