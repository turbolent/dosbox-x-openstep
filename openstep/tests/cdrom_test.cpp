/* ISO/CUE/CDDA integration fixture. The real image parser and native readers
 * run against a recording mixer and guest-memory sink, not an emulated PC. */
#include "dosbox.h"
#include "dos/cdrom.h"
#include "dos_inc.h"
#include "setup.h"
#include <vector>

bool qmount = true;
MachineType machine = MCH_VGA;
std::string RunningProgram;
static MixerChannel *channel;
static std::vector<int16_t> audio;
static std::vector<uint8_t> guest;
void DEBUG_ShowMsg(const char *, ...) {}
void AddExitFunction(SectionFunction, const char *, bool) {}
bool DOS_WriteFile(uint16_t, const uint8_t *, uint16_t *, bool) { return true; }
void MEM_BlockWrite(LinearPt, const void *data, size_t size)
{ const uint8_t *p = (const uint8_t *)data; guest.assign(p, p + size); }
MixerChannel *MIXER_AddChannel(MIXER_Handler handler, Bitu rate, const char *)
{ channel = new MixerChannel(); channel->handler = handler; channel->SetFreq(rate); return channel; }
void MIXER_DelChannel(MixerChannel *c) { delete c; channel = NULL; }
void MixerChannel::Enable(bool value) { enabled = value; }
void MixerChannel::SetFreq(Bitu rate, Bitu denominator) { freq_n = rate; freq_d = denominator; }
void MixerChannel::SetScale(float left, float right) { scale[0] = left; scale[1] = right; }
void MixerChannel::AddSamples_s16(Bitu count, const int16_t *samples)
{ audio.insert(audio.end(), samples, samples + count * 2); }
void MixerChannel::AddSamples_m16(Bitu, const int16_t *) { assert(false); }
void MixerChannel::AddSamples_m16_nonnative(Bitu, const int16_t *) { assert(false); }
void MixerChannel::AddSamples_s16_nonnative(Bitu, const int16_t *) { assert(false); }

static void Put16(std::vector<uint8_t> &bytes, unsigned offset, unsigned value)
{ bytes[offset] = value & 255; bytes[offset + 1] = (value >> 8) & 255; }
static void Put32(std::vector<uint8_t> &bytes, unsigned offset, unsigned value)
{ Put16(bytes, offset, value); Put16(bytes, offset + 2, value >> 16); }
static void Write(const char *path, const void *data, size_t size)
{ FILE *f = fopen(path, "wb"); assert(f); assert(fwrite(data, 1, size, f) == size); assert(!fclose(f)); }
static void Text(const char *path, const char *text) { Write(path, text, strlen(text)); }
static void Stereo(std::vector<uint8_t> &bytes, unsigned offset, unsigned frames)
{ for (unsigned i = 0; i < frames; ++i) { Put16(bytes, offset + i * 4, 1234); Put16(bytes, offset + i * 4 + 2, (uint16_t)-2345); } }
static void CheckAudio(unsigned frames)
{
    assert(audio.size() == frames * 2);
    for (unsigned i = 0; i < frames; ++i) assert(audio[i*2] == 1234 && audio[i*2+1] == -2345);
}

int main(void)
{
    const char *iso_path = "openstep/output/cd-fixture.iso";
    const char *bin_path = "openstep/output/cd-fixture.bin";
    const char *wav_path = "openstep/output/cd-fixture.wav";
    const char *cue_path = "openstep/output/cd-fixture.cue";
    std::vector<uint8_t> iso(32 * 2048, 0), raw(36 * 2352, 0), sector(2352, 0);
    memcpy(&iso[16 * 2048], "\1CD001\1", 7);
    memset(&iso[17 * 2048], 0x5a, 2048);
    Write(iso_path, &iso[0], iso.size());
    for (unsigned i = 0; i < 32; ++i) memcpy(&raw[i * 2352 + 16], &iso[i * 2048], 2048);
    Stereo(raw, 32 * 2352, 4 * 588);
    Write(bin_path, &raw[0], raw.size());
    {
        CDROM_Interface_Image cd(0);
        assert(cd.SetDevice(iso_path, -1));
        assert(cd.HasDataTrack() && !cd.HasAudioTrack());
        assert(cd.ReadSectorsHost(&sector[0], false, 17, 1) && sector[0] == 0x5a);
        assert(cd.ReadSectors(0, false, 17, 1) && guest.size() == 2048 && guest[2047] == 0x5a);
        assert(!cd.ReadSectorsHost(&sector[0], false, 32, 1));
        assert(!cd.ReadSectorsHost(&sector[0], true, 17, 1));
        assert(!cd.PlayAudioSector(0, 1));
        int first, last; TMSF out;
        assert(cd.GetAudioTracks(first, last, out) && first == 1 && last == 1);
        assert(MSF_TO_FRAMES(out.min, out.sec, out.fr) == 32 + 150);
    }
    Text(cue_path, "FILE \"cd-fixture.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 01 00:00:32\n");
    {
        CDROM_Interface_Image cd(0);
        assert(cd.SetDevice(cue_path, -1));
        assert(cd.ReadSectorsHost(&sector[0], false, 17, 1) && sector[0] == 0x5a);
        assert(cd.ReadSectorsHost(&sector[0], true, 17, 1) && sector[16] == 0x5a);
        assert(cd.PlayAudioSector(32, 4) && channel->freq_n == 44100);
        audio.clear(); channel->handler(100); CheckAudio(100);
        unsigned char attr, track, index; TMSF rel, abs;
        assert(cd.GetAudioSub(attr, track, index, rel, abs) && track == 2 && index == 1);
        assert(MSF_TO_FRAMES(abs.min, abs.sec, abs.fr) == 32 + 150);
        cd.PauseAudio(false); channel->handler(100); CheckAudio(100);
        cd.PauseAudio(true); channel->handler(3000); CheckAudio(4 * 588);
        bool playing, paused; cd.GetAudioStatus(playing, paused); assert(!playing && !paused);
        assert(!cd.PlayAudioSector(450000, 1));
        TCtrl control = {{1, 0, 2, 3}, {128, 255, 0, 0}};
        cd.ChannelControl(control); assert(channel->scale[0] == 1 && channel->scale[1] == 1);
        assert(cd.PlayAudioSector(33, 1)); audio.clear(); channel->handler(1);
        assert(audio.size() == 2 && audio[0] == -2345 * 128 / 255 && audio[1] == 1234);
        cd.StopAudio();
        control.out[0] = 0; control.out[1] = 1; control.vol[0] = 255; cd.ChannelControl(control);
    }
    // Include an odd-sized RIFF chunk and a partial final CD sector.
    const unsigned wav_frames = 2 * 588 + 1;
    std::vector<uint8_t> wav(56 + wav_frames * 4, 0);
    memcpy(&wav[0], "RIFF", 4); Put32(wav, 4, (unsigned)wav.size() - 8);
    memcpy(&wav[8], "WAVEJUNK", 8); Put32(wav, 16, 3);
    memcpy(&wav[24], "fmt ", 4); Put32(wav, 28, 16);
    Put16(wav, 32, 1); Put16(wav, 34, 2); Put32(wav, 36, 44100);
    Put32(wav, 40, 176400); Put16(wav, 44, 4); Put16(wav, 46, 16);
    memcpy(&wav[48], "data", 4); Put32(wav, 52, wav_frames * 4);
    Stereo(wav, 56, wav_frames); Write(wav_path, &wav[0], wav.size());
    Text(cue_path, "FILE \"cd-fixture.wav\" WAVE\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n");
    {
        CDROM_Interface_Image cd(0); assert(cd.SetDevice(cue_path, -1));
        assert(cd.PlayAudioSector(0, 3)); audio.clear(); channel->handler(3 * 588);
        assert(audio.size() == 3 * 588 * 2);
        for (unsigned i = 0; i < wav_frames; ++i) assert(audio[i*2] == 1234 && audio[i*2+1] == -2345);
        for (unsigned i = wav_frames * 2; i < audio.size(); ++i) assert(audio[i] == 0);
    }
    Put16(wav, 46, 8); Write(wav_path, &wav[0], wav.size());
    { CDROM_Interface_Image cd(0); assert(!cd.SetDevice(cue_path, -1)); }
    remove(iso_path); remove(bin_path); remove(wav_path); remove(cue_path);
    puts("OPENSTEP CD images: ISO, BIN/CUE, PCM WAV, CDDA position/pause/volume/end passed");
    return 0;
}
