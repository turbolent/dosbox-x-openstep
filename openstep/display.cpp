/* DOSBox graphics API backed directly by AppKit/Interceptor. */
#include "dosbox.h"
#include "video.h"
#include "host.h"

/* VGA scanline generators always see canonical 0x00RRGGBB host pixels. */
uint32_t GFX_Rmask = 0x00ff0000, GFX_Gmask = 0x0000ff00;
uint32_t GFX_Bmask = 0x000000ff, GFX_Amask = 0;
unsigned char GFX_Rshift = 16, GFX_Gshift = 8, GFX_Bshift = 0;
unsigned char GFX_Ashift = 24, GFX_bpp = 32;

static Bitu source_width, source_height;
static double display_aspect = 4.0 / 3.0;
static GFX_CallBack_t redraw;
static bool running, updating, lazy_fullscreen;

unsigned int GFX_GetBShift() { return GFX_Bshift; }
Bitu GFX_GetRGB(uint8_t r, uint8_t g, uint8_t b)
{ return ((Bitu)r << 16) | ((Bitu)g << 8) | b; }

Bitu GFX_GetBestMode(Bitu flags)
{
    return (flags & GFX_CAN_32) ? GFX_CAN_32 | GFX_SCALING | GFX_CAN_RANDOM : 0;
}

Bitu GFX_SetSize(Bitu w, Bitu h, Bitu flags, double sx, double sy, GFX_CallBack_t cb)
{
    updating = running = false;
    if (!w || !h || w > 4096 || h > 4096 || !(sx > 0) || !(sy > 0) ||
        !(flags & GFX_CAN_32)) return 0;
    double aspect = double(w) * sx / (double(h) * sy);
    if (!OSVideoSetMode((int)w, (int)h, 32, aspect, OSVideoFullscreen())) return 0;
    source_width = w; source_height = h; display_aspect = aspect; redraw = cb;
    running = true;
    return GFX_GetBestMode(flags);
}

bool GFX_StartUpdate(uint8_t *&pixels, Bitu &pitch)
{
    unsigned host_pitch = 0;
    if (!running || updating) return false;
    pixels = OSVideoPixels(&host_pitch);
    pitch = host_pitch;
    updating = pixels != NULL;
    return updating;
}

void GFX_EndUpdate(const uint16_t *changed)
{
    if (updating && changed) OSVideoPresent();
    updating = false;
}

void GFX_SetPalette(Bitu start, Bitu count, GFX_PalEntry *entries)
{
    if (start <= 256 && count <= 256 - start)
        OSVideoPalette((unsigned)start, (unsigned)count, (const OSByte *)entries);
}

void GFX_GetSize(int &w, int &h, bool &fullscreen)
{ w = (int)source_width; h = (int)source_height; fullscreen = OSVideoFullscreen() != 0; }
bool GFX_IsFullscreen(void) { return OSVideoFullscreen() != 0; }
void GFX_Start(void) { running = source_width && source_height; }
void GFX_Stop(void) { updating = running = false; }

void GFX_ResetScreen(void)
{
    updating = false;
    if (redraw) redraw(GFX_CallBackReset);
}
void GFX_RestoreMode(void) { GFX_ResetScreen(); }

void GFX_SwitchFullscreenNoReset(void)
{
    updating = false;
    if (source_width && source_height && OSVideoSetMode((int)source_width,
        (int)source_height, 32, display_aspect, !OSVideoFullscreen())) {
        if (redraw) redraw(GFX_CallBackRedraw);
    }
}
void GFX_SwitchFullScreen(void) { GFX_SwitchFullscreenNoReset(); }
void GFX_SwitchLazyFullscreen(bool value) { lazy_fullscreen = value; }
bool GFX_LazyFullscreenRequested(void) { return lazy_fullscreen; }

void GFX_TearDown(void)
{
    if (redraw) redraw(GFX_CallBackStop);
    redraw = NULL; running = updating = false;
    source_width = source_height = 0;
    OSVideoQuit();
}
