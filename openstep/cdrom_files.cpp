/* Raw image and PCM WAV readers for the native CD image backend. Offsets pass
 * through checked OPENSTEP stdio; no SDL codecs, CHD threads or host CD device. */
#include "dosbox.h"
#include "dos/cdrom.h"
#include "host.h"
#include <algorithm>

CDROM_Interface_Image::BinaryFile::BinaryFile(const char *path, bool &error)
    : TrackFile(RAW_SECTOR_SIZE), file(fopen(path, "rb")), length(-1)
{
    error = !file || OSHostFileSize(file, &length) != 0;
}
CDROM_Interface_Image::BinaryFile::~BinaryFile() { if (file) fclose(file); }
bool CDROM_Interface_Image::BinaryFile::seek(int64_t offset)
{
    return file && offset >= 0 && offset <= length && OSHostSeek(file, offset, SEEK_SET) == 0;
}
bool CDROM_Interface_Image::BinaryFile::read(uint8_t *buffer, int64_t offset, int count)
{
    if (!buffer || count < 0 || offset < 0 || offset > length || count > length - offset ||
        !seek(offset)) return false;
    return fread(buffer, 1, (size_t)count, file) == (size_t)count;
}
uint16_t CDROM_Interface_Image::BinaryFile::decode(uint8_t *buffer)
{
    if (!seek(audio_pos)) return 0;
    unsigned count = (unsigned)std::min<long long>(chunkSize, length - audio_pos);
    count = (unsigned)fread(buffer, 1, count, file);
    audio_pos += count;
    return (uint16_t)count;
}
bool CDROM_Interface_Image::BinaryFile::isLittleEndian() { return true; }
int64_t CDROM_Interface_Image::BinaryFile::getLength() { return length; }

static uint32_t ReadLE32(const unsigned char *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static unsigned ReadLE16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }

CDROM_Interface_Image::AudioFile::AudioFile(const char *path, bool &error)
    : TrackFile(4096), file(fopen(path, "rb")), data_start(0), data_size(0), position(0)
{
    unsigned char header[16];
    long long size;
    bool format = false, data = false;
    error = true;
    if (!file || OSHostFileSize(file, &size) != 0 || size < 12 ||
        fread(header, 1, 12, file) != 12 || memcmp(header, "RIFF", 4) ||
        memcmp(header + 8, "WAVE", 4)) return;
    const long long end = (long long)ReadLE32(header + 4) + 8;
    if (end > size || end < 12) return;
    long long cursor = 12;
    while (cursor + 8 <= end) {
        if (OSHostSeek(file, cursor, SEEK_SET) != 0 || fread(header, 1, 8, file) != 8) return;
        const uint32_t count = ReadLE32(header + 4);
        cursor += 8;
        if (count > end - cursor) return;
        if (!memcmp(header, "fmt ", 4)) {
            if (count < 16 || fread(header, 1, 16, file) != 16) return;
            /* CDDA uses stereo 44.1 kHz signed 16-bit PCM. Reject compressed
             * or differently sampled WAVs explicitly instead of mis-timing them. */
            if (ReadLE16(header) != 1 || ReadLE16(header + 2) != 2 ||
                ReadLE32(header + 4) != 44100 || ReadLE32(header + 8) != 176400 ||
                ReadLE16(header + 12) != 4 || ReadLE16(header + 14) != 16) return;
            format = true;
        } else if (!memcmp(header, "data", 4) && !data) {
            if (!count || count % 4) return;
            data_start = (uint32_t)cursor; data_size = count; data = true;
        }
        cursor += count + (count & 1U);
    }
    if (!format || !data || !seek(0)) return;
    error = false;
}
CDROM_Interface_Image::AudioFile::~AudioFile() { if (file) fclose(file); }
bool CDROM_Interface_Image::AudioFile::read(uint8_t *buffer, int64_t offset, int count)
{
    if (!buffer || count < 0 || offset < 0 || (uint64_t)offset > data_size ||
        (unsigned)count > data_size - offset || !seek(offset)) return false;
    const size_t got = fread(buffer, 1, count, file);
    position += (uint32_t)got;
    return got == (unsigned)count;
}
bool CDROM_Interface_Image::AudioFile::seek(int64_t offset)
{
    if (!file || offset < 0 || (uint64_t)offset > data_size || offset % 4 ||
        OSHostSeek(file, (long long)data_start + offset, SEEK_SET) != 0) return false;
    position = (uint32_t)offset;
    return true;
}
uint16_t CDROM_Interface_Image::AudioFile::decode(uint8_t *buffer)
{
    if (!file || !buffer || position > data_size) return 0;
    unsigned count = std::min<unsigned>(chunkSize, data_size - position);
    count = (unsigned)fread(buffer, 1, count, file);
    position += count;
    return (uint16_t)count;
}
bool CDROM_Interface_Image::AudioFile::isLittleEndian() { return true; }
uint32_t CDROM_Interface_Image::AudioFile::getRate() { return 44100; }
uint8_t CDROM_Interface_Image::AudioFile::getChannels() { return 2; }
int64_t CDROM_Interface_Image::AudioFile::getLength() { return data_size; }

void CDROM_Interface_Image::CDAudioCallBack(Bitu frames)
{
    if (!player.cd || !player.channel || !player.isPlaying || player.isPaused) return;
    while (frames && player.playbackRemaining > 0) {
        const uint32_t played = player.playbackTotal - player.playbackRemaining;
        const uint32_t sector = player.startFrame + played / RAW_SECTOR_SIZE;
        const unsigned within = played % RAW_SECTOR_SIZE;
        const int index = player.cd->GetTrack(sector) - 1;
        if (index < 0 || (size_t)index + 1 >= player.cd->tracks.size()) {
            player.cd->StopAudio(); return;
        }
        const Track &track = player.cd->tracks[index];
        if (track.attr != 0 || !track.file) { player.cd->StopAudio(); return; }
        unsigned count = std::min<unsigned>((unsigned)frames, (RAW_SECTOR_SIZE - within) / 4);
        count = std::min<unsigned>(count, player.playbackRemaining / 4);
        int16_t samples[RAW_SECTOR_SIZE / 2];
        memset(samples, 0, count * 4);
        if (sector >= track.start) {
            const int64_t offset = (int64_t)track.skip +
                (int64_t)(sector - track.start) * track.sectorSize + within;
            const int64_t available = track.file->getLength() - offset;
            /* A final partial WAV sector is padded with silence. */
            unsigned bytes = available > 0 ?
                (unsigned)std::min<int64_t>(count * 4, available) : 0;
            if (bytes && !track.file->read((uint8_t *)samples, offset, bytes)) {
                player.cd->StopAudio(); return;
            }
        }
        if (player.ctrlUsed) for (unsigned i = 0; i < count; ++i) {
            const int16_t left = samples[i * 2], right = samples[i * 2 + 1];
            for (unsigned ch = 0; ch < 2; ++ch) {
                int sample = player.ctrlData.out[ch] == 0 ? left :
                    player.ctrlData.out[ch] == 1 ? right : 0;
                samples[i * 2 + ch] = (int16_t)(sample * player.ctrlData.vol[ch] / 255);
            }
        }
        player.channel->AddSamples_s16(count, samples);
        player.playbackRemaining -= count * 4;
        player.currFrame = player.startFrame +
            (player.playbackTotal - player.playbackRemaining) / RAW_SECTOR_SIZE;
        frames -= count;
    }
    if (!player.playbackRemaining) player.cd->StopAudio();
}
