/* Standalone compiler diagnostic, intentionally fails on Apple GCC 4.2.1
 * build 5666.3. Compile with -fwrapv and -O0, -O1 or -O2, matching
 * the OPENSTEP port's arithmetic flags, to check a toolchain fix.
 * This test is independent of DOSBox headers. */
#include <stdio.h>
struct Descriptor {
    unsigned padding : 8;
    unsigned type : 5;
    unsigned Type() const { return type; }
};
struct Counter { unsigned value : 24; } counter;
static int __attribute__((noinline)) check_wrap() {
    if (--counter.value != 0xffffffU || counter.value != 0xffffffU) return 1;
    if (++counter.value != 0 || counter.value != 0) return 1;
    return 0;
}
static int __attribute__((noinline)) dispatch(const Descriptor &d) {
    switch (d.Type()) {
    case 4: case 5: case 12: return 1;
    case 9: return 2;
    case 24: case 25: case 26: case 27: return 3;
    case 28: case 29: case 30: case 31: return 4;
    default: return 0;
    }
}
int main() {
    int failures = check_wrap();
    for (unsigned i=0; i<32; ++i) {
        Descriptor d = {0, i};
        int expected = i>=28 ? 4 : i>=24 ? 3 : i==9 ? 2 :
                       i==4 || i==5 || i==12 ? 1 : 0;
        int actual=dispatch(d);
        if(actual!=expected) {
            printf("type=%u actual=%d expected=%d\n",i,actual,expected);
            ++failures;
        }
    }
    printf("failures=%d\n",failures);
    return failures ? 1 : 0;
}
