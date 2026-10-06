#include "host.h"
#include "pixels.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#ifdef NeXT
#include <libc.h>
#endif
#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1LL)
#endif

static void test_pixels(void)
{
    OSByte palette[1024], input[12], output[64];
    unsigned i;
    int x, y, w, h;
    assert(OSPixelViewport(1280, 720, 4.0 / 3.0, &x, &y, &w, &h));
    assert(x == 160 && y == 0 && w == 960 && h == 720);
    assert(OSPixelViewport(640, 600, 4.0 / 3.0, &x, &y, &w, &h));
    assert(x == 0 && y == 60 && w == 640 && h == 480);
    assert(!OSPixelViewport(0, 600, 4.0 / 3.0, &x, &y, &w, &h));
    memset(palette, 0, sizeof(palette));
    palette[4] = 255; palette[9] = 255; palette[14] = 255;
    input[0] = 1; input[1] = 2; input[2] = 99;
    input[3] = 3; input[4] = 1; input[5] = 99;
    memset(output, 0xcc, sizeof(output));
    assert(OSPixelBlit(output, 20, 4, 2, OS_PIXEL_BGRX, input, 3, 2, 2, 8, palette));
    assert(output[2] == 255 && output[6] == 255 && output[9] == 255 && output[13] == 255);
    assert(output[20] == 255 && output[30] == 255);
    for (i = 16; i < 20; i++) assert(output[i] == 0xcc);
    assert(output[40] == 0xcc);
    input[0] = 0; input[1] = 0xf8;
    assert(OSPixelBlit(output, 4, 1, 1, OS_PIXEL_RGBX, input, 2, 1, 1, 16, NULL));
    assert(output[0] == 255 && output[1] == 0 && output[2] == 0);
    input[0] = 0xe0; input[1] = 3;
    assert(OSPixelBlit(output, 4, 1, 1, OS_PIXEL_RGBX, input, 2, 1, 1, 15, NULL));
    assert(output[0] == 0 && output[1] == 255 && output[2] == 0);
    input[0] = 255; input[1] = 128; input[2] = 64; input[3] = 0;
    assert(OSPixelBlit(output, 2, 1, 1, OS_PIXEL_RGB565, input, 4, 1, 1, 32, NULL));
    assert(output[0] == 0x1f && output[1] == 0x44);
    assert(!OSPixelBlit(output, 1, 1, 1, OS_PIXEL_RGB565, input, 4, 1, 1, 32, NULL));
    assert(!OSPixelBlit(output, 4, 1, 1, OS_PIXEL_RGBX, input, 1, 1, 1, 8, NULL));
    assert(OSPixelEncoding("unknown") == OS_PIXEL_INVALID);
    assert(OSPixelEncoding("--------RRRRRRRRGGGGGGGGBBBBBBBB") == OS_PIXEL_BGRX);
}

static void test_files(void)
{
    FILE *file = tmpfile();
    long long size;
    assert(file);
    assert(fwrite("native", 1, 6, file) == 6);
    assert(OSHostSeek(file, -2, SEEK_CUR) == 0 && OSHostTell(file) == 4);
    assert(OSHostFileSize(file, &size) == 0 && size == 6 && OSHostTell(file) == 4);
    assert(OSHostSeek(file, -1, SEEK_END) == 0 && fgetc(file) == 'e');
    assert(OSHostSeek(file, 0x80000000LL, SEEK_SET) == -1);
    assert(OSHostSeek(file, LLONG_MAX, SEEK_CUR) == -1);
    assert(OSHostSeek(file, LLONG_MIN, SEEK_END) == -1);
    assert(OSHostSeek(file, -7, SEEK_END) == -1);
    assert(OSHostTell(file) == 6);
    fclose(file);
}

static void test_overlay_copy(void)
{
    char path[] = "openstep/output/copy-test-XXXXXX";
    char data[16384], restored[16384];
    int fd = mkstemp(path);
    FILE *source = tmpfile(), *destination;
    int descriptors[2];
    assert(fd >= 0 && source);
    assert(write(fd, "keep", 4) == 4);
    close(fd);
    assert(!OSHostCreateFile(path) && errno == EEXIST);
    destination = fopen(path, "rb");
    assert(destination && fread(restored, 1, 4, destination) == 4);
    assert(!memcmp(restored, "keep", 4)); fclose(destination);
    assert(!unlink(path));
    memset(data, 0x5a, sizeof(data));
    assert(fwrite(data, 1, sizeof(data), source) == sizeof(data));
    assert(!fflush(source) && !fseek(source, 123, SEEK_SET));
    destination = OSHostCreateFile(path);
    assert(destination && OSHostCopyFile(source, destination));
    assert(ftell(source) == 123 && ftell(destination) == 123);
    rewind(destination);
    assert(fread(restored, 1, sizeof(restored), destination) == sizeof(restored));
    assert(!memcmp(data, restored, sizeof(data)));
    fclose(destination); assert(!unlink(path));

    /* A broken pipe exercises a real write/flush error without filling disk.
     * OPENSTEP does not enforce RLIMIT_FSIZE for this filesystem. */
    assert(!pipe(descriptors));
    close(descriptors[0]);
    signal(SIGPIPE, SIG_IGN);
    destination = fdopen(descriptors[1], "w"); assert(destination);
    assert(!OSHostCopyFile(source, destination));
    assert(ftell(source) == 123 && fgetc(source) == 0x5a);
    fclose(destination);
    fclose(source);
}

int main(void)
{
    OSUInt32 before;
    assert(sizeof(OSUInt32) == 4 && sizeof(OSWord) == 2 && sizeof(short) == 2);
    test_pixels(); test_files(); test_overlay_copy();
    before = OSHostTicks(); OSHostSleep(20);
    assert((OSUInt32)(OSHostTicks() - before) >= 20);
    puts("OPENSTEP host tests passed");
    return 0;
}
