/* Native scanline renderer. VGA produces guest pixels; Interceptor handles
 * presentation and aspect scaling. No SDL or upstream software scalers. */
#include "dosbox.h"
#include "render.h"
#include "vga.h"
#include "pixels.h"
#include "host.h"
#include <stdlib.h>
#include <string.h>

Render_t render;
int aspect_ratio_x = 0, aspect_ratio_y = 0;
Bitu last_gfx_flags;
uint32_t GFX_palette32bpp[256];
bool useTraditionalRenderCache = false;
unsigned char *scalerSourceCacheBuffer;
unsigned int scalerSourceCacheBufferSize;

static void EmptyLine(const void *) {}
ScalerLineHandler_t RENDER_DrawLine = EmptyLine;

void scalerSourceCacheBufferFree(void)
{
    free(scalerSourceCacheBuffer);
    scalerSourceCacheBuffer = NULL;
    scalerSourceCacheBufferSize = 0;
    render.cache.pointer = NULL;
}

bool scalerSourceCacheBufferAlloc(unsigned int pitch, unsigned int height)
{
    if (scalerSourceCacheBuffer || !pitch || !height || pitch > 16384 || height > 4096)
        return false;
    unsigned size = pitch * height;
    scalerSourceCacheBuffer = (unsigned char *)calloc(size, 1);
    if (!scalerSourceCacheBuffer) return false;
    scalerSourceCacheBufferSize = size;
    render.cache.pointer = scalerSourceCacheBuffer;
    return true;
}

static void DrawLine(const void *source)
{
    if (!render.updating || render.scale.inLine >= render.src.height) return;
    /* A null line keeps the previous scanline, as in the scaler interface. */
    if (source) memcpy(scalerSourceCacheBuffer + render.scale.inLine *
        render.scale.cachePitch, source, render.scale.cachePitch);
    ++render.scale.inLine;
    render.scale.outLine = render.scale.inLine;
}

void RENDER_SetPal(uint8_t entry, uint8_t red, uint8_t green, uint8_t blue)
{
    render.pal.rgb[entry].red = red;
    render.pal.rgb[entry].green = green;
    render.pal.rgb[entry].blue = blue;
    GFX_palette32bpp[entry] = ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
    render.pal.changed = true;
}

void RENDER_EndUpdate(bool abort)
{
    if (!render.updating) return;
    RENDER_DrawLine = EmptyLine;
    /* Never expose an interrupted frame or a partly rendered mode switch. */
    if (!abort && !render.disablerender && render.scale.inLine == render.src.height) {
        uint8_t *pixels = NULL;
        Bitu pitch = 0;
        if (GFX_StartUpdate(pixels, pitch)) {
            OSPixelBlit(pixels, (unsigned)pitch, (int)render.src.width,
                (int)render.src.height, OS_PIXEL_BGRX, scalerSourceCacheBuffer,
                (unsigned)render.scale.cachePitch, (int)render.src.width,
                (int)render.src.height, (int)render.src.bpp,
                (const OSByte *)render.pal.rgb);
            const uint16_t changed[2] = {0, (uint16_t)render.src.height};
            GFX_EndUpdate(changed);
            render.scale.clearCache = false;
            render.pal.changed = false;
        }
    }
    render.updating = false;
    render.frameskip.index = (render.frameskip.index + 1) & (RENDER_SKIP_CACHE - 1);
}

bool RENDER_StartUpdate(void)
{
    if (render.updating || !render.active || render.disablerender || !scalerSourceCacheBuffer)
        return false;
    if (render.frameskip.count < render.frameskip.max) {
        ++render.frameskip.count;
        return false;
    }
    render.frameskip.count = 0;
    render.scale.inLine = render.scale.outLine = 0;
    render.updating = render.fullFrame = true;
    /* This backend copies complete frames and does not use the SDL scaler's
     * dirty-line cache. Ask VGA's on-demand path to finish every accepted frame. */
    vga.draw.must_complete_frame = true;
    RENDER_DrawLine = DrawLine;
    return true;
}

void RENDER_Reset(void);
void RENDER_CallBack(GFX_CallBackFunctions_t function)
{
    if (function == GFX_CallBackReset) RENDER_Reset();
    else if (function == GFX_CallBackStop) {
        RENDER_EndUpdate(true);
        render.active = false;
        RENDER_DrawLine = EmptyLine;
    } else if (function == GFX_CallBackRedraw) {
        render.scale.clearCache = true;
        vga.draw.must_complete_frame = true;
    }
}

void RENDER_Reset(void)
{
    RENDER_EndUpdate(true);
    RENDER_DrawLine = EmptyLine;
    render.active = false;
    scalerSourceCacheBufferFree();
    TempLineFree();
    if (!render.src.width || !render.src.height) return;
    render.scale.cachePitch = render.src.width * ((render.src.bpp + 7) / 8);
    if (!scalerSourceCacheBufferAlloc((unsigned)render.scale.cachePitch,
        (unsigned)render.src.height) || !TempLineAlloc((unsigned)render.src.width)) {
        scalerSourceCacheBufferFree();
        E_Exit("OPENSTEP: cannot allocate VGA scanline buffers");
        return;
    }
    double sy = render.aspect ? render.src.ratio : 1.0;
    if (render.aspect && aspect_ratio_x > 0 && aspect_ratio_y > 0)
        sy = double(render.src.width) * aspect_ratio_y /
             (double(render.src.height) * aspect_ratio_x);
    else if (render.aspect && aspect_ratio_x == -1) {
        unsigned w = 0, h = 0;
        OSVideoDimensions(NULL, NULL, &w, &h);
        if (w && h) sy = double(render.src.width) * h / (double(render.src.height) * w);
    }
    last_gfx_flags = GFX_SetSize(render.src.width, render.src.height,
        GFX_CAN_32, 1.0, sy, RENDER_CallBack);
    if (!last_gfx_flags) {
        E_Exit("OPENSTEP: cannot create Interceptor display");
        return;
    }
    render.scale.inMode = render.src.bpp == 8 ? scalerMode8 :
        render.src.bpp == 15 ? scalerMode15 : render.src.bpp == 16 ? scalerMode16 : scalerMode32;
    render.scale.outMode = scalerMode32;
    render.scale.inHeight = render.src.height;
    render.scale.size = 1;
    render.scale.op = scalerOpNormal;
    render.scale.clearCache = true;
    render.cache.width = render.src.width; render.cache.height = render.src.height;
    render.active = true;
    vga.draw.must_complete_frame = true;
}

void RENDER_SetSize(Bitu width, Bitu height, Bitu bpp, float fps, double screen_ratio)
{
    RENDER_EndUpdate(true);
    render.active = false;
    RENDER_DrawLine = EmptyLine;
    if (!width || !height || width > 4096 || height > 4096 ||
        (bpp != 8 && bpp != 15 && bpp != 16 && bpp != 32) ||
        !(screen_ratio > 0.01 && screen_ratio < 100)) return;
    render.src.width = width; render.src.height = height; render.src.bpp = bpp;
    render.src.fps = fps; render.src.scrn_ratio = screen_ratio;
    render.src.ratio = (double(width) / double(height)) / screen_ratio;
    render.src.dblw = render.src.dblh = false;
    RENDER_Reset();
}

bool RENDER_GetForceUpdate(void) { return render.forceUpdate; }
void RENDER_SetForceUpdate(bool force) { render.forceUpdate = force; }
bool RENDER_IsScalerCompatibleWithDoublescan(void) { return true; }

void RENDER_Shutdown(void)
{
    RENDER_EndUpdate(true);
    render.active = false;
    RENDER_DrawLine = EmptyLine;
    scalerSourceCacheBufferFree();
    TempLineFree();
}
