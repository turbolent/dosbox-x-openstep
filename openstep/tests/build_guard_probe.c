/* Reserve address space without touching pages. The guard must stop this
 * process before it prints the failure marker; no swap allocation is needed. */
#include <libc.h>
#include <mach/mach.h>
int main(void)
{
    vm_address_t address = 0;
    if (vm_allocate(task_self(), &address, 300 * 1024 * 1024, TRUE)) return 2;
    printf("Reserved 300 MiB without touching pages (pid=%d group=%d)\n",
           getpid(), getpgrp(0));
    fflush(stdout);
    sleep(10);
    fprintf(stderr, "FAIL: build guard did not stop oversized process\n");
    return 3;
}
