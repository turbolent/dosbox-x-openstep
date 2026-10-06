#include "dosbox.h"
#include "input.h"
#include "video.h"
#include <vector>
#include <utility>

static std::vector<std::pair<KBD_KEYS, bool> > keys;
static unsigned mouse_down, mouse_up, fullscreen, actions;
static int captured;
static float mx, my, mdx, mdy;
static bool relative;
void KEYBOARD_AddKey(KBD_KEYS key, bool down) { keys.push_back(std::make_pair(key, down)); }
void Mouse_ButtonPressed(uint8_t button) { mouse_down |= 1u << button; }
void Mouse_ButtonReleased(uint8_t button) { mouse_up |= 1u << button; }
void Mouse_CursorMoved(float dx, float dy, float x, float y, bool emulate)
{ mdx = dx; mdy = dy; mx = x; my = y; relative = emulate; }
void OSVideoCapture(int value) { captured = value; }
int OSVideoCaptured(void) { return captured; }
int OSVideoMapPoint(int x, int y, float *gx, float *gy)
{ *gx = (float)x / 639; *gy = (float)y / 479; return 1; }
void GFX_SwitchFullScreen(void) { ++fullscreen; }
static void Action(const OSHostEvent *) { ++actions; }
static OSHostEvent Event(int type, int key, int pressed)
{
    OSHostEvent e; memset(&e, 0, sizeof(e));
    e.type = type; e.key = key; e.pressed = pressed; return e;
}
int main(void)
{
    assert(OSInputTranslate('A', 0) == KBD_a);
    assert(OSInputTranslate('!', OS_MOD_SHIFT) == KBD_1);
    assert(OSInputTranslate(0xf704, 0) == KBD_f1);
    assert(OSInputTranslate(0xf70f, 0) == KBD_f12);
    assert(OSInputTranslate(0xf728, 0) == KBD_delete);
    assert(OSInputTranslate(127, 0) == KBD_backspace);
    assert(OSInputTranslate('2', OS_MOD_KEYPAD) == KBD_kp2);
    assert(OSInputTranslate(3, 0) == KBD_kpenter);
    assert(OSInputTranslate(0x2603, 0) == KBD_NONE);
    OSHostEvent e = Event(OS_EVENT_KEY, 'a', 1); e.scancode = 20;
    OSInputEvent(&e); OSInputEvent(&e);
    assert(keys.size() == 1 && keys.back() == std::make_pair(KBD_a, true));
    // Release by physical key even if the character/layout changed while held.
    e.key = 'b'; e.pressed = 0; OSInputEvent(&e);
    assert(keys.size() == 2 && keys.back() == std::make_pair(KBD_a, false));
    e = Event(OS_EVENT_KEY, 'z', 1); e.modifiers = OS_MOD_SHIFT | OS_MOD_CONTROL;
    OSInputEvent(&e);
    e = Event(OS_EVENT_BUTTON, 0, 1); OSInputEvent(&e);
    GFX_CaptureMouse(true);
    e = Event(OS_EVENT_MOTION, 0, 0); e.dx = -5; e.dy = 8; OSInputEvent(&e);
    assert(relative && mdx == -5 && mdy == 8);
    OSInputSensitivity(200, -50); OSInputEvent(&e);
    assert(relative && mdx == -10 && mdy == -4);
    OSInputSensitivity(100, 100);
    e = Event(OS_EVENT_FOCUS, 0, 0); OSInputEvent(&e);
    assert(!captured && !mouselocked && mouse_down == 1 && mouse_up == 1);
    assert(keys.back() == std::make_pair(KBD_leftctrl, false));
    size_t before = keys.size();
    e = Event(OS_EVENT_KEY, 'q', 1); OSInputEvent(&e); assert(keys.size() == before);
    e = Event(OS_EVENT_FOCUS, 0, 1); OSInputEvent(&e);
    e = Event(OS_EVENT_KEY, 0, 2); e.modifiers = OS_MOD_CAPS; OSInputEvent(&e);
    assert(keys[keys.size()-2] == std::make_pair(KBD_capslock, true));
    assert(keys.back() == std::make_pair(KBD_capslock, false));
    before = keys.size(); OSInputRelease(); assert(keys.size() == before);
    e = Event(OS_EVENT_MOTION, 0, 0); e.x = 639; e.y = 479; OSInputEvent(&e);
    assert(!relative && mx == 1 && my == 1);
    e.x = 629; e.y = 459; OSInputEvent(&e); assert(mdx == -10 && mdy == -20);
    OSInputSensitivity(50, 200);
    e.x = 619; e.y = 449; OSInputEvent(&e);
    assert(mdx == -5 && mdy == -20 && mx == 619.0f/639 && my == 449.0f/479);
    OSInputSensitivity(100, 100);
    e = Event(OS_EVENT_ACTION, OS_ACTION_FULLSCREEN, 1); OSInputEvent(&e); assert(fullscreen == 1);
    OSInputActions(Action);
    e = Event(OS_EVENT_ACTION, OS_ACTION_PAUSE, 1); OSInputEvent(&e); assert(actions == 1);
    e = Event(OS_EVENT_QUIT, 0, 0); OSInputEvent(&e); assert(actions == 2);
    puts("OPENSTEP input: key mapping, repeats, focus release and mouse routing passed");
    return 0;
}
