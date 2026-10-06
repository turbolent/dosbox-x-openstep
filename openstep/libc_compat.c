/* Bounded formatting, including the 64-bit integers used by DOSBox-X. */
#include "libc_compat.h"
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "vendor/stb_sprintf.h"

#if defined(NeXT)
/* OPENSTEP compares signed bytes in these routines. DOS filenames and
 * code-page strings need the unsigned-byte ordering required by C. */
int strcmp(const char *first, const char *second)
{
    const unsigned char *a = (const unsigned char *)first;
    const unsigned char *b = (const unsigned char *)second;
    while (*a && *a == *b) { ++a; ++b; }
    return (int)*a - (int)*b;
}

int strncmp(const char *first, const char *second, size_t count)
{
    const unsigned char *a = (const unsigned char *)first;
    const unsigned char *b = (const unsigned char *)second;
    while (count) {
        int difference = (int)*a - (int)*b;
        if (difference || !*a) return difference;
        ++a; ++b; --count;
    }
    return 0;
}

char *strdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = (char *)malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}
#endif

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    return stbsp_vsnprintf(buffer, size > INT_MAX ? INT_MAX : (int)size, format, args);
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    int length;
    va_list args;
    va_start(args, format);
    length = vsnprintf(buffer, size, format, args);
    va_end(args);
    return length;
}

unsigned long long strtoull(const char *text, char **end, int base)
{
    const char *p = text;
    unsigned long long value = 0, limit;
    unsigned digit, remainder;
    int negative = 0, any = 0, overflow = 0;
    if (end) *end = (char *)text;
    if (base && (base < 2 || base > 36)) { errno = EINVAL; return 0; }
    while (isspace((unsigned char)*p)) ++p;
    if (*p == '-' || *p == '+') negative = *p++ == '-';
    if ((!base || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
        isxdigit((unsigned char)p[2])) { p += 2; base = 16; }
    if (!base) base = *p == '0' ? 8 : 10;
    limit = ~0ULL / (unsigned)base;
    remainder = (unsigned)(~0ULL % (unsigned)base);
    for (;; ++p) {
        unsigned ch = (unsigned char)*p;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'z') digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'Z') digit = ch - 'A' + 10;
        else break;
        if (digit >= (unsigned)base) break;
        any = 1;
        if (value > limit || (value == limit && digit > remainder)) overflow = 1;
        if (!overflow) value = value * (unsigned)base + digit;
    }
    if (any && end) *end = (char *)p;
    if (overflow) { errno = ERANGE; return ~0ULL; }
    return negative ? 0ULL - value : value;
}

#if defined(NeXT)
#include <sys/param.h>
char *getcwd(char *buffer, size_t size)
{
    char path[MAXPATHLEN];
    size_t length;
    if (!buffer || !size) { errno = EINVAL; return NULL; }
    if (!getwd(path)) return NULL;
    length = strlen(path) + 1;
    if (length > size) { errno = ERANGE; return NULL; }
    memcpy(buffer, path, length);
    return buffer;
}
#endif
