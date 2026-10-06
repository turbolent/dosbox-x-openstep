/* Exercise the protected-mode descriptor dispatch with the target optimizer. */
#include "dosbox.h"
#include "cpu.h"
#include <stdio.h>

static int __attribute__((noinline)) jump_class(const Descriptor &descriptor)
{
    switch (descriptor.Type()) {
    case DESC_286_CALL_GATE: case DESC_TASK_GATE: case DESC_386_CALL_GATE: return 1;
    case DESC_386_TSS_A: return 2;
    case DESC_CODE_N_NC_A: case DESC_CODE_N_NC_NA:
    case DESC_CODE_R_NC_A: case DESC_CODE_R_NC_NA: return 3;
    case DESC_CODE_N_C_A: case DESC_CODE_N_C_NA:
    case DESC_CODE_R_C_A: case DESC_CODE_R_C_NA: return 4;
    default: return 0;
    }
}

int main()
{
    int failures = 0;
    for (unsigned type = 0; type < 32; ++type) {
        Descriptor descriptor;
        descriptor.saved.fill[1] = 0x00cf8000U | (type << 8);
        const int expected = type >= 28 ? 4 : type >= 24 ? 3 :
                             type == 9 ? 2 : type == 4 || type == 5 || type == 12 ? 1 : 0;
        const int actual = jump_class(descriptor);
        if (descriptor.Type() != type || actual != expected) {
            printf("descriptor=%02x read=%02x dispatch=%d expected=%d\n",
                   type, (unsigned)descriptor.Type(), actual, expected);
            ++failures;
        }
    }
    if (!failures) puts("OPENSTEP descriptor dispatch passed (all 32 types)");
    return failures ? 1 : 0;
}
