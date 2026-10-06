/* Declarations missing from OPENSTEP's native BSD header profile. */
#ifndef DOSBOX_OPENSTEP_LIBC_COMPAT_H
#define DOSBOX_OPENSTEP_LIBC_COMPAT_H
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#if defined(NeXT)
#include <libc.h>
#include <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXU 0700
#endif
#ifndef O_ACCMODE
#define O_ACCMODE 3
#endif
#ifdef __cplusplus
extern "C" {
#endif
int snprintf(char *buffer, size_t size, const char *format, ...);
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
char *getcwd(char *buffer, size_t size);
unsigned long long strtoull(const char *text, char **end, int base);
char *strdup(const char *text);
char *realpath(const char *path, char *resolved);
#ifdef __cplusplus
}
#endif
#endif
#endif
