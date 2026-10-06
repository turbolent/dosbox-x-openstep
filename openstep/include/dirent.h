#ifndef DOSBOX_OPENSTEP_DIRENT_H
#define DOSBOX_OPENSTEP_DIRENT_H
#if defined(NeXT)
/* Native BSD libc returns struct direct; the POSIX declarations describe
 * a different entry layout and require the separate POSIX library. */
#include <sys/dir.h>
#define dirent direct
#else
#include_next <dirent.h>
#endif
#endif
