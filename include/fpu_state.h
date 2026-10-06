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

#ifndef DOSBOX_FPU_STATE_H
#define DOSBOX_FPU_STATE_H

#include <algorithm>
#include <cstdint>
#include <string>

#include "cpu.h"
#include "fpu_types.h"

template<class T, unsigned bitno, unsigned nbits=1>
struct RegBit
{
	enum { basemask = (1 << nbits) - 1 };
	enum { mask = basemask << bitno };
	T data;
	template <class T2> RegBit& operator=(T2 val)
	{
		data = (data & ~mask) | ((nbits > 1 ? val & basemask : !!val) << bitno);
		return *this;
	}
	operator unsigned() const { return (data & mask) >> bitno; }
};

struct FPUControlWord
{
	union
	{
		uint16_t reg;
		RegBit<uint16_t, 0>     IM;  // Invalid operation mask
		RegBit<uint16_t, 1>     DM;  // Denormalized operand mask
		RegBit<uint16_t, 2>     ZM;  // Zero divide mask
		RegBit<uint16_t, 3>     OM;  // Overflow mask
		RegBit<uint16_t, 4>     UM;  // Underflow mask
		RegBit<uint16_t, 5>     PM;  // Precision mask
		RegBit<uint16_t, 7>     M;   // Interrupt mask   (8087-only)
		RegBit<uint16_t, 8, 2>  PC;  // Precision control
		RegBit<uint16_t, 10, 2> RC;  // Rounding control
		RegBit<uint16_t, 12>    IC;  // Infinity control (8087/80287-only)
	};

	enum
	{
		mask8087     = 0x1fff,
		maskNon8087  = 0x1f7f,
		reservedMask = 0x40,
		initValue    = 0x37f
	};
	enum RoundMode
	{
		Nearest = 0,
		Down    = 1,
		Up      = 2,
		Chop    = 3
	};

	FPUControlWord() : reg(initValue) {}
	FPUControlWord(const FPUControlWord& other) : reg(other.reg) {}
	FPUControlWord& operator=(const FPUControlWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUControlWord& operator=(T val)
	{
		reg = (val & (FPU_ArchitectureType<=FPU_ARCHTYPE_8087 ? mask8087 : maskNon8087)) | reservedMask;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUControlWord& operator |=(T val)
	{
		*this = reg | val;
		return *this;
	}
	void init() { reg = initValue; }
	FPUControlWord allMasked() const
	{
		FPUControlWord masked = *this;
		masked |= IM.mask | DM.mask | ZM.mask | OM.mask | UM.mask | PM.mask;
		return masked;
	}
	RoundMode roundMode() const
	{
		return static_cast<RoundMode>(static_cast<unsigned>(RC));
	}
};

struct FPUStatusWord
{
	union
	{
		uint16_t reg;
		RegBit<uint16_t, 0>     IE;  // Invalid operation
		RegBit<uint16_t, 1>     DE;  // Denormalized operand
		RegBit<uint16_t, 2>     ZE;  // Divide-by-zero
		RegBit<uint16_t, 3>     OE;  // Overflow
		RegBit<uint16_t, 4>     UE;  // Underflow
		RegBit<uint16_t, 5>     PE;  // Precision
		RegBit<uint16_t, 6>     SF;  // Stack Flag (non-8087/802087)
		RegBit<uint16_t, 7>     IR;  // Interrupt request (8087-only)
		RegBit<uint16_t, 7>     ES;  // Error summary     (non-8087)
		RegBit<uint16_t, 8>     C0;  // Condition flag
		RegBit<uint16_t, 9>     C1;  // Condition flag
		RegBit<uint16_t, 10>    C2;  // Condition flag
		RegBit<uint16_t, 11, 3> top; // Top of stack pointer
		RegBit<uint16_t, 14>    C3;  // Condition flag
		RegBit<uint16_t, 15>    B;   // Busy flag
	};

	FPUStatusWord() : reg(0) {}
	FPUStatusWord(const FPUStatusWord& other) : reg(other.reg) {}
	FPUStatusWord& operator=(const FPUStatusWord& other)
	{
		reg = other.reg;
		return *this;
	}
	template<class T>
	FPUStatusWord& operator=(T val)
	{
		reg = val;
		return *this;
	}
	operator unsigned() const
	{
		return reg;
	}
	template <class T>
	FPUStatusWord& operator |=(T val)
	{
		*this = reg | val;
		return *this;
	}
	void init() { reg = 0; }
	void clearExceptions()
	{
		IE = false; DE = false; ZE = false; OE = false; UE = false; PE = false;
		ES = false;
        B = false;
	}
	enum
	{
		conditionMask = 0x4700,
		conditionAndExceptionMask = 0x47bf,
        exceptionMask = 0xff
	};
	std::string to_string() const;
};


struct FPU
{
    FPU() : ea_denormal(false) {}
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	FPU_Reg_64          _do_not_use__regs[9];
#else
	FPU_Reg_64          regs[9];
#endif
	union {/*these two have the same format, so alias them as an anon union to make switching between dynamic and normal core easier!*/
		FPU_P_Reg       p_regs[9];
		FPU_Reg_80      regs_80[9];
	};
#if defined(HAS_LONG_DOUBLE)//probably shouldn't allow struct to change size based on this
	bool                _do_not_use__use80[9];		// if set, use the 80-bit precision version
#else
	bool                use80[9];		// if set, use the 80-bit precision version
#endif
    bool regvalid[9];
	bool                ea_denormal;
	XMM_Reg             xmmreg[8]; // SSE emulation

	uint32_t            mxcsr; // SSE control register
	FPUControlWord      cw;
	FPUStatusWord       sw;
};

extern FPU fpu;

static INLINE Bitu FPU_StackIndex(Bitu index)
{
	return (fpu.sw.top + index) & 7;
}

#endif // DOSBOX_FPU_STATE_H
