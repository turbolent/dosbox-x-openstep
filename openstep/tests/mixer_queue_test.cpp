/* Exercise the actual mixer consumer with deterministic stereo samples. */
#include "../../src/hardware/mixer.cpp"

static void Prepare(unsigned start, unsigned count)
{
    memset(&mixer, 0, sizeof(mixer));
    mixer.freq = 44100;
    mixer.blocksize = 1024;
    mixer.work_wrap = MIXER_BUFSIZE;
    mixer.mastervol[0] = mixer.mastervol[1] = 1;
    mixer.work_out = start;
    mixer.work_in = (start + count) % MIXER_BUFSIZE;
    for (unsigned i = 0; i < count; i++) {
        unsigned position = (start + i) % MIXER_BUFSIZE;
        mixer.work[position][0] = (int32_t)(i + 1) * (1 << MIXER_VOLSHIFT);
        mixer.work[position][1] = -(int32_t)(i + 1) * (1 << MIXER_VOLSHIFT);
    }
}

int main(void)
{
    short samples[2048];
    const unsigned starts[] = {0, MIXER_BUFSIZE - 512};
    for (unsigned pass = 0; pass < 2; pass++) {
        Prepare(starts[pass], 6144);
        /* A 139 ms catch-up burst must survive six consecutive SoundKit
         * requests, including one crossing the ring boundary. */
        for (unsigned block = 0; block < 6; block++) {
            assert(MIXER_SoundKitFill(NULL, samples, 1024) == 1024);
            for (unsigned i = 0; i < 1024; i++) {
                assert(samples[i * 2] == (short)(block * 1024 + i + 1));
                assert(samples[i * 2 + 1] == -samples[i * 2]);
            }
        }
        assert(mixer.work_in == mixer.work_out);
        assert(!MIXER_SoundKitFill(NULL, samples, 1024));
    }
    Prepare(0, 1023);
    assert(!MIXER_SoundKitFill(NULL, samples, 1024) && !mixer.work_out);
    Prepare(0, 4096);
    mixer.prebuffer_wait = true;
    mixer.prebuffer_samples = 5000;
    assert(!MIXER_SoundKitFill(NULL, samples, 1024) && !mixer.work_out);
    Prepare(0, 14000);
    assert(MIXER_SoundKitFill(NULL, samples, 1024) == 1024);
    assert(mixer.work_in - mixer.work_out == 1024);
    puts("SoundKit mixer: burst continuity, ring wrap, readiness and stale backlog passed");
    return 0;
}
