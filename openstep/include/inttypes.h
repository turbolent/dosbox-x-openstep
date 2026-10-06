#ifndef DOSBOX_OPENSTEP_INTTYPES_H
#define DOSBOX_OPENSTEP_INTTYPES_H
#if !defined(NeXT)
#include_next <inttypes.h>
#else
#include <stdint.h>
#define PRId8 "d"
#define PRIi8 "i"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"
#define PRIo8 "o"
#define PRId16 "d"
#define PRIi16 "i"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"
#define PRIo16 "o"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRIo32 "o"
#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRIo64 "llo"
#define PRIdPTR PRId32
#define PRIuPTR PRIu32
#define PRIxPTR PRIx32
#define PRIdMAX PRId64
#define PRIuMAX PRIu64
#define PRIxMAX PRIx64
#endif
#endif
