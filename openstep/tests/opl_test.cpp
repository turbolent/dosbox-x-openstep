/* Exercise the real DBOPL synthesizer through its normal mixer interface.
 * This recording sink is only linked into the component test. */
#include "dosbox.h"
#include <math.h>
#include "hardware/dbopl.h"
#include <vector>
#include <assert.h>
#include <stdio.h>

static std::vector<int32_t> samples;
static unsigned channels;
bool adlib_pcm_boost = false;
void adlib_boost32(int32_t *, unsigned int, unsigned int) { assert(false); }
void DEBUG_ShowMsg(const char *, ...) {}
void MixerChannel::AddSamples_m32(Bitu count, const int32_t *data)
{ channels = 1; samples.insert(samples.end(), data, data + count); }
void MixerChannel::AddSamples_s32(Bitu count, const int32_t *data)
{ channels = 2; samples.insert(samples.end(), data, data + count * 2); }

static uint32_t Render(bool opl3, unsigned rate)
{
    DBOPL::Handler synth(opl3);
    MixerChannel sink;
    synth.Init(rate);
    if (opl3) synth.WriteReg(0x105, 1);
    synth.WriteReg(1, 0x20);
    synth.WriteReg(0x20, 1); synth.WriteReg(0x23, 1);
    synth.WriteReg(0x40, 0x18); synth.WriteReg(0x43, 0);
    synth.WriteReg(0x60, 0xf2); synth.WriteReg(0x63, 0xf2);
    synth.WriteReg(0x80, 0x74); synth.WriteReg(0x83, 0x74);
    synth.WriteReg(0xc0, opl3 ? 0x30 : 0);
    synth.WriteReg(0xa0, 0x98); synth.WriteReg(0xb0, 0x31);
    samples.clear();
    for (unsigned i = 0; i < 24; ++i) {
        if (i == 8) synth.WriteReg(0xa0, 0x40);
        if (i == 16) synth.WriteReg(0xb0, 0x11);
        synth.Generate(&sink, 256);
    }
    assert(channels == (opl3 ? 2u : 1u));
    assert(samples.size() == 24u * 256u * channels);
    uint32_t hash = UINT32_C(2166136261);
    unsigned nonzero = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i]) ++nonzero;
        uint32_t value = (uint32_t)samples[i];
        for (unsigned byte = 0; byte < 4; ++byte) {
            hash = (hash ^ (value & 255u)) * UINT32_C(16777619);
            value >>= 8;
        }
    }
    assert(nonzero > 512);
    return hash;
}

int main(void)
{
    uint32_t a = Render(false, 22050), b = Render(true, 44100);
    /* Same samples from the unmodified upstream C++14 synthesizer. */
    assert(a == UINT32_C(0x14818c05) && b == UINT32_C(0xc3be1c71));
    assert(Render(false, 22050) == a && Render(true, 44100) == b);
    printf("OPENSTEP DBOPL sample digests: %08lx %08lx\n", (unsigned long)a, (unsigned long)b);
    return 0;
}
