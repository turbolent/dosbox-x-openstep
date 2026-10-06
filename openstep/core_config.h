/* Fixed OPENSTEP 4.2 i386 feature profile. Copied to output/config.h by make. */
#ifndef DOSBOX_OPENSTEP_CONFIG_H
#define DOSBOX_OPENSTEP_CONFIG_H
#define C_OPENSTEP 1
#define C_TARGETCPU X86
#define C_DYNAMIC_X86 1
#define C_FPU 1
#define C_FPU_X86 1
#define C_UNALIGNED_MEMORY 1
#define C_LIBZ 1
#define C_ICONV 1
#define C_HAS_ATTRIBUTE 1
#define C_HAS_BUILTIN_EXPECT 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_PWD_H 1
#define ENVIRON_LINKED 1
#define HAS_CDIRECTLPT 0
#define SIZEOF_INT_P 4
#define SIZEOF_UNSIGNED_CHAR 1
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT 4
#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_UNSIGNED_LONG_LONG 8
/* Forced inlining of the interpreter overwhelms the native GCC optimizer. */
#define INLINE inline
#define DB_FASTCALL
#define GCC_ATTRIBUTE(x) __attribute__((x))
#define GCC_UNLIKELY(x) __builtin_expect(!!(x),0)
#define GCC_LIKELY(x) __builtin_expect(!!(x),1)
#define CONST const
#define PACKAGE "dosbox-x"
#define PACKAGE_NAME "DOSBox-X"
#define PACKAGE_VERSION "2026.10.01"
#define VERSION PACKAGE_VERSION
#define PACKAGE_STRING "DOSBox-X " VERSION
#define COPYRIGHT_YEAR "2026"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#if defined(NeXT)
#include <sys/types.h>
#include "openstep/libc_compat.h"
/* Native headers expose this only in their strict POSIX profile. */
typedef unsigned short mode_t;
#ifdef __cplusplus
/* OPENSTEP defines FILE as a macro; libstdc++'s cstdio replaces it with
 * std::FILE, but does not provide the usual global typedef. */
#include <cstdio>
using std::FILE;
#endif
#endif
typedef double Real64;
typedef uint8_t Bit8u;
typedef int8_t Bit8s;
typedef uint16_t Bit16u;
typedef int16_t Bit16s;
typedef uint32_t Bit32u;
typedef int32_t Bit32s;
typedef uint64_t Bit64u;
typedef int64_t Bit64s;
typedef uintptr_t Bitu;
typedef intptr_t Bits;
#endif
