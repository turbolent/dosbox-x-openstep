/* Run a command in an existing desktop session's Mach service namespace.
 * Uses the caller's existing task-port permissions; does not elevate uid or
 * change WindowServer preferences. Desktop PID is explicit, never guessed.
 * cc -o generic-exec generic-exec.c
 * ./generic-exec <desktop-process-pid> <command> [arguments...]
 */
#include <libc.h>
#include <errno.h>
#include <limits.h>
#include <mach/mach.h>
#include <mach/task_special_ports.h>

int main(int argc, char **argv)
{
    port_t desktop_task, desktop_bootstrap;
    kern_return_t result;
    long pid;
    char *end;

    if (argc < 3) {
        fprintf(stderr, "usage: generic-exec desktop-pid command [args...]\n");
        return 2;
    }
    errno = 0;
    pid = strtol(argv[1], &end, 10);
    if (errno || end == argv[1] || *end || pid <= 0 || pid > INT_MAX) {
        fprintf(stderr, "generic-exec: invalid desktop PID: %s\n", argv[1]);
        return 2;
    }
    result = task_by_unix_pid(task_self(), (int)pid, &desktop_task);
    if (!result)
        result = task_get_bootstrap_port(desktop_task, &desktop_bootstrap);
    if (!result)
        result = task_set_bootstrap_port(task_self(), desktop_bootstrap);
    if (result) {
        fprintf(stderr, "generic-exec: cannot join desktop namespace: %d\n", result);
        return 1;
    }
    execvp(argv[2], argv + 2);
    perror("generic-exec: execvp");
    return 1;
}
