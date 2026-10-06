/* AppKit input translated directly into the emulated keyboard and mouse. */
#include "dosbox.h"
#include "input.h"
#include "mouse.h"
#include "video.h"
#include <string.h>

bool mouselocked = false;
static KBD_KEYS physical_keys[512];
static unsigned key_users[KBD_LAST], modifiers;
static bool buttons[3], last_mouse_valid, focused = true;
static int last_mouse_x, last_mouse_y;
static OSInputActionHandler action_handler;
static float sensitivity_x = 1.0f, sensitivity_y = 1.0f;

void OSInputSensitivity(int x_percent, int y_percent)
{
    sensitivity_x = x_percent / 100.0f;
    sensitivity_y = y_percent / 100.0f;
}

KBD_KEYS OSInputTranslate(int ch, unsigned mods)
{
    static const KBD_KEYS letters[] = {KBD_a,KBD_b,KBD_c,KBD_d,KBD_e,KBD_f,
        KBD_g,KBD_h,KBD_i,KBD_j,KBD_k,KBD_l,KBD_m,KBD_n,KBD_o,KBD_p,
        KBD_q,KBD_r,KBD_s,KBD_t,KBD_u,KBD_v,KBD_w,KBD_x,KBD_y,KBD_z};
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    if (ch >= 'a' && ch <= 'z') return letters[ch - 'a'];
    if (ch >= 0xf704 && ch <= 0xf70f) return (KBD_KEYS)(KBD_f1 + ch - 0xf704);
    if (ch >= 0xf710 && ch <= 0xf71b) return (KBD_KEYS)(KBD_f13 + ch - 0xf710);
    if (mods & OS_MOD_KEYPAD) {
        if (ch >= '1' && ch <= '9') return (KBD_KEYS)(KBD_kp1 + ch - '1');
        switch (ch) {
        case '0': return KBD_kp0; case '.': case ',': return KBD_kpperiod;
        case '/': return KBD_kpdivide; case '*': return KBD_kpmultiply;
        case '-': return KBD_kpminus; case '+': return KBD_kpplus;
        case '=': return KBD_kpequals; case '\r': case 3: return KBD_kpenter;
        }
    }
    if (ch >= '1' && ch <= '9') return (KBD_KEYS)(KBD_1 + ch - '1');
    switch (ch) {
    case '0': case ')': return KBD_0;
    case '!': return KBD_1; case '@': return KBD_2; case '#': return KBD_3;
    case '$': return KBD_4; case '%': return KBD_5; case '^': return KBD_6;
    case '&': return KBD_7; case '*': return KBD_8; case '(': return KBD_9;
    case 27: return KBD_esc; case '\t': case 0x19: return KBD_tab;
    case 8: case 127: return KBD_backspace;
    case '\r': case '\n': return KBD_enter; case 3: return KBD_kpenter;
    case ' ': return KBD_space;
    case '`': case '~': return KBD_grave; case '-': case '_': return KBD_minus;
    case '=': case '+': return KBD_equals; case '\\': case '|': return KBD_backslash;
    case '[': case '{': return KBD_leftbracket; case ']': case '}': return KBD_rightbracket;
    case ';': case ':': return KBD_semicolon; case '\'': case '"': return KBD_quote;
    case ',': case '<': return KBD_comma; case '.': case '>': return KBD_period;
    case '/': case '?': return KBD_slash;
    case 0xf700: return KBD_up; case 0xf701: return KBD_down;
    case 0xf702: return KBD_left; case 0xf703: return KBD_right;
    case 0xf727: return KBD_insert; case 0xf728: return KBD_delete;
    case 0xf729: return KBD_home; case 0xf72b: return KBD_end;
    case 0xf72c: return KBD_pageup; case 0xf72d: return KBD_pagedown;
    case 0xf72e: return KBD_printscreen; case 0xf72f: return KBD_scrolllock;
    case 0xf730: return KBD_pause; case 0xf739: return KBD_numlock;
    }
    return KBD_NONE;
}

static void Modifiers(unsigned flags)
{
    const unsigned masks[] = {OS_MOD_SHIFT, OS_MOD_CONTROL, OS_MOD_ALT};
    const KBD_KEYS keys[] = {KBD_leftshift, KBD_leftctrl, KBD_leftalt};
    for (unsigned i = 0; i < 3; ++i)
        if ((modifiers ^ flags) & masks[i]) KEYBOARD_AddKey(keys[i], (flags & masks[i]) != 0);
    if ((modifiers ^ flags) & OS_MOD_CAPS) {
        KEYBOARD_AddKey(KBD_capslock, true); KEYBOARD_AddKey(KBD_capslock, false);
    }
    modifiers = flags;
}

void OSInputRelease(void)
{
    for (unsigned i = 1; i < KBD_LAST; ++i)
        if (key_users[i]) KEYBOARD_AddKey((KBD_KEYS)i, false);
    memset(physical_keys, 0, sizeof(physical_keys));
    memset(key_users, 0, sizeof(key_users));
    Modifiers(modifiers & OS_MOD_CAPS);
    for (unsigned i = 0; i < 3; ++i) if (buttons[i]) {
        Mouse_ButtonReleased((uint8_t)i); buttons[i] = false;
    }
    last_mouse_valid = false;
}

void GFX_CaptureMouse(bool capture)
{
    OSVideoCapture(capture);
    mouselocked = OSVideoCaptured() != 0;
    last_mouse_valid = false;
}
void GFX_CaptureMouse(void) { GFX_CaptureMouse(!OSVideoCaptured()); }
void GFX_LosingFocus(void) { OSInputRelease(); GFX_CaptureMouse(false); }
void OSInputActions(OSInputActionHandler handler) { action_handler = handler; }

void OSInputEvent(const OSHostEvent *event)
{
    if (!event) return;
    if (!focused && (event->type == OS_EVENT_KEY || event->type == OS_EVENT_BUTTON ||
        event->type == OS_EVENT_MOTION)) return;
    mouselocked = OSVideoCaptured() != 0;
    switch (event->type) {
    case OS_EVENT_KEY: {
        Modifiers(event->modifiers);
        if (event->pressed == 2) break; // flagsChanged has no character
        if (event->scancode < 0 || event->scancode >= 512) break;
        KBD_KEYS &held = physical_keys[event->scancode];
        if (event->pressed) {
            if (held != KBD_NONE) break; // guest owns typematic repeat
            held = OSInputTranslate(event->key, event->modifiers);
            if (held != KBD_NONE && ++key_users[held] == 1) KEYBOARD_AddKey(held, true);
        } else if (held != KBD_NONE) {
            if (--key_users[held] == 0) KEYBOARD_AddKey(held, false);
            held = KBD_NONE;
        }
        break;
    }
    case OS_EVENT_FOCUS:
        focused = event->pressed != 0;
        if (!event->pressed) GFX_LosingFocus();
        break;
    case OS_EVENT_MOTION: {
        float x = 0, y = 0, dx, dy;
        if (mouselocked) { dx = (float)event->dx; dy = (float)event->dy; }
        else {
            OSVideoMapPoint(event->x, event->y, &x, &y);
            dx = last_mouse_valid ? (float)(event->x - last_mouse_x) : 0;
            dy = last_mouse_valid ? (float)(event->y - last_mouse_y) : 0;
            last_mouse_x = event->x; last_mouse_y = event->y; last_mouse_valid = true;
        }
        Mouse_CursorMoved(dx * sensitivity_x, dy * sensitivity_y, x, y, mouselocked);
        break;
    }
    case OS_EVENT_BUTTON:
        if (event->key >= 0 && event->key < 3 && buttons[event->key] != !!event->pressed) {
            buttons[event->key] = !!event->pressed;
            if (event->pressed) Mouse_ButtonPressed((uint8_t)event->key);
            else Mouse_ButtonReleased((uint8_t)event->key);
        }
        break;
    case OS_EVENT_ACTION:
        if (event->key == OS_ACTION_FULLSCREEN) { OSInputRelease(); GFX_SwitchFullScreen(); }
        else if (event->key == OS_ACTION_CAPTURE) GFX_CaptureMouse();
        else if (action_handler) action_handler(event);
        break;
    case OS_EVENT_QUIT:
        OSInputRelease();
        if (action_handler) action_handler(event);
        break;
    }
}
