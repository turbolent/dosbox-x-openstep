/* Native OPENSTEP host interface. No emulator or SDL types cross this ABI. */
#ifndef DOSBOX_OPENSTEP_HOST_H
#define DOSBOX_OPENSTEP_HOST_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char OSByte;
typedef unsigned short OSWord;
typedef unsigned int OSUInt32;
typedef unsigned long long OSUInt64;

/* Input uses Unicode characters, AppKit function-key values, and these flags. */
enum { OS_MOD_SHIFT = 1, OS_MOD_CONTROL = 2, OS_MOD_ALT = 4,
       OS_MOD_COMMAND = 8, OS_MOD_CAPS = 16, OS_MOD_KEYPAD = 32 };
enum { OS_EVENT_KEY, OS_EVENT_MOTION, OS_EVENT_BUTTON, OS_EVENT_FOCUS,
       OS_EVENT_QUIT, OS_EVENT_ACTION };
enum { OS_ACTION_FULLSCREEN, OS_ACTION_CAPTURE, OS_ACTION_PAUSE,
       OS_ACTION_RESET, OS_ACTION_MOUNT, OS_ACTION_UNMOUNT };
typedef struct OSHostEvent {
    int type, key, pressed, scancode;
    unsigned modifiers;
    int x, y, dx, dy;
    const char *path; /* borrowed for the duration of the callback */
} OSHostEvent;
typedef void (*OSEventHandler)(const OSHostEvent *event);

OSUInt32 OSHostTicks(void);
void OSHostSleep(unsigned milliseconds);
/* Borrowed loader path, independent of Workspace's argv[0]. */
const char *OSHostExecutablePath(void);
/* Checked signed-32-bit stdio offsets, even when tested on a wider host. */
int OSHostSeek(FILE *file, long long offset, int origin);
long long OSHostTell(FILE *file);
int OSHostFileSize(FILE *file, long long *size);
/* Exclusive creation never truncates an existing overlay file. */
FILE *OSHostCreateFile(const char *path);
/* Copy and flush all bytes; preserve source position even when copying fails. */
int OSHostCopyFile(FILE *source, FILE *destination);
void *OSHostExecutableAlloc(unsigned bytes);
void OSHostExecutableFree(void *memory, unsigned bytes);

int OSVideoInit(OSEventHandler handler);
/* Entry must catch C++ exceptions before returning through the AppKit stack. */
void OSApplicationRun(void (*entry)(void *), void *userdata);
void OSVideoQuit(void);
/* Source pixels are top-down; 8=palette, 15=RGB555, 16=RGB565, 32=0x00RRGGBB. */
int OSVideoSetMode(int width, int height, int bits, double aspect, int fullscreen);
OSByte *OSVideoPixels(unsigned *pitch);
void OSVideoPalette(unsigned first, unsigned count, const OSByte *rgba);
void OSVideoPresent(void);
void OSVideoEvents(void);
void OSVideoCapture(int capture);
int OSVideoCaptured(void);
void OSVideoShowCursor(int visible);
int OSVideoScreenCount(void);
/* View coordinates to normalized guest coordinates, clamped at letterbox edges. */
int OSVideoMapPoint(int x, int y, float *guest_x, float *guest_y);
void OSVideoTitle(const char *title);
int OSVideoFullscreen(void);
int OSVideoActive(void);
void OSVideoBuffered(int buffered);
/* Borrowed result, valid until the next chooser call. Multiple paths use '|'. */
const char *OSVideoChoosePaths(const char *title, int directories, int multiple,
                              int pattern_count, const char *const *patterns);
/* Alert results: first=1, second=0, third=-1. */
int OSVideoAlert(const char *title, const char *message, const char *first,
                 const char *second, const char *third);
void OSVideoDimensions(unsigned *window_width, unsigned *window_height,
                       unsigned *screen_width, unsigned *screen_height);
/* A malloc-owned, terminated UTF-16 copy, or NULL when unavailable. */
OSWord *OSClipboardRead(void);

/* Called only on the emulator/main thread. Return the number of native signed
 * stereo PCM frames produced (at most frames), or zero if none are ready yet.
 * A hardware queue refill must never advance the emulated machine's mixer. */
typedef unsigned (*OSAudioFill)(void *userdata, short *samples, unsigned frames);
int OSAudioOpen(unsigned *rate, unsigned *frames, OSAudioFill fill, void *userdata);
void OSAudioService(void);
void OSAudioPause(int paused);
int OSAudioPaused(void);
void OSAudioClose(void);
unsigned OSAudioUnderruns(void);

#ifdef __cplusplus
}
#endif
#endif
