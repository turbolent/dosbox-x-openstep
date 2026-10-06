/* BSD host services for OPENSTEP 4.2. */
#include "host.h"
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef NeXT
#include <libc.h>
#include <mach-o/dyld.h>
#endif
#ifndef EOVERFLOW
#define EOVERFLOW ERANGE
#endif

const char *OSHostExecutablePath(void)
{
#ifdef NeXT
    return _dyld_get_image_name(0);
#else
    return NULL;
#endif
}

OSUInt32 OSHostTicks(void)
{
    static OSUInt64 previous, elapsed;
    static int started;
    struct timeval tv;
    OSUInt64 now;
    if (gettimeofday(&tv, NULL) != 0)
        return (OSUInt32)elapsed;
    now = (OSUInt64)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (!started) { previous = now; started = 1; }
    /* Rebase after clock corrections; never turn backwards time into a huge tick. */
    if (now >= previous) elapsed += now - previous;
    previous = now;
    return (OSUInt32)elapsed;
}

void OSHostSleep(unsigned milliseconds)
{
    OSUInt32 start = OSHostTicks();
    unsigned elapsed;
    struct timeval delay;
    while ((elapsed = (OSUInt32)(OSHostTicks() - start)) < milliseconds) {
        unsigned remaining = milliseconds - elapsed;
        delay.tv_sec = remaining / 1000;
        delay.tv_usec = (remaining % 1000) * 1000;
        if (select(0, NULL, NULL, NULL, &delay) < 0 && errno != EINTR)
            break;
    }
}

long long OSHostTell(FILE *file)
{
    long position;
    if (!file) { errno = EINVAL; return -1; }
    position = ftell(file);
    if (position < 0 || (unsigned long)position > 0x7fffffffUL) {
        errno = EOVERFLOW;
        return -1;
    }
    return position;
}

int OSHostFileSize(FILE *file, long long *size)
{
    long long position = OSHostTell(file), end;
    if (position < 0 || !size) { errno = EINVAL; return -1; }
    if (fseek(file, 0, SEEK_END) != 0) return -1;
    end = OSHostTell(file);
    if (fseek(file, (long)position, SEEK_SET) != 0) return -1;
    if (end < 0) return -1;
    *size = end;
    return 0;
}

int OSHostSeek(FILE *file, long long offset, int origin)
{
    long long base = 0;
    if (!file) { errno = EINVAL; return -1; }
    if (origin == SEEK_CUR) {
        base = OSHostTell(file);
        if (base < 0) return -1;
    } else if (origin == SEEK_END) {
        if (OSHostFileSize(file, &base) != 0) return -1;
    } else if (origin != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }
    /* Check before adding, so even LLONG_MIN/LLONG_MAX cannot wrap. */
    if (offset < -base || offset > 0x7fffffffLL - base) {
        errno = EOVERFLOW;
        return -1;
    }
    return fseek(file, (long)(base + offset), SEEK_SET);
}

FILE *OSHostCreateFile(const char *path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    FILE *file;
    if (fd < 0) return NULL;
    /* Native fdopen parses '+' immediately after the access letter. */
    file = fdopen(fd, "w+");
    if (!file) { int error = errno; close(fd); unlink(path); errno = error; }
    return file;
}

int OSHostCopyFile(FILE *source, FILE *destination)
{
    char buffer[8192];
    size_t count;
    int success = 1, error;
    long long position = OSHostTell(source);
    if (position < 0 || !destination || OSHostSeek(source, 0, SEEK_SET)) return 0;
    while ((count = fread(buffer, 1, sizeof(buffer), source)) != 0) {
        if (fwrite(buffer, 1, count, destination) != count) { success = 0; break; }
    }
    if (ferror(source) || ferror(destination) || fflush(destination) ||
        OSHostSeek(destination, position, SEEK_SET)) success = 0;
    error = errno;
    clearerr(source);
    if (OSHostSeek(source, position, SEEK_SET)) return 0;
    errno = error;
    return success;
}
