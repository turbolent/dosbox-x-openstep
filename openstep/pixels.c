/* Interceptor encodings follow quake2/openstep/swimp_pixels.h (GPL-2.0-or-later).
 * Expanded here to accept all software DOSBox renderer formats. */
#include "pixels.h"
#include <string.h>

enum OSPixelLayout OSPixelEncoding(const char *encoding)
{
    if (!encoding) return OS_PIXEL_INVALID;
    if (!strcmp(encoding, "RRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA") ||
        !strcmp(encoding, "RRRRRRRRGGGGGGGGBBBBBBBB--------")) return OS_PIXEL_RGBX;
    if (!strcmp(encoding, "--------RRRRRRRRGGGGGGGGBBBBBBBB")) return OS_PIXEL_BGRX;
    if (!strcmp(encoding, "RRRRRGGGGGBBBBB") ||
        !strcmp(encoding, "-RRRRRGGGGGBBBBB")) return OS_PIXEL_RGB555;
    if (!strcmp(encoding, "RRRRRRGGGGGGBBBBBB") ||
        !strcmp(encoding, "RRRRRGGGGGGBBBBB")) return OS_PIXEL_RGB565;
    return OS_PIXEL_INVALID;
}

int OSPixelBytes(enum OSPixelLayout layout)
{
    switch (layout) {
    case OS_PIXEL_RGBX: case OS_PIXEL_BGRX: return 4;
    case OS_PIXEL_RGB555: case OS_PIXEL_RGB565: return 2;
    default: return 0;
    }
}

int OSPixelViewport(int width, int height, double aspect, int *x, int *y, int *w, int *h)
{
    if (width < 1 || height < 1 || width > 16384 || height > 16384 ||
        !(aspect > 0.01 && aspect < 100) || !x || !y || !w || !h) return 0;
    *w = width; *h = (int)(width / aspect);
    if (*h > height) { *h = height; *w = (int)(height * aspect); }
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
    *x = (width - *w) / 2; *y = (height - *h) / 2;
    return 1;
}

int OSPixelBlit(OSByte *destination, unsigned destination_pitch,
                int destination_width, int destination_height,
                enum OSPixelLayout layout, const OSByte *source,
                unsigned source_pitch, int width, int height, int bits,
                const OSByte *palette)
{
    int x, y, sx, step, remainder, error;
    unsigned r, g, b, packed;
    int db = OSPixelBytes(layout), sb = (bits + 7) / 8;
    if (!destination || !source || !db || width < 1 || height < 1 ||
        destination_width < 1 || destination_height < 1 ||
        width > 16384 || height > 16384 ||
        destination_width > 16384 || destination_height > 16384 ||
        (bits != 8 && bits != 15 && bits != 16 && bits != 32) ||
        (bits == 8 && !palette) || source_pitch / sb < (unsigned)width ||
        destination_pitch / db < (unsigned)destination_width)
        return 0;
    step = width / destination_width;
    remainder = width % destination_width;
    for (y = 0; y < destination_height; y++) {
        const OSByte *row = source + (size_t)(y * height / destination_height) * source_pitch;
        OSByte *out = destination + (size_t)y * destination_pitch;
        sx = error = 0;
        for (x = 0; x < destination_width; x++, out += db) {
            const OSByte *in = row + sx * sb;
            if (bits == 8) {
                r = palette[*in * 4]; g = palette[*in * 4 + 1]; b = palette[*in * 4 + 2];
            } else if (bits == 32) {
                b = in[0]; g = in[1]; r = in[2];
            } else {
                packed = in[0] | (in[1] << 8);
                r = (packed >> (bits == 16 ? 11 : 10)) & 31;
                g = (packed >> 5) & (bits == 16 ? 63 : 31);
                b = packed & 31;
                r = (r << 3) | (r >> 2); b = (b << 3) | (b >> 2);
                g = bits == 16 ? (g << 2) | (g >> 4) : (g << 3) | (g >> 2);
            }
            if (layout == OS_PIXEL_RGBX || layout == OS_PIXEL_BGRX) {
                out[0] = layout == OS_PIXEL_RGBX ? r : b;
                out[1] = g; out[2] = layout == OS_PIXEL_RGBX ? b : r; out[3] = 0;
            } else {
                packed = layout == OS_PIXEL_RGB555 ?
                    ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3) :
                    ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                out[0] = packed & 255; out[1] = packed >> 8;
            }
            sx += step; error += remainder;
            if (error >= destination_width) { error -= destination_width; sx++; }
        }
    }
    return 1;
}
