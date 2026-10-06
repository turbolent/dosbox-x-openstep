#include "host.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    const OSByte function[] = {0xb8, 42, 0, 0, 0, 0xc3}; /* mov eax,42; ret */
    OSByte *code;
    int (*execute)(void);
    unsigned iteration;
    assert(OSHostExecutableAlloc(0) == NULL);
    assert(OSHostExecutableAlloc(~0U) == NULL);
    for (iteration = 0; iteration < 32; ++iteration) {
        code = (OSByte *)OSHostExecutableAlloc(16384);
        assert(code && !((unsigned long)code & 4095));
        memcpy(code, function, sizeof(function));
        execute = (int (*)(void))code;
        assert(execute() == 42);
        code[1] = 43;
        assert(execute() == 43);
        OSHostExecutableFree(code, 16384);
    }
    puts("OPENSTEP executable cache tests passed");
    return 0;
}
