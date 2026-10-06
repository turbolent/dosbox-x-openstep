/* SoundKit stream lifecycle adapted from quake2/openstep/snddma_openstep.m.
 * Quake II code: Copyright (C) 1997-2001 Id Software, Inc., GPL-2.0-or-later.
 * DOSBox-X native output uses a pull callback instead of Quake's DMA ring. */
#import <Foundation/Foundation.h>
#import <SoundKit/NXSoundOut.h>
#import <SoundKit/NXPlayStream.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

#define AUDIO_BLOCKS 8
static NXSoundOut *device;
static NXPlayStream *stream;
static id delegate;
static vm_address_t memory;
static vm_size_t stride, memory_size;
static short *mix;
static OSAudioFill callback;
static void *callback_data;
static unsigned frames, next_tag = 1, underruns;
static unsigned submitted_frames, completed_frames;
static int ready, paused = 1, stream_paused = 1, servicing;
static struct { unsigned tag, frames; int busy; } blocks[AUDIO_BLOCKS];

@interface DOSBoxSoundDelegate : NSObject
@end
@implementation DOSBoxSoundDelegate
- (void)soundStream:(id)sender didCompleteBuffer:(int)tag
{
    unsigned i;
    if (sender != stream || !ready) return;
    for (i = 0; i < AUDIO_BLOCKS; i++)
        if (blocks[i].busy && blocks[i].tag == (unsigned)tag) {
            completed_frames += blocks[i].frames;
            blocks[i].busy = 0;
            break;
        }
}
- (void)soundStreamDidUnderrun:(id)sender
{
    if (sender == stream && ready) underruns++;
}
- (void)soundStreamDidAbort:(id)sender deviceReserved:(BOOL)reserved
{
    if (sender == stream && reserved) ready = 0;
}
@end

static int Check(NXSoundDeviceError error, const char *operation)
{
    if (error == NX_SoundDeviceErrorNone) return 1;
    fprintf(stderr, "SoundKit: %s failed: %d (%s)\n", operation, (int)error,
        [[NXSoundDevice textForError:error] cString]);
    ready = 0;
    return 0;
}

static unsigned SelectRate(unsigned requested)
{
    const float *rates = NULL;
    float low, high;
    unsigned count = 0, i;
    int rate22 = 0, rate44 = 0;
    if ([device acceptsContinuousStreamSamplingRates]) {
        if (!Check([device getStreamSamplingRatesLow:&low high:&high], "rate query")) return 0;
        rate22 = low <= 22050 && high >= 22050;
        rate44 = low <= 44100 && high >= 44100;
    } else {
        if (!Check([device getStreamSamplingRates:&rates count:&count], "rate query")) return 0;
        for (i = 0; rates && i < count; i++) {
            if (rates[i] == 22050) rate22 = 1;
            if (rates[i] == 44100) rate44 = 1;
        }
    }
    if (requested <= 22050 && rate22) return 22050;
    return rate44 ? 44100 : (rate22 ? 22050 : 0);
}

void OSAudioClose(void)
{
    if (submitted_frames) {
        fprintf(stderr, "SoundKit: submitted=%u completed=%u frames, underruns=%u\n",
                submitted_frames, completed_frames, underruns);
        submitted_frames = completed_frames = 0;
    }
    ready = 0;
    paused = stream_paused = 1;
    callback = NULL;
    if (stream) {
        [stream setDelegate:nil];
        [stream deactivate];
    }
    /* OPENSTEP may dispatch late completion messages. Keep the one device,
     * stream and delegate for process lifetime; tags never reset on reopen. */
    free(mix); mix = NULL;
    if (memory) vm_deallocate(task_self(), memory, memory_size);
    memory = 0;
    memset(blocks, 0, sizeof(blocks));
}

int OSAudioOpen(unsigned *rate, unsigned *block_frames, OSAudioFill fill, void *userdata)
{
    id <NXSoundParameters> parameters;
    unsigned selected;
    OSAudioClose();
    if (!rate || !block_frames || !fill) return 0;
    [NXSoundDevice setTimeout:1000];
    [NXSoundDevice setUseSeparateThread:NO];
    if (!device) device = [[NXSoundOut alloc] init];
    if (!device || !(selected = SelectRate(*rate))) return 0;
    if (!stream) {
        parameters = [[NXSoundParameters alloc] init];
        stream = [[NXPlayStream alloc] initOnDevice:device withParameters:parameters];
        [(id)parameters release];
    }
    if (!stream) return 0;
    parameters = [stream parameters];
    [parameters setParameter:NX_SoundStreamDataEncoding toInt:NX_SoundStreamDataEncoding_Linear16];
    [parameters setParameter:NX_SoundStreamSamplingRate toFloat:selected];
    [parameters setParameter:NX_SoundStreamChannelCount toInt:2];
    if (!delegate) delegate = [[DOSBoxSoundDelegate alloc] init];
    [stream setDelegate:delegate];
    if (!Check([stream activate], "activate") ||
        !Check([stream pauseAtTime:NX_SOUNDSTREAM_TIME_NULL], "pause")) {
        OSAudioClose(); return 0;
    }
    frames = *block_frames;
    if (frames < 256) frames = 256;
    if (frames > 4096) frames = 4096;
    stride = ((frames * 4 + vm_page_size - 1) / vm_page_size) * vm_page_size;
    memory_size = stride * AUDIO_BLOCKS;
    mix = (short *)calloc(frames * 2, sizeof(short));
    if (!mix || vm_allocate(task_self(), &memory, memory_size, TRUE) != KERN_SUCCESS) {
        OSAudioClose(); return 0;
    }
    callback = fill; callback_data = userdata;
    *rate = selected; *block_frames = frames;
    ready = 1; underruns = 0;
    fprintf(stderr, "SoundKit: %u Hz, 16-bit stereo, %u frames/block\n", selected, frames);
    return 1;
}

void OSAudioPause(int pause)
{
    paused = !!pause;
    if (!ready) return;
    if (paused) {
        [stream abort:nil];
        if (!Check([stream lastError], "abort")) return;
        if (!Check([stream pauseAtTime:NX_SOUNDSTREAM_TIME_NULL], "pause")) return;
        memset(blocks, 0, sizeof(blocks));
        stream_paused = 1;
    }
}

void OSAudioService(void)
{
    unsigned i, sample, queued = 0;
    if (!ready || paused || servicing || !callback) return;
    servicing = 1;
    for (i = 0; i < AUDIO_BLOCKS; i++) queued += !!blocks[i].busy;
    if (!queued && !stream_paused) {
        if (!Check([stream pauseAtTime:NX_SOUNDSTREAM_TIME_NULL], "rebuffer")) goto done;
        stream_paused = 1;
    }
    for (i = 0; i < AUDIO_BLOCKS; i++) {
        OSByte *out;
        unsigned produced;
        if (blocks[i].busy) continue;
        produced = callback(callback_data, mix, frames);
        if (!ready || paused) goto done;
        if (!produced) break;
        if (produced > frames) { ready = 0; goto done; }
        out = (OSByte *)(memory + stride * i);
        for (sample = 0; sample < produced * 2; sample++) {
            unsigned value = (unsigned short)mix[sample];
            out[sample * 2] = value >> 8; out[sample * 2 + 1] = value & 255;
        }
        /* Stop rather than reuse a tag after two billion submissions. */
        if (next_tag > 0x7fffffffU) { ready = 0; goto done; }
        blocks[i].tag = next_tag++;
        blocks[i].frames = produced;
        blocks[i].busy = 1;
        if (!Check([stream playBuffer:out size:produced * 4 tag:blocks[i].tag], "submit")) goto done;
        submitted_frames += produced;
        queued++;
    }
    /* IOAudio can consume four blocks into its DMA ring immediately. As in
     * Quake II, prime eight so completion callbacks cannot drain our entire
     * producer queue before the next emulated block exists. */
    if (stream_paused && queued == AUDIO_BLOCKS) {
        if (!Check([stream resumeAtTime:NX_SOUNDSTREAM_TIME_NULL], "resume")) goto done;
        stream_paused = 0;
    }
done:
    servicing = 0;
}

unsigned OSAudioUnderruns(void) { return underruns; }
int OSAudioPaused(void) { return paused; }
