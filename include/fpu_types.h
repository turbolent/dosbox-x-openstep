/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_FPU_TYPES_H
#define DOSBOX_FPU_TYPES_H

#include "compiler_compat.h"
#include <cstddef>
#include <cstdint>

namespace FPUTag { enum Type
{
	Valid   = 0,
	Zero    = 1,
	Special = 2,
	Empty   = 3
}; }


#if !defined(C_OPENSTEP)
#pragma pack(push,1)
#endif
union DBX_ALIGNAS(8) FPU_Reg_64 {
	struct {
		uint64_t	mantissa:52;		// [51:0]
		uint64_t	exponent:11;		// [62:52]
		uint64_t	sign:1;			// [63:63]
	} f;
	double			v;
	uint64_t		raw;

};
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_64*)0)->f) == 8, "FPU_Reg_64 error" );
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_64*)0)->v) == 8, "FPU_Reg_64 error" );
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_64*)0)->raw) == 8, "FPU_Reg_64 error" );
DBX_STATIC_ASSERT( sizeof(FPU_Reg_64) == 8, "FPU_Reg_64 error" );

#if !defined(C_OPENSTEP)
#pragma pack(pop)
#endif

extern const FPU_Reg_64 FPU_QNaN_64;

#define FPU_Reg_64_exponent_bias	(1023)


#if !defined(C_OPENSTEP)
#pragma pack(push,1)
#endif
union DBX_ALIGNAS(4) FPU_Reg_32 {
	struct {
		uint32_t	mantissa:23;		// [22:0]
		uint32_t	exponent:8;		// [30:23]
		uint32_t	sign:1;			// [31:31]
	} f;
	float			v;
	uint32_t		raw;

};
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_32*)0)->f) == 4, "FPU_Reg_32 error" );
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_32*)0)->v) == 4, "FPU_Reg_32 error" );
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_32*)0)->raw) == 4, "FPU_Reg_32 error" );
DBX_STATIC_ASSERT( sizeof(FPU_Reg_32) == 4, "FPU_Reg_32 error" );

#if !defined(C_OPENSTEP)
#pragma pack(pop)
#endif

#define FPU_Reg_32_exponent_bias	(127)


#if !defined(C_OPENSTEP)
#pragma pack(push,1)
#endif
union DBX_ALIGNAS(16) MMX_reg {

	uint64_t q;

#ifndef WORDS_BIGENDIAN
	struct DBX_PACKED fpu_t {
		uint64_t m;
		uint16_t e;
	} fpu;

	struct {
		uint32_t d0,d1;
	} ud;

	struct {
		int32_t d0,d1;
	} sd;

	struct uw_t {
		uint16_t w0,w1,w2,w3;
	} uw;

	uint8_t uba[8];
	uint16_t uwa[4];
	uint32_t uda[2];

	struct {
		int16_t w0,w1,w2,w3;
	} sw;

	struct {
		uint8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} ub;

	struct {
		int8_t b0,b1,b2,b3,b4,b5,b6,b7;
	} sb;

	struct DBX_ALIGNAS(4) { /* MMX registers can contain single precision float if the program uses AMD 3DNow! instructions */
		FPU_Reg_32 f0,f1;
	} f32;
#else
	struct DBX_PACKED fpu_t {
		uint64_t m;
		uint16_t e;
	} fpu;

	struct {
		uint32_t d1,d0;
	} ud;

	struct {
		int32_t d1,d0;
	} sd;

	struct {
		uint16_t w3,w2,w1,w0;
	} uw;

	uint8_t uba[8];
	uint16_t uwa[4];
	uint32_t uda[2];

	struct {
		uint16_t w3,w2,w1,w0;
	} sw;

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} ub;

	struct {
		uint8_t b7,b6,b5,b4,b3,b2,b1,b0;
	} sb;

	struct { /* MMX registers can contain single precision float if the program uses AMD 3DNow! instructions */
		FPU_Reg_32 f1,f0;
	} f32;
#endif
};
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->fpu) == 10, "MMX packing error");
DBX_STATIC_ASSERT(offsetof(MMX_reg::fpu_t,m) == 0, "MMX packing error");
DBX_STATIC_ASSERT(offsetof(MMX_reg::fpu_t,e) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->ud) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->sd) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->uw) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->uba) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->uwa) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->uda) == 8, "MMX packing error");
#ifndef WORDS_BIGENDIAN
DBX_STATIC_ASSERT(offsetof(MMX_reg::uw_t,w0) == 0, "MMX packing error");
#endif
#ifndef WORDS_BIGENDIAN
DBX_STATIC_ASSERT(offsetof(MMX_reg::uw_t,w1) == 2, "MMX packing error");
#endif
#ifndef WORDS_BIGENDIAN
DBX_STATIC_ASSERT(offsetof(MMX_reg::uw_t,w2) == 4, "MMX packing error");
#endif
#ifndef WORDS_BIGENDIAN
DBX_STATIC_ASSERT(offsetof(MMX_reg::uw_t,w3) == 6, "MMX packing error");
#endif
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->sw) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->ub) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->sb) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(((MMX_reg*)0)->f32) == 8, "MMX packing error");
DBX_STATIC_ASSERT(sizeof(MMX_reg) == 16, "MMX packing error");

#if !defined(C_OPENSTEP)
#pragma pack(pop)
#endif


#if !defined(C_OPENSTEP)
#pragma pack(push,1)
#endif
union DBX_ALIGNAS(16) XMM_Reg {
	FPU_Reg_32		f32[4];
	FPU_Reg_64		f64[2];

	int8_t			i8[16];
	int16_t			i16[8];
	int32_t			i32[4];
	int64_t			i64[2];

	uint8_t			u8[16];
	uint16_t		u16[8];
	uint32_t		u32[4];
	uint64_t		u64[2];

};
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->u8)  == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->u16) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->u32) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->u64) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->i8)  == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->i16) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->i32) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->i64) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->f32) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(((XMM_Reg*)0)->f64) == 16 /* 128-bit */, "XMM reg struct error" );
DBX_STATIC_ASSERT( sizeof(XMM_Reg)     == 16 /* 128-bit */, "XMM reg struct error" );

#if !defined(C_OPENSTEP)
#pragma pack(pop)
#endif

extern MMX_reg * reg_mmx[8];
extern MMX_reg * lookupRMregMM[256];


#if !defined(C_OPENSTEP)
#pragma pack(push,1)
#endif
union DBX_ALIGNAS(16) FPU_Reg_80
{
// TODO: The configure script needs to use "long double" on x86/x86_64 and verify sizeof(long double) == 10,
//       else undef a macro to let the code emulate long double 80-bit IEEE. Also needs to determine host
//       byte order here so host long double matches our struct.
	struct DBX_PACKED f_t {
		uint64_t	mantissa;		// [63:0]
		uint16_t	exponent:15;	// [78:64]
		uint16_t	sign:1;			// [79:79]
	} f;
#if defined(HAS_LONG_DOUBLE)
	long double		v;			// [79:0]
#endif
	struct DBX_PACKED raw_t {
		uint64_t	l;
		uint16_t	h;
	} raw;

	MMX_reg reg_mmx;

};
DBX_STATIC_ASSERT( sizeof(((FPU_Reg_80*)0)->reg_mmx) == 16, "FPU_Reg_80 error" );
DBX_STATIC_ASSERT( offsetof(FPU_Reg_80::f_t,mantissa) == 0, "oops" );
DBX_STATIC_ASSERT( offsetof(FPU_Reg_80::raw_t,l) == 0, "oops" );
DBX_STATIC_ASSERT( offsetof(FPU_Reg_80::raw_t,h) == 8, "oops" );
DBX_STATIC_ASSERT( offsetof(MMX_reg,q) == 0, "oops" );
DBX_STATIC_ASSERT( sizeof(FPU_Reg_80) == 16, "FPU_Reg_80 error" );/*NTS: GCC can and often will define long double as 16 bytes or at least align by 16 bytes*/
// ^ Remember that in 80-bit extended, the mantissa contains both the fraction and integer bit. There is no
//   "implied bit" like 32-bit and 64-bit formats.

#if !defined(C_OPENSTEP)
#pragma pack(pop)
#endif

extern const FPU_Reg_80 FPU_QNaN_80;

#define FPU_Reg_80_exponent_bias	(16383)

static inline bool IsZero(const FPU_Reg_32& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa == 0;
}

static inline bool IsZero(const FPU_Reg_64& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa == 0;
}

static inline bool IsZero(const FPU_Reg_80& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa == 0;
}

static inline bool IsSubnormal(const FPU_Reg_32& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa != 0;
}

static inline bool IsSubnormal(const FPU_Reg_64& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa != 0;
}

static inline bool IsSubnormal(const FPU_Reg_80& reg)
{
    return reg.f.exponent == 0 && reg.f.mantissa != 0;
}

static inline bool IsInfinity(const FPU_Reg_32& reg)
{
    return reg.f.exponent == 0xFFU && reg.f.mantissa == 0;
}

static inline bool IsInfinity(const FPU_Reg_64& reg)
{
    return reg.f.exponent == 0x7FFU && reg.f.mantissa == 0;
}

static inline bool IsInfinity(const FPU_Reg_80& reg)
{
    const uint64_t integer_bit = 0x8000000000000000ULL;
    return reg.f.exponent == 0x7FFFU && reg.f.mantissa == integer_bit;
}

static inline bool IsNaN(const FPU_Reg_32& reg)
{
    return reg.f.exponent == 0xFFU && reg.f.mantissa != 0;
}

static inline bool IsNaN(const FPU_Reg_64& reg)
{
    return reg.f.exponent == 0x7FFU && reg.f.mantissa != 0;
}

static inline bool IsNaN(const FPU_Reg_80& reg)
{
    const uint64_t integer_bit = 0x8000000000000000ULL;
    return reg.f.exponent == 0x7FFFU && reg.f.mantissa != integer_bit;
}

// Retain the x87 exception-oriented name for existing callers.
static inline bool IsDenormal(const FPU_Reg_32& reg)
{
    return IsSubnormal(reg);
}

static inline bool IsDenormal(const FPU_Reg_64& reg)
{
    return IsSubnormal(reg);
}

static inline bool IsDenormal(const FPU_Reg_80& reg)
{
    return IsSubnormal(reg);
}

static inline bool IsNormal(const FPU_Reg_32& reg)
{
    return reg.f.exponent != 0 && reg.f.exponent != 0xFFU;
}

static inline bool IsNormal(const FPU_Reg_64& reg)
{
    return reg.f.exponent != 0 && reg.f.exponent != 0x7FFU;
}

static inline bool IsNormal(const FPU_Reg_80& reg)
{
    const uint64_t integer_bit = 0x8000000000000000ULL;
    return reg.f.exponent != 0 && reg.f.exponent != 0x7FFFU &&
           (reg.f.mantissa & integer_bit) != 0;
}

static inline bool IsSNaN(const FPU_Reg_32& reg)
{
    const uint32_t quiet_nan_bit = 0x00400000U;
    return reg.f.exponent == 0xFFU && reg.f.mantissa != 0 &&
           (reg.f.mantissa & quiet_nan_bit) == 0;
}

static inline bool IsSNaN(const FPU_Reg_64& reg)
{
    const uint64_t quiet_nan_bit = 0x0008000000000000ULL;
    return reg.f.exponent == 0x7FFU && reg.f.mantissa != 0 &&
           (reg.f.mantissa & quiet_nan_bit) == 0;
}

static inline bool IsSNaN(const FPU_Reg_80& reg)
{
    const uint64_t integer_bit = 0x8000000000000000ULL;
    const uint64_t quiet_nan_bit = 0x4000000000000000ULL;
    const uint64_t payload_mask = 0x3FFFFFFFFFFFFFFFULL;
    return reg.f.exponent == 0x7FFFU &&
           (reg.f.mantissa & (integer_bit | quiet_nan_bit)) == integer_bit &&
           (reg.f.mantissa & payload_mask) != 0;
}

static inline bool IsSpecial(const FPU_Reg_32& reg)
{
    return !IsNormal(reg) && !IsZero(reg);
}

static inline bool IsSpecial(const FPU_Reg_64& reg)
{
    return !IsNormal(reg) && !IsZero(reg);
}

static inline bool IsSpecial(const FPU_Reg_80& reg)
{
    return !IsNormal(reg) && !IsZero(reg);
}

// dynamic x86 core needs this
struct FPU_P_Reg {
    // 80-bit extended float (m2:m1 = 64-bit mantissa  m3 = sign:exponent)
    uint32_t m1;
    uint32_t m2;
    uint16_t m3;
    // Padding to make the structure 16 bytes so the inline asm in fpu_instructions_x86.h can shift by 4 to index FPU registers
    uint16_t d1;
    uint32_t d2;
};
DBX_STATIC_ASSERT( sizeof(FPU_P_Reg) == 16, "FPU_P_Reg error" );

// memory barrier macro. to ensure that reads/stores to one half of the FPU reg struct
// do not overlap with reads/stores from the other half. things can go wrong if the
// compiler writes code to write the mantissa, then load the overall as float, then store
// the exponent. note this is not a hardware level memory barrier, this is a compiler
// level memory barrier against the optimization engine.
#if defined(__GCC__)
# define FPU_Reg_m_barrier()	__asm__ __volatile__ ("":::"memory")
#else
# define FPU_Reg_m_barrier()
#endif

#endif // DOSBOX_FPU_TYPES_H
