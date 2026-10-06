#ifndef DOSBOX_OPENSTEP_PIXELS_H
#define DOSBOX_OPENSTEP_PIXELS_H
#include "host.h"

#ifdef __cplusplus
extern "C" {
#endif

enum OSPixelLayout { OS_PIXEL_INVALID, OS_PIXEL_RGBX, OS_PIXEL_BGRX,
                     OS_PIXEL_RGB555, OS_PIXEL_RGB565 };
enum OSPixelLayout OSPixelEncoding(const char *encoding);
int OSPixelBytes(enum OSPixelLayout layout);
int OSPixelViewport(int width, int height, double aspect, int *x, int *y, int *w, int *h);
/* Source and destination must not overlap. Padding is never modified. */
int OSPixelBlit(OSByte *destination, unsigned destination_pitch,
                int destination_width, int destination_height,
                enum OSPixelLayout layout, const OSByte *source,
                unsigned source_pitch, int width, int height, int bits,
                const OSByte *palette);
#ifdef __cplusplus
}
#endif
#endif
