/* Preserve short host scheduling delays without unbounded catch-up work. */
#ifndef DOSBOX_OPENSTEP_TICK_BACKLOG_H
#define DOSBOX_OPENSTEP_TICK_BACKLOG_H
static unsigned OSTickSchedule(unsigned *backlog, unsigned elapsed)
{
    unsigned pending;
    /* Long pauses and sustained overload still rebase the guest clock. */
    if (elapsed > 250 || *backlog > 250 - elapsed) {
        *backlog = 0;
        return elapsed > 20 ? 20 : elapsed;
    }
    pending = *backlog + elapsed;
    if (pending > 20) {
        *backlog = pending - 20;
        return 20;
    }
    *backlog = 0;
    return pending;
}
#endif
