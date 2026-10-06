#include "tick_backlog.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    unsigned debt = 0, wall = 0, guest = 0, i;
    const unsigned jitter[] = {1, 37, 1, 0, 70, 1, 0, 0, 1, 0, 0};
    /* An overslept frame is caught up on later batches, including same-tick
     * polls; the old unconditional 20 ms cap loses this elapsed time. */
    assert(OSTickSchedule(&debt, 37) == 20 && debt == 17);
    assert(OSTickSchedule(&debt, 0) == 17 && !debt);
    for (i = 0; i < 2000; i++) {
        unsigned elapsed = jitter[i % (sizeof(jitter) / sizeof(jitter[0]))];
        unsigned scheduled = OSTickSchedule(&debt, elapsed);
        wall += elapsed;
        guest += scheduled;
        assert(scheduled <= 20 && guest + debt == wall);
    }
    while (debt) guest += OSTickSchedule(&debt, 0);
    assert(guest == wall);
    assert(OSTickSchedule(&debt, 2000) == 20 && !debt);
    assert(OSTickSchedule(&debt, UINT_MAX) == 20 && !debt);
    debt = 240;
    assert(OSTickSchedule(&debt, 20) == 20 && !debt);
    printf("OPENSTEP clock: %u jittered milliseconds preserved; long stalls bounded\n", wall);
    return 0;
}
