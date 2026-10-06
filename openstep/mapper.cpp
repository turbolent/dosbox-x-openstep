/* Native host shortcuts and guest key events; GPL-2.0-or-later, see COPYING. */
#include "emulator.h"
#include "input.h"
#include "pic.h"
#include "logging.h"
#include "keymap.h"
#include <deque>

bool dotype = false, autofire = false, load = false;
int joy1axes[8] = {}, joy2axes[8] = {};
unsigned int mapper_keyboard_layout = DKM_US, host_keyboard_layout = DKM_US;
struct Handler {
    MAPPER_Handler *function;
    KBD_KEYS key;
    unsigned mods;
    std::string label, shortcut;
    Handler() : function(NULL), key(KBD_NONE), mods(0) {}
};
static std::map<std::string, Handler> handlers;
static std::map<int, std::string> held;
static std::deque<std::pair<std::string, bool> > pending;
struct NamedKey { const char *name; KBD_KEYS key; };
static const NamedKey keys[] = {
#include "key_names.inc"
};

static KBD_KEYS MapKey(MapKeys key)
{
    if (key >= MK_f1 && key <= MK_f12) return (KBD_KEYS)(KBD_f1 + key - MK_f1);
    if (key >= MK_a && key <= MK_z) return (KBD_KEYS)(KBD_a + key - MK_a);
    if (key >= MK_1 && key <= MK_9) return (KBD_KEYS)(KBD_1 + key - MK_1);
    switch (key) {
    case MK_0: return KBD_0;
    case MK_return: return KBD_enter; case MK_tab: return KBD_tab;
    case MK_space: return KBD_space; case MK_backspace: return KBD_backspace;
    case MK_kpminus: return KBD_kpminus; case MK_kpplus: return KBD_kpplus;
    case MK_scrolllock: return KBD_scrolllock; case MK_printscreen: return KBD_printscreen;
    case MK_pause: return KBD_pause; case MK_home: return KBD_home; case MK_end: return KBD_end;
    case MK_insert: return KBD_insert; case MK_delete: return KBD_delete;
    case MK_escape: return KBD_esc; case MK_uparrow: return KBD_up; case MK_downarrow: return KBD_down;
    case MK_leftarrow: return KBD_left; case MK_rightarrow: return KBD_right;
    case MK_pageup: return KBD_pageup; case MK_pagedown: return KBD_pagedown;
    case MK_slash: return KBD_slash; case MK_backslash: return KBD_backslash;
    case MK_minus: return KBD_minus; case MK_equals: return KBD_equals;
    case MK_lbracket: return KBD_leftbracket; case MK_rbracket: return KBD_rightbracket;
    case MK_comma: return KBD_comma; case MK_period: return KBD_period;
    case MK_semicolon: return KBD_semicolon; case MK_quote: return KBD_quote; case MK_grave: return KBD_grave;
    default: return KBD_NONE;
    }
}
static const char *KeyName(KBD_KEYS key)
{
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) if (keys[i].key == key) return keys[i].name;
    return "";
}
static KBD_KEYS FindKey(const std::string &name)
{
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) if (name == keys[i].name) return keys[i].key;
    return KBD_NONE;
}
void MAPPER_AddHandler(MAPPER_Handler *function, MapKeys key, Bitu mods,
    const char *event, const char *label, DOSBoxMenu::item **result)
{
    Handler &h = handlers[event];
    h.function = function; h.key = MapKey(key); h.label = label;
    h.mods = ((mods & MMOD1) ? OS_MOD_CONTROL : 0) | ((mods & MMOD2) ? OS_MOD_ALT : 0) |
             ((mods & MMOD3) ? OS_MOD_SHIFT : 0) | ((mods & MMODHOST) ? OS_MOD_COMMAND : 0);
    h.shortcut.clear();
    if (h.key != KBD_NONE) {
        if (h.mods & OS_MOD_COMMAND) h.shortcut += "Command+";
        if (h.mods & OS_MOD_CONTROL) h.shortcut += "Control+";
        if (h.mods & OS_MOD_ALT) h.shortcut += "Alternate+";
        if (h.mods & OS_MOD_SHIFT) h.shortcut += "Shift+";
        h.shortcut += KeyName(h.key);
    }
    const std::string name = std::string("mapper_") + event;
    DOSBoxMenu::item &item = mainMenu.item_exists(name) ? mainMenu.get_item(name) :
        mainMenu.alloc_item(DOSBoxMenu::item_type_id, name);
    item.set_text(label).set_mapper_event(event).set_shortcut_text(h.shortcut);
    if (result) *result = &item;
}
bool OSMapperKey(const OSHostEvent *event)
{
    if (event->pressed == 2) return false;
    std::map<int, std::string>::iterator down = held.find(event->scancode);
    if (down != held.end()) {
        if (!event->pressed) { pending.push_back(std::make_pair(down->second, false)); held.erase(down); }
        return true;
    }
    if (!event->pressed) return false;
    const KBD_KEYS key = OSInputTranslate(event->key, event->modifiers);
    unsigned mods = event->modifiers & (OS_MOD_COMMAND | OS_MOD_CONTROL | OS_MOD_ALT | OS_MOD_SHIFT);
    for (std::map<std::string, Handler>::const_iterator it = handlers.begin(); it != handlers.end(); ++it)
        if (it->second.key != KBD_NONE && it->second.key == key && it->second.mods == mods) {
            held[event->scancode] = it->first;
            pending.push_back(std::make_pair(it->first, true)); return true;
        }
    return false;
}
void OSMapperService(void)
{
    while (!pending.empty()) {
        std::pair<std::string, bool> event = pending.front(); pending.pop_front();
        std::map<std::string, Handler>::iterator it = handlers.find(event.first);
        if (it != handlers.end() && it->second.function) it->second.function(event.second);
    }
}
void OSMapperRelease(void)
{
    for (std::map<int, std::string>::const_iterator it = held.begin(); it != held.end(); ++it)
        pending.push_back(std::make_pair(it->second, false));
    held.clear();
}
void MAPPER_ReleaseAllKeys(void) { OSInputRelease(); OSMapperRelease(); }
void MAPPER_LosingFocus(void) { MAPPER_ReleaseAllKeys(); }
std::string mapper_event_keybind_string(const std::string &name)
{ std::map<std::string, Handler>::const_iterator it = handlers.find(name); return it == handlers.end() ? "" : it->second.shortcut; }
std::map<std::string, std::string> get_event_map(void)
{
    std::map<std::string, std::string> result;
    for (std::map<std::string, Handler>::const_iterator it = handlers.begin(); it != handlers.end(); ++it)
        result[it->first] = it->second.shortcut;
    return result;
}
void MAPPER_TriggerEventByName(const std::string &name)
{
    std::map<std::string, Handler>::iterator it = handlers.find(name);
    if (it != handlers.end() && it->second.function) { it->second.function(true); it->second.function(false); }
}
std::vector<std::string> MAPPER_GetEventNames(const std::string &prefix)
{
    std::vector<std::string> names;
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        std::string name = std::string("key_") + keys[i].name;
        size_t pos = name.find(prefix);
        if (pos != std::string::npos) names.push_back(name.substr(pos + prefix.size()));
    }
    return names;
}
void set_eventbutton_text(const char *event, const char *text)
{
    std::map<std::string, Handler>::iterator it = handlers.find(event);
    if (it != handlers.end()) {
        it->second.label = text;
        mainMenu.get_item(std::string("mapper_") + event).set_text(text);
    }
}
void update_bindbutton_text(void)
{
    for (std::map<std::string, Handler>::const_iterator it = handlers.begin(); it != handlers.end(); ++it)
        mainMenu.get_item(std::string("mapper_") + it->first).set_shortcut_text(it->second.shortcut);
}
void MAPPER_Init(void) { update_bindbutton_text(); }
void KeyboardLayoutDetect(void) { host_keyboard_layout = DKM_US; }
void setScanCode(Section_prop *) { MAPPER_ReleaseAllKeys(); }
void loadScanCode(void) { MAPPER_ReleaseAllKeys(); }
void MAPPER_Run(bool pressed)
{
    if (pressed) OSVideoAlert("Keyboard", "OPENSTEP uses the host keyboard layout and native Command shortcuts. The graphical binding editor is unavailable.", "OK", NULL, NULL);
}
void ReloadMapper(Section_prop *, bool) { MAPPER_Run(true); }

struct TypedKey { KBD_KEYS key; bool shift; TypedKey(KBD_KEYS k, bool s) : key(k), shift(s) {} };
static std::deque<TypedKey> typing;
static KBD_KEYS typed_down = KBD_NONE;
static bool typed_shift, typed_choice;
static unsigned typing_pace;
static void TypeEvent(Bitu)
{
    if (typed_down != KBD_NONE) {
        KEYBOARD_AddKey(typed_down, false);
        if (typed_shift) KEYBOARD_AddKey(KBD_leftshift, false);
        typed_down = KBD_NONE;
        if (!typing.empty()) PIC_AddEvent(TypeEvent, typing_pace);
        return;
    }
    if (typing.empty() || (typed_choice && !dotype)) { typing.clear(); return; }
    TypedKey key = typing.front(); typing.pop_front();
    if (key.key == KBD_NONE) { PIC_AddEvent(TypeEvent, typing_pace); return; }
    typed_down = key.key; typed_shift = key.shift;
    if (typed_shift) KEYBOARD_AddKey(KBD_leftshift, true);
    KEYBOARD_AddKey(typed_down, true); PIC_AddEvent(TypeEvent, 50);
}
void MAPPER_AutoType(std::vector<std::string> &sequence, unsigned wait, unsigned pace, bool choice)
{
    PIC_RemoveEvents(TypeEvent);
    if (typed_down != KBD_NONE) { KEYBOARD_AddKey(typed_down, false); if (typed_shift) KEYBOARD_AddKey(KBD_leftshift, false); }
    typed_down = KBD_NONE; typing.clear();
    for (size_t i = 0; i < sequence.size(); ++i) {
        std::string name = sequence[i];
        bool upper = name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z';
        if (upper) name[0] += 'a' - 'A';
        KBD_KEYS key = name == "," ? KBD_NONE : FindKey(name);
        if (key == KBD_NONE && name != ",") { LOG_MSG("AUTOTYPE: unknown key %s", name.c_str()); typing.clear(); return; }
        typing.push_back(TypedKey(key, upper));
    }
    typed_choice = choice; typing_pace = pace;
    if (!typing.empty()) PIC_AddEvent(TypeEvent, wait);
}
void OSMapperShutdown(void)
{
    PIC_RemoveEvents(TypeEvent); typing.clear();
    if (typed_down != KBD_NONE) { KEYBOARD_AddKey(typed_down, false); if (typed_shift) KEYBOARD_AddKey(KBD_leftshift, false); }
    typed_down = KBD_NONE; held.clear(); pending.clear(); handlers.clear();
}
