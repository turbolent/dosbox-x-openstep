/* Exercise the shipping renderer/display bridge with either a recording host
 * or the real Interceptor backend. This harness does not execute a guest CPU. */
#include "dosbox.h"
#include "render.h"
#include "vga.h"
#include "host.h"
#include <stdarg.h>
#include <vector>

VGA_Type vga;
static std::vector<uint8_t> temp_line;
bool TempLineAlloc(unsigned int width) { temp_line.resize(width * 4 + 1024); return true; }
void TempLineFree(void) { temp_line.clear(); }
void E_Exit(const char *message, ...) DBX_NOEXCEPT
{
    va_list ap; va_start(ap, message); vfprintf(stderr, message, ap); va_end(ap);
    abort();
}
void RENDER_Shutdown(void);

#ifndef OS_RENDER_GUI_TEST
static std::vector<OSByte> host_pixels;
static unsigned host_pitch, presents;
static int host_fullscreen;
static double host_aspect;
int OSVideoSetMode(int w, int h, int bits, double aspect, int fullscreen)
{
    assert(bits == 32); host_pitch = w * 4;
    host_pixels.assign(host_pitch * h, 0);
    host_fullscreen = fullscreen; host_aspect = aspect;
    return 1;
}
OSByte *OSVideoPixels(unsigned *pitch)
{ if (pitch) *pitch = host_pitch; return host_pixels.empty() ? NULL : &host_pixels[0]; }
int OSVideoFullscreen(void) { return host_fullscreen; }
void OSVideoPresent(void) { ++presents; }
void OSVideoPalette(unsigned, unsigned, const OSByte *) {}
void OSVideoQuit(void) { host_pixels.clear(); }
void OSVideoDimensions(unsigned *w, unsigned *h, unsigned *sw, unsigned *sh)
{ if (w) *w = 640; if (h) *h = 480; if (sw) *sw = 1280; if (sh) *sh = 720; }
#else
static bool running = true;
static void Events(const OSHostEvent *event)
{
    if (event->type == OS_EVENT_QUIT) running = false;
    if (event->type == OS_EVENT_ACTION && event->key == OS_ACTION_FULLSCREEN)
        GFX_SwitchFullScreen();
}
#endif

static void Frame(unsigned bits, unsigned w, unsigned h)
{
    const unsigned bytes = (bits + 7) / 8;
    std::vector<uint8_t> line(w * bytes);
    assert(RENDER_StartUpdate());
    assert(!RENDER_StartUpdate());
    assert(vga.draw.must_complete_frame);
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            unsigned color = x * 3 / w;
            uint32_t value = 0;
            if (bits == 8) value = color;
            else if (bits == 15) value = color == 0 ? 0x7c00 : color == 1 ? 0x03e0 : 0x001f;
            else if (bits == 16) value = color == 0 ? 0xf800 : color == 1 ? 0x07e0 : 0x001f;
            else value = color == 0 ? 0xff0000 : color == 1 ? 0x00ff00 : 0x0000ff;
            memcpy(&line[x * bytes], &value, bytes);
        }
        RENDER_DrawLine(&line[0]);
    }
    RENDER_DrawLine(&line[0]); // extra scanlines must not overwrite the buffer
    assert(render.scale.inLine == h);
    RENDER_EndUpdate(false);
    unsigned pitch;
    const OSByte *pixels = OSVideoPixels(&pitch);
    for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
        unsigned color = x * 3 / w;
        const OSByte *p = pixels + y * pitch + x * 4;
        assert(p[0] == (color == 2 ? 255 : 0));
        assert(p[1] == (color == 1 ? 255 : 0));
        assert(p[2] == (color == 0 ? 255 : 0));
    }
}

int main(void)
{
#ifdef OS_RENDER_GUI_TEST
    if (!OSVideoInit(Events)) return 1;
#endif
    const unsigned formats[] = {8, 15, 16, 32};
    render.aspect = ASPECT_TRUE;
    RENDER_SetPal(0, 255, 0, 0);
    RENDER_SetPal(1, 0, 255, 0);
    RENDER_SetPal(2, 0, 0, 255);
    for (unsigned i = 0; i < 4; ++i) {
        unsigned bits = formats[i];
        RENDER_SetSize(321, 201, bits, 60, 4.0 / 3.0);
        assert(render.active && render.src.bpp == bits);
        Frame(bits, 321, 201);
#ifndef OS_RENDER_GUI_TEST
        assert(fabs(host_aspect - 4.0 / 3.0) < 0.00001);
        unsigned before = presents;
        unsigned pitch;
        const OSByte *pixels = OSVideoPixels(&pitch);
        std::vector<OSByte> snapshot(pixels, pixels + pitch * 201);
        std::vector<uint8_t> black(321 * 4, 0);
        assert(RENDER_StartUpdate()); RENDER_DrawLine(&black[0]); RENDER_EndUpdate(true);
        assert(presents == before && snapshot == host_pixels);
        assert(RENDER_StartUpdate()); RENDER_DrawLine(&black[0]); RENDER_EndUpdate(false);
        assert(presents == before && snapshot == host_pixels);
        render.frameskip.max = 2;
        assert(!RENDER_StartUpdate()); assert(!RENDER_StartUpdate());
        assert(RENDER_StartUpdate()); RENDER_EndUpdate(true);
        render.frameskip.max = 0;
        GFX_SwitchFullScreen(); assert(OSVideoFullscreen());
        Frame(bits, 321, 201);
        GFX_SwitchFullScreen(); assert(!OSVideoFullscreen());
#else
        OSVideoTitle("DOSBox-X renderer test (not the emulator)");
        unsigned start = OSHostTicks();
        while (running && OSHostTicks() - start < 3000) {
            OSVideoEvents(); Frame(bits, 321, 201); OSHostSleep(10);
        }
        if (!running) break;
        if (i == 1) GFX_SwitchFullScreen();
        if (i == 2) { GFX_SwitchFullScreen(); OSVideoBuffered(1); }
#endif
    }
#ifndef OS_RENDER_GUI_TEST
    extern int aspect_ratio_x, aspect_ratio_y;
    aspect_ratio_x = 16; aspect_ratio_y = 10;
    RENDER_SetSize(320, 200, 8, 60, 4.0 / 3.0);
    assert(fabs(host_aspect - 1.6) < 0.00001);
    aspect_ratio_x = aspect_ratio_y = -1;
    RENDER_SetSize(320, 200, 8, 60, 4.0 / 3.0);
    assert(fabs(host_aspect - 16.0 / 9.0) < 0.00001);
    aspect_ratio_x = aspect_ratio_y = 0;
    // A palette update alone must recolor an unchanged indexed image.
    RENDER_SetSize(321, 201, 8, 60, 4.0 / 3.0); Frame(8, 321, 201);
    RENDER_SetPal(0, 255, 255, 255);
    assert(RENDER_StartUpdate());
    for (unsigned y = 0; y < 201; ++y) RENDER_DrawLine(NULL);
    RENDER_EndUpdate(false);
    assert(host_pixels[0] == 255 && host_pixels[1] == 255 && host_pixels[2] == 255);
    RENDER_SetSize(0, 201, 8, 60, 4.0 / 3.0);
    assert(!render.active && !RENDER_StartUpdate());
    RENDER_SetSize(4097, 201, 32, 60, 4.0 / 3.0); assert(!render.active);
    RENDER_SetSize(321, 201, 24, 60, 4.0 / 3.0); assert(!render.active);
#endif
    RENDER_Shutdown(); GFX_TearDown();
    assert(!render.active && !scalerSourceCacheBuffer);
    puts("Native renderer: pixel formats, frame lifecycle and mode changes passed");
    return 0;
}
