/* AppKit host controls for DOSBox-X. GPL-2.0-or-later, see COPYING. */
#include "emulator.h"
#include "host.h"
#include "input.h"
#include "video.h"
#include "render.h"
#include "mouse.h"
#include "cpu.h"
#include "cross.h"
#include "menudef.h"
#include "logging.h"
#include "../src/dos/drives.h"
#include <sys/stat.h>
#include <sstream>
#include <unistd.h>
#include "bios.h"

bool dos_kernel_disabled = true, guest_machine_power_on = false;
bool dos_shell_running_program = false, winrun = false, use_save_file = false;
bool startup_state_numlock = false, startup_state_capslock = false, startup_state_scrlock = false;
bool enableime = false, tonoime = false, tooutttf = false, usesystemcursor = false, kana_input = false;
bool clipboard_dosapi = false, clipboard_biospaste = false;
bool mountfro[26] = {}, mountiro[26] = {};
bool custom_bios = false, boot_debug_break = false, addovl = false, addne2k = false;
size_t custom_bios_image_size = 0;
Bitu custom_bios_image_offset = 0;
unsigned char *custom_bios_image = NULL;
std::vector<uint8_t> boot_code_image;
PhysPt boot_code_image_load_to = 0;
uint16_t boot_code_image_stack_ss = 0, boot_code_image_stack_sp = 0;
unsigned int maincp = 0, page = 0, hostkeyalt = 0, sendkeymap = 0;
Bitu frames = 0, time_limit_ms = 0;
double rtdelta = 0;
int wheel_key = 0, paste_speed = 20;
bool wheel_guest = false;
const char *modifier = "Command";
std::string configfile, working_dir, dosboxpath, savefilename, strPasteBuffer;
MOUSE_EMULATION user_cursor_emulation = MOUSE_EMULATION_ALWAYS;
bool user_cursor_locked = false;
int user_cursor_x = 0, user_cursor_y = 0, user_cursor_sw = 640, user_cursor_sh = 480;

static bool paused, auto_capture, capture_requested;
static int pending_signal = -1, pending_action = -1;
static OSUInt32 started_at;
static std::vector<std::string> restart_args;
bool OSMapperKey(const OSHostEvent *);
void OSMapperRelease(void);
void OSMapperService(void);
void MountHelper(char, const char *, const std::string &);
void MenuUnmountDrive(char);
void runImgmount(const char *);
void runImgmountISO(char, const std::vector<std::string> &, bool);
void MenuBrowseProgramFile(void);
void GUI_Run(bool);
void PasteClipboard(bool);
void GFX_SetTitle(int32_t, int, Bits, bool);

static void Pause(bool pressed)
{
    if (!pressed) return;
    paused = !paused; OSAudioPause(paused); OSInputRelease();
    if (paused) GFX_CaptureMouse(false);
    mainMenu.get_item("mapper_pause").check(paused).refresh_item(mainMenu);
    GFX_SetTitle(-1, -1, -1, paused);
}
static void Quit(bool pressed) { if (pressed) pending_signal = 0; }
static void Reset(bool pressed) { if (pressed) pending_signal = 3; }
static void Fullscreen(bool pressed) { if (pressed) GFX_SwitchFullScreen(); }
static void Capture(bool pressed) { if (pressed) GFX_CaptureMouse(!OSVideoCaptured()); }
static void QuickLaunch(bool pressed)
{ if (pressed && !dos_kernel_disabled && !control->SecureMode()) MenuBrowseProgramFile(); }
static void StopPaste(bool pressed) { if (pressed) strPasteBuffer.clear(); }

static void HostBindings(void)
{
    MAPPER_AddHandler(Quit, MK_f9, MMOD1, "shutdown", "Quit DOSBox-X");
    MAPPER_AddHandler(Capture, MK_f10, MMOD1, "capmouse", "Capture mouse");
    MAPPER_AddHandler(Fullscreen, MK_f, MMODHOST, "fullscr", "Toggle fullscreen");
    MAPPER_AddHandler(Pause, MK_pause, MMODHOST, "pause", "Pause emulation");
    MAPPER_AddHandler(Reset, MK_r, MMODHOST, "reset", "Reset DOSBox-X");
    MAPPER_AddHandler(QuickLaunch, MK_nothing, 0, "quickrun", "Launch DOS program");
    MAPPER_AddHandler(PasteClipboard, MK_f6, MMOD1, "paste", "Paste from clipboard");
    MAPPER_AddHandler(StopPaste, MK_nothing, 0, "pasteend", "Stop clipboard paste");
    MAPPER_AddHandler(GUI_Run, MK_c, MMODHOST, "gui", "Configuration");
}

static Section_prop *HostSection(void)
{ return static_cast<Section_prop *>(control->GetFrontendSection()); }

static void HostSettings(Section *section)
{
    Section_prop *p = static_cast<Section_prop *>(section);
    OSVideoBuffered(p->Get_bool("buffered"));
    OSInputSensitivity(p->Get_int("sensitivity"), p->Get_int("sensitivity"));
    auto_capture = p->Get_bool("autolock");
}

void OSFrontendConfigure(void)
{
    Section_prop *p = control->AddSection_prop("openstep", NULL, true);
    p->Add_bool("fullscreen", Property::Changeable::Always, false);
    p->Add_bool("buffered", Property::Changeable::Always, false);
    p->Add_bool("autolock", Property::Changeable::Always, true);
    p->Add_int("sensitivity", Property::Changeable::Always, 100)->SetMinMax(1, 1000);
    p->Add_bool("showdetails", Property::Changeable::Always, false);
    p->Add_string("output", Property::Changeable::OnlyAtStart, "interceptor");
    p->onpropchange.push_back(HostSettings);
    control->GetSection("voodoo")->HandleInputline("voodoo_card=false");
    control->GetSection("joystick")->HandleInputline("joysticktype=none");
    control->GetSection("midi")->HandleInputline("mididevice=none");
    control->GetSection("render")->HandleInputline("scaler=none");
    control->GetSection("mixer")->HandleInputline("rate=44100");
    control->GetSection("mixer")->HandleInputline("blocksize=1024");
}

static void Action(const OSHostEvent *event)
{
    if (event->type == OS_EVENT_QUIT) pending_signal = 0;
    else if (event->key == OS_ACTION_RESET) pending_signal = 3;
    else pending_action = event->key;
}

static void Event(const OSHostEvent *event)
{
    if (event->type == OS_EVENT_FOCUS && !event->pressed) OSMapperRelease();
    if (event->type == OS_EVENT_KEY && OSMapperKey(event)) return;
    if (paused && (event->type == OS_EVENT_KEY || event->type == OS_EVENT_BUTTON ||
                   event->type == OS_EVENT_MOTION)) return;
    if (event->type == OS_EVENT_BUTTON && event->pressed && capture_requested && auto_capture)
        GFX_CaptureMouse(true);
    OSInputEvent(event);
    user_cursor_locked = OSVideoCaptured() != 0;
}

void OSFrontendStart(void)
{
    HostBindings();
    if (static_cast<Section_prop *>(control->GetSection("imfc"))->Get_bool("imfc"))
        E_Exit("OPENSTEP does not support IBM Music Feature Card; set imfc=false");
    Section_prop *voodoo = static_cast<Section_prop *>(control->GetSection("voodoo"));
    if (strcmp(voodoo->Get_string("voodoo_card"), "false") || voodoo->Get_bool("glide"))
        E_Exit("OPENSTEP requires voodoo_card=false and glide=false");
    if (strcmp(HostSection()->Get_string("output"), "interceptor"))
        E_Exit("OPENSTEP output must be interceptor");
    if (!OSVideoInit(Event)) E_Exit("Cannot initialize AppKit");
    OSInputActions(Action);
    if (!OSVideoSetMode(640, 400, 32, 4.0 / 3.0,
        control->opt_fullscreen || HostSection()->Get_bool("fullscreen")))
        E_Exit("Cannot initialize Interceptor video");
    HostSettings(HostSection());
    menu.startup = true; menu.showrt = HostSection()->Get_bool("showdetails");
    menu.hidecycles = !menu.showrt;
    started_at = OSHostTicks();
    if (control->opt_time_limit > 0) time_limit_ms = (Bitu)(control->opt_time_limit * 1000);
}

bool systemmessagebox(const char *title, const char *message, const char *type,
                      const char *icon, int default_button)
{
    (void)icon; (void)default_button;
    bool choice = type && (!strcmp(type, "yesno") || !strcmp(type, "yesnocancel"));
    return OSVideoAlert(title, message, choice ? "Yes" : "OK", choice ? "No" : NULL,
        type && !strcmp(type, "yesnocancel") ? "Cancel" : NULL) == 1;
}

static int ChooseDrive(bool mounting)
{
    for (int offset = 0; offset < 25; ++offset) {
        const int drive = (offset + 2) % 25;
        if (mounting ? Drives[drive] != NULL : Drives[drive] == NULL) continue;
        char message[96], button[24];
        snprintf(message, sizeof(message), "%s drive %c:?", mounting ? "Mount on" : "Unmount", 'A' + drive);
        snprintf(button, sizeof(button), "%s %c:", mounting ? "Use" : "Unmount", 'A' + drive);
        int result = OSVideoAlert(mounting ? "Mount" : "Unmount", message, button, "Cancel", "Next Drive");
        if (result == 1) return drive;
        if (result == 0) return -1;
    }
    OSVideoAlert("Drives", mounting ? "No drive letter is available." : "No mounted drive was selected.", "OK", NULL, NULL);
    return -1;
}

static void DriveAction(int action)
{
    if (dos_kernel_disabled || control->SecureMode()) {
        OSVideoAlert("Drives", "Drive changes require the built-in DOS kernel and unrestricted mode.", "OK", NULL, NULL);
        return;
    }
    OSInputRelease(); OSMapperRelease();
    int drive = ChooseDrive(action == OS_ACTION_MOUNT);
    if (drive < 0) return;
    if (action == OS_ACTION_UNMOUNT) { MenuUnmountDrive('A' + drive); return; }
    const char *selected = OSVideoChoosePaths("Mount folder or disk image", 2, 0, 0, NULL);
    if (!selected) return;
    const std::string path(selected);
    struct stat st;
    if (stat(path.c_str(), &st)) { OSVideoAlert("Mount", "Cannot read the selected path.", "OK", NULL, NULL); return; }
    if (S_ISDIR(st.st_mode)) MountHelper('A' + drive, path.c_str(), "LOCAL");
    else {
        const size_t dot = path.find_last_of('.');
        const std::string ext = dot == std::string::npos ? "" : path.substr(dot);
        if (!strcasecmp(ext.c_str(), ".iso") || !strcasecmp(ext.c_str(), ".cue"))
            runImgmountISO('A' + drive, std::vector<std::string>(1, path), false);
        else if (path.find('"') == std::string::npos) {
            std::string command = std::string(1, 'A' + drive) + " \"" + path + "\"";
            runImgmount(command.c_str());
        } else OSVideoAlert("Mount", "This disk-image path contains a quote; rename it before mounting.", "OK", NULL, NULL);
    }
}

void GFX_Events(void)
{
    do {
        OSVideoEvents(); OSMapperService();
        if (!paused && !dos_kernel_disabled && !strPasteBuffer.empty()) {
            static OSUInt32 last_paste;
            const OSUInt32 now = OSHostTicks();
            if ((OSUInt32)(now - last_paste) >= (unsigned)(paste_speed > 0 ? paste_speed : 1) &&
                BIOS_AddKeyToBuffer((uint8_t)strPasteBuffer[0])) {
                strPasteBuffer.erase(0, 1); last_paste = now;
            }
        }
        if (time_limit_ms && (OSUInt32)(OSHostTicks() - started_at) >= time_limit_ms) pending_signal = 0;
        if (pending_signal >= 0) { int signal = pending_signal; pending_signal = -1; paused = false; OSAudioPause(0); throw signal; }
        if (pending_action >= 0) {
            int action = pending_action; pending_action = -1;
            if (action == OS_ACTION_PAUSE) Pause(true);
            if (action == OS_ACTION_MOUNT || action == OS_ACTION_UNMOUNT) DriveAction(action);
        }
        if (paused) { OSVideoPresent(); OSHostSleep(10); }
    } while (paused);
}

bool DOSBox_Paused(void) { return paused; }
void GFX_SetTitle(int32_t cycles, int skip, Bits timing, bool is_paused)
{
    (void)skip; (void)timing;
    extern std::string dosbox_title, RunningProgram;
    std::ostringstream title;
    title << "DOSBox-X";
    if (!dosbox_title.empty()) title << " - " << dosbox_title;
    if (!RunningProgram.empty()) title << " - " << RunningProgram;
    if (!menu.hidecycles) title << " (" << (cycles < 0 ? CPU_CycleMax : cycles) << " cycles/ms)";
    if (paused || is_paused) title << " - Paused";
    OSVideoTitle(title.str().c_str());
}
void GFX_ReleaseMouse(void) { GFX_CaptureMouse(false); }
bool MOUSE_IsLocked(void) { return OSVideoCaptured() != 0; }
bool Mouse_IsLocked(void) { return MOUSE_IsLocked(); }
void Mouse_AutoLock(bool requested) { capture_requested = requested; }
void CaptureMouseNotify(bool capture) { GFX_CaptureMouse(capture); }
void GFX_ForceRedrawScreen(void) { GFX_ResetScreen(); OSVideoPresent(); }
bool GFX_GetPreventFullscreen(void) { return false; }
void GetMaxWidthHeight(unsigned *w, unsigned *h) { OSVideoDimensions(NULL, NULL, w, h); }
void GetDrawWidthHeight(unsigned *w, unsigned *h) { OSVideoDimensions(w, h, NULL, NULL); }
void SetDisplayNumber(int display) { if (display) LOG_MSG("OPENSTEP uses the main display"); }
void SetWindowTransparency(int value) { if (value) LOG_MSG("OPENSTEP does not support window transparency"); }
bool TTF_using(void) { return false; }
std::string GetDefaultOutput(void) { return "interceptor"; }
bool toOutput(const char *name) { return name && !strcmp(name, "interceptor"); }
void change_output(int) { GFX_ResetScreen(); }
std::string GetDOSBoxXPath(bool executable)
{
    if (executable) return dosboxpath;
    const size_t slash = dosboxpath.find_last_of('/');
    return slash == std::string::npos ? working_dir + "/" : dosboxpath.substr(0, slash + 1);
}
bool Get_Custom_SaveDir(std::string &path) { path = Cross::CreatePlatformConfigDir() + "saves"; mkdir(path.c_str(), 0700); return true; }
void UpdateKeyboardLEDState(Bitu) { /* AppKit cannot set physical keyboard LEDs. Guest state lives in keyboard.cpp. */ }
void OSFrontendShutdown(void) { OSVideoQuit(); }

void refreshExtChar(void)
{
    /* This menu state only applies to the optional TrueType renderer. */
    if (mainMenu.item_exists("ttf_extcharset"))
        mainMenu.get_item("ttf_extcharset").enable(false).refresh_item(mainMenu);
}

void GUI_Run(bool pressed)
{
    if (pressed) return;
    OSVideoAlert("Configuration", "Use CONFIG at the DOS prompt to inspect and change settings. "
        "Use CONFIG -writeconf to save them. The graphical configuration editor is unavailable on OPENSTEP.",
        "OK", NULL, NULL);
}
void GUI_Shortcut(int selection) { if (selection) GUI_Run(false); }

void RebootConfig(std::string filename, bool confirm)
{
    if (control->SecureMode()) return;
    if (confirm && OSVideoAlert("Restart", "Restart with the selected configuration?",
        "Restart", "Cancel", NULL) != 1) return;
    if (access(filename.c_str(), R_OK)) {
        OSVideoAlert("Restart", "The configuration file cannot be read.", "OK", NULL, NULL);
        return;
    }
    restart_args.clear(); restart_args.push_back(dosboxpath);
    restart_args.push_back("-conf"); restart_args.push_back(filename);
    throw 0;
}

void RebootLanguage(std::string filename, bool confirm)
{
    if (control->SecureMode()) return;
    if (confirm && OSVideoAlert("Restart", "Restart to apply the language settings?",
        "Restart", "Cancel", NULL) != 1) return;
    std::string name = Cross::CreatePlatformConfigDir() + "restart-XXXXXX";
    std::vector<char> temp(name.begin(), name.end()); temp.push_back(0);
    int fd = mkstemp(&temp[0]);
    if (fd < 0) { OSVideoAlert("Restart", "Cannot create a temporary configuration.", "OK", NULL, NULL); return; }
    close(fd);
    if (!control->PrintConfig(&temp[0], false, true)) {
        remove(&temp[0]); OSVideoAlert("Restart", "Cannot save the configuration.", "OK", NULL, NULL); return;
    }
    restart_args.clear(); restart_args.push_back(dosboxpath);
    restart_args.push_back("-conf"); restart_args.push_back(&temp[0]);
    restart_args.push_back("-eraseconf");
    if (!filename.empty()) { restart_args.push_back("-lang"); restart_args.push_back(filename); }
    throw 0;
}

int OSFrontendRestart(void)
{
    if (restart_args.empty()) return 0;
    std::vector<char *> args;
    for (size_t i = 0; i < restart_args.size(); ++i)
        args.push_back(const_cast<char *>(restart_args[i].c_str()));
    args.push_back(NULL);
#if defined(NeXT)
    execv(args[0], const_cast<const char **>(&args[0]));
#else
    execv(args[0], &args[0]);
#endif
    perror("DOSBox-X restart"); return 1;
}

/* Save only native mouse policy; never persist AppKit or Interceptor pointers. */
void POD_Save_Sdlmain(std::ostream &stream)
{ stream.write((const char *)&auto_capture, sizeof(auto_capture)); stream.write((const char *)&capture_requested, sizeof(capture_requested)); }
void POD_Load_Sdlmain(std::istream &stream)
{ OSInputRelease(); stream.read((char *)&auto_capture, sizeof(auto_capture)); stream.read((char *)&capture_requested, sizeof(capture_requested)); }
