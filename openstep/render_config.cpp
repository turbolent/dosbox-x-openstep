/* Renderer configuration is separate from scanline delivery for focused tests. */
#include "dosbox.h"
#include "control.h"
#include "render.h"
#include "vga.h"

int eurAscii = -1;
void RENDER_Reset(void);

extern int aspect_ratio_x, aspect_ratio_y;
void setAspectRatio(Section_prop *section)
{
    const char *text = section->Get_string("aspect_ratio");
    char *end = NULL;
    long x = strtol(text, &end, 10), y = 0;
    if (end && *end == ':') {
        const char *next = end + 1;
        y = strtol(next, &end, 10);
        if (end == next || *end) x = y = 0;
    } else x = y = 0;
    if (!((x > 0 && x <= 10000 && y > 0 && y <= 10000) || (x == -1 && y == -1))) x = y = 0;
    aspect_ratio_x = (int)x; aspect_ratio_y = (int)y;
}

static void RenderProperties(Section *section)
{
    Section_prop *p = static_cast<Section_prop *>(section);
    const std::string aspect = p->Get_string("aspect");
    render.aspect = (aspect == "true" || aspect == "1") ? ASPECT_TRUE : ASPECT_FALSE;
    setAspectRatio(p);
    int skip = p->Get_int("frameskip");
    render.frameskip.max = skip < 0 ? 0 : skip > 10 ? 10 : skip;
    render.frameskip.count = 0;
    vga.draw.doublescan_set = p->Get_bool("doublescan");
    vga.draw.char9_set = p->Get_bool("char9");
    eurAscii = p->Get_int("euro");
    if (eurAscii < 33 || eurAscii > 255) eurAscii = -1;
    vga.draw.monochrome_pal = Green;
    vga.draw.monochrome_bright = 1;
    Prop_multival *mono = p->Get_multival("monochrome_pal");
    if (mono) {
        const std::string color = mono->GetSection()->Get_string("color");
        const std::string bright = mono->GetSection()->Get_string("bright");
        if (color == "amber") vga.draw.monochrome_pal = Amber;
        else if (color == "gray" || color == "grey") vga.draw.monochrome_pal = Gray;
        else if (color == "white") vga.draw.monochrome_pal = White;
        if (bright == "bright") vga.draw.monochrome_bright = 0;
    }
    if (render.src.width && render.src.height) RENDER_Reset();
}

void RENDER_Init(void)
{
    Section *section = control->GetSection("render");
    if (!section) E_Exit("OPENSTEP: missing render configuration");
    RenderProperties(section);
    static bool registered;
    if (!registered) {
        section->onpropchange.push_back(RenderProperties);
        registered = true;
    }
}
