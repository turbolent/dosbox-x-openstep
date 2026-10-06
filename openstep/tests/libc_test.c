#include "libc_compat.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
    char text[128], small[4] = {'X', 'X', 'X', 'X'}, path[4096];
    int result;
    char *end;
    const char *invalid = "  +x";
    char low[] = {'b', 0}, high[] = {(char)0xa9, 0};
    int (*volatile compare)(const char *, const char *) = strcmp;
    int (*volatile compare_n)(const char *, const char *, size_t) = strncmp;
    assert(compare(low, high) < 0 && compare(high, low) > 0);
    assert(compare(high, high) == 0 && compare("", high) < 0);
    assert(compare_n(low, high, 0) == 0);
    assert(compare_n(low, high, 1) < 0 && compare_n(high, low, 2) > 0);
    assert(compare_n("a", "aa", 1) == 0 && compare_n("a", "aa", 2) < 0);
    errno = 0;
    assert(strtoull("18446744073709551615!", &end, 10) == ~0ULL && *end == '!' && !errno);
    assert(strtoull("  -0x10?", &end, 0) == ~15ULL && *end == '?');
    assert(strtoull("0779", &end, 0) == 63 && *end == '9');
    assert(strtoull("0xz", &end, 0) == 0 && *end == 'x');
    assert(strtoull("z", &end, 36) == 35 && !*end);
    assert(strtoull(invalid, &end, 10) == 0 && end == invalid);
    assert(strtoull("18446744073709551616000!", &end, 10) == ~0ULL && errno == ERANGE && *end == '!');
    assert(snprintf(NULL, 0, "%s-%d", "hello", 42) == 8);
    assert(snprintf(small, 0, "hello") == 5 && small[0] == 'X');
    assert(snprintf(small, 1, "hello") == 5 && small[0] == 0 && small[1] == 'X');
    assert(snprintf(small, sizeof(small), "hello") == 5 && !strcmp(small, "hel"));
    result = snprintf(text, sizeof(text), "%llu %lld %016llx",
                      ~0ULL, (-9223372036854775807LL - 1), 0xc80d2f7eff541609ULL);
    assert(result == 58);
    assert(!strcmp(text, "18446744073709551615 -9223372036854775808 c80d2f7eff541609"));
    assert(snprintf(text, sizeof(text), "%*.*f %04d", 7, 2, 3.125, 7) == 12);
    assert(!strcmp(text, "   3.13 0007"));
    assert(getcwd(path, sizeof(path)) == path && path[0] == '/');
    errno = 0;
    assert(getcwd(small, 1) == NULL && errno == ERANGE);
    puts("OPENSTEP libc compatibility tests passed");
    return 0;
}
