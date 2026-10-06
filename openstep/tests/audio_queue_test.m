/* Native SoundKit regression: a producer can decline or partially fill a
 * request. Inspect the transport here without exposing test APIs to the app. */
#import <AppKit/AppKit.h>
#include <assert.h>
#include <math.h>
#include "../audio.m"

static unsigned available, produced_total, refused, phase, test_rate;
static unsigned Produce(void *unused, short *pcm, unsigned capacity)
{
    unsigned i, count = available < capacity ? available : capacity;
    (void)unused;
    if (!count) { refused++; return 0; }
    for (i = 0; i < count; i++, phase++)
        pcm[i * 2] = pcm[i * 2 + 1] =
            (short)(1200 * sin(phase * 440.0 * 6.283185307179586 / test_rate));
    available -= count;
    produced_total += count;
    return count;
}

static void Pump(void)
{
    NSEvent *event;
    while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask
        untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES]))
        [NSApp sendEvent:event];
    OSAudioService();
}

int main(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    unsigned rates[] = {44100, 22050}, pass;
    [NSApplication sharedApplication];
    for (pass = 0; pass < 2; pass++) {
        unsigned block = 1024, start, ticks, target, initial;
        test_rate = rates[pass];
        available = produced_total = refused = phase = 0;
        assert(OSAudioOpen(&test_rate, &block, Produce, NULL));
        OSAudioPause(0);
        OSAudioService(); OSAudioService();
        assert(!submitted_frames && stream_paused && refused == 2);
        available = 1024;
        OSAudioService();
        assert(submitted_frames == 1024 && stream_paused);
        available = 512;
        OSAudioService();
        assert(submitted_frames == 1536 && stream_paused);
        available = block * (AUDIO_BLOCKS - 2);
        OSAudioService();
        initial = 1536 + block * (AUDIO_BLOCKS - 2);
        assert(submitted_frames == initial && !stream_paused);
        start = OSHostTicks();
        while ((ticks = OSHostTicks() - start) < 3000) {
            /* Produce a full block only when its emulated time has passed. */
            target = initial + ((ticks * test_rate / 1000) / block) * block;
            available = target - produced_total;
            Pump();
            assert(submitted_frames == produced_total);
            OSHostSleep(2);
        }
        assert(completed_frames && produced_total > test_rate * 2);
        OSAudioPause(1);
        available = 0;
        target = submitted_frames;
        Pump();
        assert(submitted_frames == target && stream_paused);
        OSAudioPause(0);
        Pump();
        assert(submitted_frames == target && stream_paused);
        available = block * AUDIO_BLOCKS;
        Pump();
        assert(submitted_frames == target + block * AUDIO_BLOCKS && !stream_paused);
        fprintf(stderr, "SoundKit producer test: %u Hz, %u frames, %u empty requests, %u underruns\n",
                test_rate, produced_total, refused, underruns);
        assert(underruns == 0);
        OSAudioClose();
    }
    [pool release];
    return 0;
}
