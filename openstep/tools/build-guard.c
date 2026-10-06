/* Bound one build's process group; OPENSTEP's Mach allocator bypasses
 * RLIMIT_DATA. Monitor the group as well as the filesystem's free space. */
#include <libc.h>
#include <sys/resource.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MEMORY_LIMIT (256.0 * 1024 * 1024)
#define DISK_RESERVE (256.0 * 1024 * 1024)
static volatile sig_atomic_t interrupted;

static void interrupt_build(int sig) { interrupted = sig; }

static double disk_free(void)
{
    struct statfs info;
    if (statfs(".", &info) < 0) return -1;
    return (double)info.f_bavail * info.f_bsize;
}

static double memory_size(const char *text)
{
    char *end;
    double value = strtod(text, &end);
    if (*end == 'G') return value * 1024 * 1024 * 1024;
    if (*end == 'M') return value * 1024 * 1024;
    return value * 1024; /* ps uses K, or unsuffixed kilobytes */
}

static double group_memory(int group)
{
    FILE *listing = popen("ps -auxww", "r");
    char line[2048], user[32], size[32];
    int pid;
    double total = 0;
    if (!listing) return -1;
    while (fgets(line, sizeof(line), listing)) {
        if (sscanf(line, "%31s %d %*s %*s %31s", user, &pid, size) == 3 &&
            pid > 0 && getpgrp(pid) == group)
            total += memory_size(size);
    }
    if (pclose(listing) != 0) return -1;
    return total;
}

static void delay(void)
{
    struct timeval timeout;
    timeout.tv_sec = 1; timeout.tv_usec = 0;
    select(0, NULL, NULL, NULL, &timeout);
}

int main(int argc, char **argv)
{
    int child, waited, failed = 0;
    union wait status;
    double peak = 0;
    status.w_status = 0;
    if (argc < 2) return 2;
    if (disk_free() < DISK_RESERVE) {
        fprintf(stderr, "Build refused: cannot reserve 256 MiB of free disk space.\n");
        return 1;
    }
    signal(SIGINT, interrupt_build); signal(SIGTERM, interrupt_build);
    child = fork();
    if (child < 0) { perror("build-guard: fork"); return 1; }
    if (!child) {
        struct rlimit limit;
        signal(SIGINT, SIG_DFL); signal(SIGTERM, SIG_DFL);
        /* BSD setpgrp does not translate group zero to our PID. */
        if (setpgrp(0, getpid()) < 0) _exit(126);
        limit.rlim_cur = limit.rlim_max = 8 * 1024 * 1024;
        if (setrlimit(RLIMIT_STACK, &limit) < 0) _exit(126);
        limit.rlim_cur = limit.rlim_max = 256 * 1024 * 1024;
        if (setrlimit(RLIMIT_DATA, &limit) < 0) _exit(126);
        limit.rlim_cur = limit.rlim_max = 0;
        setrlimit(RLIMIT_CORE, &limit);
        execvp(argv[1], (const char **)(argv + 1));
        perror("build-guard: exec"); _exit(127);
    }
    /* There is only one live direct child between completed ps/pclose calls.
     * Native OPENSTEP libc supplies BSD wait3, not POSIX waitpid. */
    while ((waited = wait3(&status, WNOHANG, NULL)) == 0 ||
           (waited < 0 && errno == EINTR)) {
        double memory = group_memory(child);
        if (memory > peak) peak = memory;
        if (interrupted || memory < 0 || memory > MEMORY_LIMIT || disk_free() < DISK_RESERVE) {
            int i;
            fprintf(stderr, "Build stopped: %s (group memory %.1f MiB).\n",
                interrupted ? "interrupted" : memory > MEMORY_LIMIT ? "memory limit" :
                memory < 0 ? "cannot monitor processes" : "disk reserve", memory / 1048576);
            kill(-child, SIGTERM);
            for (i = 0; i < 5 && kill(-child, 0) == 0; ++i) delay();
            kill(-child, SIGKILL);
            wait3(&status, 0, NULL);
            failed = 1;
            break;
        }
        delay();
    }
    fprintf(stderr, "Build peak process-group memory: %.1f MiB\n", peak / 1048576);
    return !failed && waited >= 0 && WIFEXITED(status) ? status.w_retcode : 1;
}
