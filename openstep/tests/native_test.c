/* Backend acceptance harness, deliberately separate from the emulator binary. */
#include "host.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int running = 1, fullscreen, capture;
static unsigned audio_rate = 44100, phase;

static unsigned Fill(void *unused, short *pcm, unsigned frames)
{
    unsigned i;
    (void)unused;
    for (i = 0; i < frames; i++, phase++) {
        pcm[i * 2] = (short)(sin(phase * 440.0 * 6.283185307179586 / audio_rate) * 3000);
        pcm[i * 2 + 1] = (short)(sin(phase * 660.0 * 6.283185307179586 / audio_rate) * 3000);
    }
    return frames;
}

static void Events(const OSHostEvent *e)
{
    if (e->type == OS_EVENT_QUIT) running = 0;
    if (e->type == OS_EVENT_KEY && e->pressed == 1 && e->key == 27) running = 0;
    if (e->type == OS_EVENT_ACTION && e->key == OS_ACTION_FULLSCREEN) {
        fullscreen = !fullscreen;
        if (!OSVideoSetMode(320, 200, 32, 4.0 / 3.0, fullscreen)) running = 0;
    }
    if (e->type == OS_EVENT_ACTION && e->key == OS_ACTION_CAPTURE) {
        capture = !capture; OSVideoCapture(capture);
    }
    if (e->type == OS_EVENT_ACTION && e->key == OS_ACTION_PAUSE)
        OSAudioPause(!OSAudioPaused());
    if (e->type == OS_EVENT_KEY || e->type == OS_EVENT_ACTION)
        fprintf(stderr, "event type=%d key=%d state=%d mods=%u\n", e->type, e->key, e->pressed, e->modifiers);
}

int main(int argc, char **argv)
{
    unsigned start, duration = 20000, frames = 1024, pitch;
    int stage = 0, sound = 0;
    if (argc > 1) duration = (unsigned)atoi(argv[1]) * 1000;
    if (argc > 2) sound = !strcmp(argv[2], "audio");
    if (!OSVideoInit(Events) || !OSVideoSetMode(320, 200, 32, 4.0 / 3.0, 0)) return 1;
    OSVideoTitle("DOSBox-X native backend test (not the emulator)");
    if (sound) {
        if (!OSAudioOpen(&audio_rate, &frames, Fill, NULL)) { OSVideoQuit(); return 2; }
        OSAudioPause(0);
    }
    start = OSHostTicks();
    while (running && (unsigned)(OSHostTicks() - start) < duration) {
        int x, y;
        OSByte *pixels = OSVideoPixels(&pitch);
        unsigned elapsed = OSHostTicks() - start;
        if (elapsed > duration / 3 && stage == 0) {
            if (!OSVideoSetMode(320, 200, 32, 4.0 / 3.0, 1)) break;
            stage = 1; continue;
        }
        if (elapsed > duration * 2 / 3 && stage == 1) {
            if (!OSVideoSetMode(320, 200, 32, 4.0 / 3.0, 0)) break;
            OSVideoBuffered(1); stage = 2; continue;
        }
        for (y = 0; y < 200; y++) for (x = 0; x < 320; x++) {
            OSByte *out = pixels + y * pitch + x * 4;
            out[0] = x < 107 ? 0 : (x < 214 ? 0 : 255);
            out[1] = x >= 107 && x < 214 ? 255 : 0;
            out[2] = x < 107 ? 255 : 0;
            out[3] = 0;
            if (y < 10 || x < 5 || x >= 315 || y >= 190) out[0] = out[1] = out[2] = 255;
        }
        OSVideoEvents(); OSVideoPresent(); OSHostSleep(5);
    }
    fprintf(stderr, "native test finished: stage=%d underruns=%u\n", stage, OSAudioUnderruns());
    OSVideoQuit();
    return stage == 2 ? 0 : 3;
}
