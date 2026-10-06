/* Native AppKit/Interceptor frontend, based on the OPENSTEP Quake II port.
 * Quake II: Copyright (C) 1997-2001 Id Software, Inc., GPL-2.0-or-later. */
#import <AppKit/AppKit.h>
#import "InterceptorPrivate.h"
#include "host.h"
#include "pixels.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>

extern void PSsetmouse(float, float);
extern void PScurrentmouse(int, float *, float *);
extern void PShidecursor(void);
extern void PSshowcursor(void);
extern void PSWait(void);

@interface DOSBoxWindow : NSWindow
@end
@implementation DOSBoxWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface DOSBoxRunner : NSObject
{
@public
    void (*entry)(void *);
    void *userdata;
}
- (void)run:(NSTimer *)timer;
@end
@implementation DOSBoxRunner
- (void)run:(NSTimer *)timer
{
    (void)timer;
    entry(userdata);
    [NSApp stop:nil];
    [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
        location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0 windowNumber:0
        context:[NSDPSContext currentContext] subtype:0 data1:0 data2:0] atStart:NO];
}
@end

void OSApplicationRun(void (*entry)(void *), void *userdata)
{
    DOSBoxRunner *runner = [[DOSBoxRunner alloc] init];
    runner->entry = entry; runner->userdata = userdata;
    /* OPENSTEP stopModalWithCode: unwinds only nested run levels. Keep a real
     * outer application loop while the emulator pumps events on this thread. */
    [NSTimer scheduledTimerWithTimeInterval:0 target:runner selector:@selector(run:)
                                  userInfo:nil repeats:NO];
    [NSApp run];
    [runner release];
}

@interface DOSBoxView : NSView
- (void)command:(id)sender;
@end

static NSAutoreleasePool *root_pool;
static DOSBoxWindow *window;
static DOSBoxView *view;
static NSMenu *application_menu;
static NSDirectBitmap *bitmap;
static NSFramebuffer *screen;
static OSByte *pixels, palette[1024];
static unsigned pitch;
static int width, height, bits, dirty, buffered, captured, cursor_hidden;
static int guest_cursor_visible = 1;
static unsigned lock_misses;
static double aspect;
static NSPoint mouse_origin;
static OSEventHandler event_handler;
static OSHostEvent events[512];
static unsigned event_count;
static volatile sig_atomic_t quit_signal;
static char *chosen_paths;

OSWord *OSClipboardRead(void)
{
    NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
    unsigned length = [text length];
    OSWord *copy;
    if (!text || length > 1024 * 1024) return NULL;
    copy = (OSWord *)malloc((length + 1) * sizeof(OSWord));
    if (!copy) return NULL;
    [text getCharacters:copy]; copy[length] = 0;
    return copy;
}

static void SignalQuit(int sig) { (void)sig; quit_signal = 1; }

static void Queue(OSHostEvent event)
{
    if (event_count == 512) {
        unsigned i;
        for (i = 0; i < event_count; i++) free((void *)events[i].path);
        memset(events, 0, sizeof(events));
        /* Release held keys in the emulator before accepting more input. */
        events[0].type = OS_EVENT_FOCUS;
        events[1].type = OS_EVENT_FOCUS; events[1].pressed = OSVideoActive();
        event_count = 2;
    }
    events[event_count++] = event;
}

static void Action(int action)
{
    OSHostEvent event;
    memset(&event, 0, sizeof(event));
    event.type = OS_EVENT_ACTION; event.key = action;
    Queue(event);
}

int OSVideoActive(void)
{
    return window && [NSApp isActive] && [window isKeyWindow] &&
        [window isVisible] && ![window isMiniaturized];
}

static NSPoint MousePosition(void);

static void Cursor(void)
{
    int hide = (captured || screen) && OSVideoActive();
    if (!hide && !guest_cursor_visible && OSVideoActive()) {
        NSPoint point = MousePosition();
        NSRect bounds = [view bounds];
        hide = point.x >= 0 && point.y >= 0 &&
            point.x < bounds.size.width && point.y < bounds.size.height;
    }
    if (hide && !cursor_hidden) { PShidecursor(); cursor_hidden = 1; PSWait(); }
    if (!hide && cursor_hidden) { PSshowcursor(); cursor_hidden = 0; PSWait(); }
}

static NSPoint MousePosition(void)
{
    NSPoint point;
    PScurrentmouse([window windowNumber], &point.x, &point.y);
    return [view convertPoint:point fromView:nil];
}

static void CenterMouse(void)
{
    NSRect bounds = [view bounds];
    [view lockFocus];
    PSsetmouse(bounds.size.width / 2, bounds.size.height / 2);
    mouse_origin = MousePosition();
    [view unlockFocus];
}

void OSVideoCapture(int capture)
{
    captured = capture && OSVideoActive();
    if (captured) CenterMouse();
    Cursor();
}

int OSVideoCaptured(void) { return captured && OSVideoActive(); }
int OSVideoScreenCount(void) { return [[NSScreen screens] count]; }

void OSVideoShowCursor(int visible)
{
    guest_cursor_visible = !!visible;
    Cursor();
}

int OSVideoMapPoint(int x, int y, float *guest_x, float *guest_y)
{
    int vx, vy, vw, vh;
    NSRect bounds;
    if (!view || !guest_x || !guest_y) return 0;
    bounds = [view bounds];
    if (!OSPixelViewport((int)bounds.size.width, (int)bounds.size.height,
        aspect, &vx, &vy, &vw, &vh)) return 0;
    *guest_x = vw > 1 ? (float)(x - vx) / (vw - 1) : 0;
    *guest_y = vh > 1 ? (float)(y - vy) / (vh - 1) : 0;
    if (*guest_x < 0) *guest_x = 0; if (*guest_x > 1) *guest_x = 1;
    if (*guest_y < 0) *guest_y = 0; if (*guest_y > 1) *guest_y = 1;
    return x >= vx && y >= vy && x < vx + vw && y < vy + vh;
}

static void Focus(int active)
{
    OSHostEvent event;
    memset(&event, 0, sizeof(event));
    event.type = OS_EVENT_FOCUS; event.pressed = active;
    if (!active) OSVideoCapture(0);
    Queue(event); dirty = 1; Cursor();
}

static unsigned Modifiers(NSEvent *event)
{
    unsigned flags = [event modifierFlags], result = 0;
    if (flags & NSShiftKeyMask) result |= OS_MOD_SHIFT;
    if (flags & NSControlKeyMask) result |= OS_MOD_CONTROL;
    if (flags & NSAlternateKeyMask) result |= OS_MOD_ALT;
    if (flags & NSCommandKeyMask) result |= OS_MOD_COMMAND;
    if (flags & NSAlphaShiftKeyMask) result |= OS_MOD_CAPS;
    if (flags & NSNumericPadKeyMask) result |= OS_MOD_KEYPAD;
    return result;
}

static void Key(NSEvent *native, int down)
{
    OSHostEvent event;
    NSString *characters = [native type] == NSFlagsChanged ? nil : [native charactersIgnoringModifiers];
    memset(&event, 0, sizeof(event));
    event.type = OS_EVENT_KEY;
    event.key = [characters length] ? [characters characterAtIndex:0] : 0;
    event.scancode = [native keyCode];
    event.pressed = down; event.modifiers = Modifiers(native);
    Queue(event);
}

static void Button(NSEvent *native, int button, int down)
{
    OSHostEvent event;
    NSPoint point = [view convertPoint:[native locationInWindow] fromView:nil];
    memset(&event, 0, sizeof(event));
    event.type = OS_EVENT_BUTTON; event.key = button; event.pressed = down;
    event.x = point.x; event.y = [view bounds].size.height - 1 - point.y;
    Queue(event);
}

static void CloseWindow(void)
{
    captured = 0;
    if (cursor_hidden) { PSshowcursor(); cursor_hidden = 0; PSWait(); }
    if (view) [[NSNotificationCenter defaultCenter] removeObserver:view];
    if (application_menu) {
        NSArray *items = [application_menu itemArray];
        unsigned i;
        for (i = 0; i < [items count]; ++i) [[items objectAtIndex:i] setTarget:nil];
    }
    if (window) { [window setDelegate:nil]; [window makeFirstResponder:nil]; }
    if (bitmap) { [bitmap setDirectMapped:NO]; [bitmap release]; bitmap = nil; }
    if (screen) { [screen release]; screen = nil; }
    if (window) { [window orderOut:nil]; [window close]; [window release]; window = nil; }
    if (view) { [view release]; view = nil; }
}

static void InstallMenu(void)
{
    id item;
    const char *titles[] = {"Fullscreen", "Capture Mouse", "Pause", "Reset",
                           "Mount...", "Unmount...", "Quit"};
    unsigned i;
    /* OPENSTEP's application stores this pointer without retaining the menu.
     * Keep our ownership for the application's lifetime, across mode changes. */
    if (!application_menu) {
        application_menu = [[NSMenu alloc] initWithTitle:@"DOSBox-X"];
        for (i = 0; i < 7; i++) {
            item = [application_menu addItemWithTitle:[NSString stringWithCString:titles[i]]
                action:@selector(command:) keyEquivalent:@""];
            [item setTag:i];
        }
        [NSApp setMainMenu:application_menu];
    }
    for (i = 0; i < 7; i++) {
        item = [application_menu itemWithTag:i];
        [item setTarget:view];
    }
}

int OSVideoInit(OSEventHandler handler)
{
    if (!root_pool) root_pool = [[NSAutoreleasePool alloc] init];
    /* Foundation caches the original argv before main; a login-style argv[0]
     * otherwise produces a nil defaults domain on OPENSTEP. */
    [[NSProcessInfo processInfo] setProcessName:@"DOSBox-X"];
    [NSApplication sharedApplication];
    [NSApp finishLaunching];
    event_handler = handler;
    signal(SIGTERM, SignalQuit); signal(SIGINT, SignalQuit);
    return NSApp != nil;
}

void OSVideoQuit(void)
{
    unsigned i;
    OSAudioClose();
    CloseWindow();
    [NSApp setMainMenu:nil];
    [application_menu release]; application_menu = nil;
    free(pixels); pixels = NULL;
    free(chosen_paths); chosen_paths = NULL;
    for (i = 0; i < event_count; i++) free((void *)events[i].path);
    event_count = 0;
    event_handler = NULL;
    [root_pool release]; root_pool = nil;
}

static void UpdateBitmap(void)
{
    NSRect rect = [view convertRect:[view bounds] toView:nil];
    [bitmap updateForRect:rect inWindow:window];
    [bitmap setDirectMapped:buffered ? NO : YES];
    [bitmap setBuffered:[bitmap isDirectMapped] ? NO : YES];
    fprintf(stderr, "Interceptor window: direct=%d buffered=%d size=%dx%d stride=%d\n",
        [bitmap isDirectMapped], [bitmap isBuffered], [bitmap pixelsWide],
        [bitmap pixelsHigh], [bitmap bytesPerRow]);
    dirty = 0;
}

int OSVideoSetMode(int new_width, int new_height, int new_bits, double new_aspect, int fullscreen)
{
    OSByte *new_pixels;
    NSFramebuffer *new_screen = nil;
    NSRect rect;
    unsigned new_pitch;
    int window_width, window_height;
    if (new_width < 1 || new_height < 1 || new_width > 4096 || new_height > 4096 ||
        (new_bits != 8 && new_bits != 15 && new_bits != 16 && new_bits != 32) ||
        !(new_aspect > 0.01 && new_aspect < 100)) return 0;
    new_pitch = (new_width * ((new_bits + 7) / 8) + 3) & ~3U;
    new_pixels = (OSByte *)calloc(new_pitch, new_height);
    if (!new_pixels) return 0;
    if (pixels && new_width == width && new_height == height && new_bits == bits)
        memcpy(new_pixels, pixels, new_pitch * new_height);
    if (fullscreen) {
        id number = [[[NSScreen mainScreen] deviceDescription] objectForKey:@"NSScreenNumber"];
        new_screen = [[NSFramebuffer alloc] initFromScreen:number ? [number intValue] : 0 andMapIfPossible:YES];
        rect = [[NSScreen mainScreen] frame];
        if (!new_screen || ![new_screen isMappable] || ![new_screen data] ||
            !OSPixelBytes(OSPixelEncoding([[new_screen pixelEncoding] cString])) ||
            [new_screen pixelsWide] != rect.size.width || [new_screen pixelsHigh] != rect.size.height) {
            [new_screen release]; new_screen = nil;
            fprintf(stderr, "Interceptor: fullscreen unavailable; using a window\n");
        }
    }
    Focus(0); CloseWindow(); free(pixels);
    pixels = new_pixels; pitch = new_pitch; width = new_width; height = new_height;
    bits = new_bits; aspect = new_aspect; screen = new_screen;
    window_width = width < 640 ? 640 : width;
    window_height = window_width / aspect;
    if (!screen) rect = NSMakeRect(100, 100, window_width, window_height);
    window = [[DOSBoxWindow alloc] initWithContentRect:rect
        styleMask:screen ? NSBorderlessWindowMask :
            NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:screen ? NSBackingStoreNonretained : NSBackingStoreRetained defer:NO];
    if (!window) { CloseWindow(); return 0; }
    [window setReleasedWhenClosed:NO]; [window setTitle:@"DOSBox-X - OPENSTEP"];
    if (screen) { [window setLevel:NSMainMenuWindowLevel + 1]; [window setHidesOnDeactivate:NO]; }
    rect.origin.x = rect.origin.y = 0;
    view = [[DOSBoxView alloc] initWithFrame:rect];
    [window setContentView:view]; [window setDelegate:view]; [window makeFirstResponder:view];
    [window setAcceptsMouseMovedEvents:YES];
    [[NSNotificationCenter defaultCenter] addObserver:view selector:@selector(applicationDidResignActive:)
        name:NSApplicationDidResignActiveNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:view selector:@selector(applicationDidBecomeActive:)
        name:NSApplicationDidBecomeActiveNotification object:NSApp];
    InstallMenu();
    [NSApp activateIgnoringOtherApps:YES]; [window makeKeyAndOrderFront:nil]; [window display]; PSWait();
    if (!screen) {
        rect = [view convertRect:[view bounds] toView:nil];
        bitmap = [[NSDirectBitmap alloc] initForRect:rect inWindow:window];
        if (!bitmap) { CloseWindow(); return 0; }
        UpdateBitmap();
    }
    dirty = 1; Cursor();
    fprintf(stderr, "Interceptor: %dx%dx%d %s\n", width, height, bits, screen ? "fullscreen" : "window");
    return 1;
}

OSByte *OSVideoPixels(unsigned *out_pitch) { if (out_pitch) *out_pitch = pitch; return pixels; }
int OSVideoFullscreen(void) { return screen != nil; }
void OSVideoBuffered(int value) { buffered = !!value; dirty = 1; }

const char *OSVideoChoosePaths(const char *title, int directories, int multiple,
                              int pattern_count, const char *const *patterns)
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSMutableArray *types = [NSMutableArray array];
    NSArray *names;
    NSMutableString *joined;
    int i, count, all_types = !pattern_count;
    int audio_paused = OSAudioPaused(), was_captured = captured;
    int fullscreen = OSVideoFullscreen();
    free(chosen_paths); chosen_paths = NULL;
    OSAudioPause(1); OSVideoCapture(0);
    /* Hide the fullscreen window while AppKit owns the modal display, keeping
     * the guest pixel buffer and mode intact for the suspended emulator. */
    if (fullscreen) { [window orderOut:nil]; PSWait(); Cursor(); }
    [panel setTitle:[NSString stringWithCString:title ? title : "Select"]];
    [panel setCanChooseDirectories:directories != 0 ? YES : NO];
    [panel setCanChooseFiles:directories != 1 ? YES : NO];
    [panel setAllowsMultipleSelection:multiple ? YES : NO];
    for (i = 0; i < pattern_count; ++i) {
        const char *pattern = patterns[i];
        if (!strncmp(pattern, "*.", 2) && !strpbrk(pattern + 2, "*?"))
            [types addObject:[NSString stringWithCString:pattern + 2]];
        else all_types = 1;
    }
    if ([panel runModalForDirectory:nil file:nil types:all_types ? nil : types] != NSOKButton)
        goto done;
    names = [panel filenames];
    count = [names count];
    joined = [NSMutableString string];
    for (i = 0; i < count; ++i) {
        NSString *name = [names objectAtIndex:i];
        if (strchr([name cString], '|')) {
            NSRunAlertPanel(@"Cannot select this path", @"The path contains a | character.", @"OK", nil, nil);
            goto done;
        }
        if (i) [joined appendString:@"|"];
        [joined appendString:name];
    }
    if (count) {
        const char *text = [joined cString];
        chosen_paths = (char *)malloc(strlen(text) + 1);
        if (chosen_paths) strcpy(chosen_paths, text);
    }
done:
    if (fullscreen) { [window makeKeyAndOrderFront:nil]; [window display]; dirty = 1; OSVideoPresent(); }
    OSAudioPause(audio_paused);
    if (was_captured) OSVideoCapture(1);
    return chosen_paths;
}

int OSVideoAlert(const char *title, const char *message, const char *first,
                 const char *second, const char *third)
{
    int result;
    int paused = OSAudioPaused(), capture = captured;
    int restore = screen && [window isVisible];
    OSAudioPause(1); OSVideoCapture(0);
    if (restore) { [window orderOut:nil]; PSWait(); Cursor(); }
    result = NSRunAlertPanel([NSString stringWithCString:title ? title : "DOSBox-X"],
        @"%@", [NSString stringWithCString:first ? first : "OK"],
        second ? [NSString stringWithCString:second] : nil,
        third ? [NSString stringWithCString:third] : nil,
        [NSString stringWithCString:message ? message : ""]);
    if (restore) { [window makeKeyAndOrderFront:nil]; [window display]; dirty = 1; OSVideoPresent(); }
    OSAudioPause(paused);
    if (capture) OSVideoCapture(1);
    return result;
}

void OSVideoDimensions(unsigned *ww, unsigned *wh, unsigned *sw, unsigned *sh)
{
    NSRect rect = view ? [view bounds] : NSMakeRect(0, 0, 640, 480);
    NSRect desktop = [[NSScreen mainScreen] frame];
    if (ww) *ww = rect.size.width; if (wh) *wh = rect.size.height;
    if (sw) *sw = desktop.size.width; if (sh) *sh = desktop.size.height;
}
void OSVideoTitle(const char *title) { if (title) [window setTitle:[NSString stringWithCString:title]]; }

void OSVideoPalette(unsigned first, unsigned count, const OSByte *rgba)
{
    if (!rgba || first > 256 || count > 256 - first) return;
    memcpy(palette + first * 4, rgba, count * 4);
}

void OSVideoPresent(void)
{
    id target;
    OSByte *data;
    int dw, dh, ow, oh, ox, oy, stride, bytes, y;
    enum OSPixelLayout layout;
    if (!pixels || !window || [window isMiniaturized]) return;
    Cursor();
    if (screen && !OSVideoActive()) return;
    if (bitmap) {
        if (dirty) UpdateBitmap();
        if (![bitmap tryLockBitmap]) {
            if (++lock_misses == 1) fprintf(stderr, "Interceptor: waiting for bitmap lock\n");
            PSWait();
            return;
        }
        lock_misses = 0;
    }
    target = screen ? (id)screen : (id)bitmap;
    dw = [target pixelsWide]; dh = [target pixelsHigh]; stride = [target bytesPerRow];
    data = (OSByte *)[target data]; layout = OSPixelEncoding([[target pixelEncoding] cString]);
    bytes = OSPixelBytes(layout);
    if (!data || !bytes || dw < 1 || dh < 1 || dw > 16384 || dh > 16384 || stride / bytes < dw) {
        if (bitmap) [bitmap unlockBitmap];
        fprintf(stderr, "Interceptor: invalid bitmap layout\n"); return;
    }
    OSPixelViewport(dw, dh, aspect, &ox, &oy, &ow, &oh);
    for (y = 0; y < dh; y++) {
        OSByte *row = data + (size_t)y * stride;
        if (y < oy || y >= oy + oh) memset(row, 0, dw * bytes);
        else { memset(row, 0, ox * bytes); memset(row + (ox + ow) * bytes, 0, (dw - ox - ow) * bytes); }
    }
    OSPixelBlit(data + (size_t)oy * stride + ox * bytes, stride, ow, oh,
        layout, pixels, pitch, width, height, bits, palette);
    if (bitmap) {
        /* OPENSTEP 4.2 flushIn: requires the bitmap to remain locked.
         * Unlocking first silently suppresses buffered presentation. */
        [bitmap flushIn:[view bounds]];
        [bitmap unlockBitmap];
    }
    dirty = 0; PSWait();
}

void OSVideoEvents(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSEvent *native;
    OSHostEvent event;
    unsigned i, count;
    while ((native = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast]
        inMode:NSDefaultRunLoopMode dequeue:YES])) {
        unsigned mods = Modifiers(native);
        if (([native type] == NSKeyDown || [native type] == NSKeyUp) && OSVideoActive() &&
            (mods & (OS_MOD_ALT | OS_MOD_COMMAND)) && [[native charactersIgnoringModifiers] length] &&
            [[native charactersIgnoringModifiers] characterAtIndex:0] == '\r') {
            if ([native type] == NSKeyDown && ![native isARepeat]) Action(OS_ACTION_FULLSCREEN);
        } else [NSApp sendEvent:native];
    }
    if (captured && OSVideoActive()) {
        NSPoint position = MousePosition();
        memset(&event, 0, sizeof(event)); event.type = OS_EVENT_MOTION;
        event.dx = position.x - mouse_origin.x; event.dy = mouse_origin.y - position.y;
        if (event.dx || event.dy) { CenterMouse(); Queue(event); }
    }
    if (quit_signal) { memset(&event, 0, sizeof(event)); event.type = OS_EVENT_QUIT; Queue(event); quit_signal = 0; }
    /* Drain before invoking callbacks: handlers may recreate windows and enqueue focus changes. */
    count = event_count; event_count = 0;
    if (count) {
        OSHostEvent pending[512];
        memcpy(pending, events, count * sizeof(OSHostEvent));
        for (i = 0; i < count; i++) {
            if (event_handler) event_handler(&pending[i]);
            free((void *)pending[i].path);
        }
    }
    Cursor();
    OSAudioService();
    [pool release];
}

@implementation DOSBoxView
- (BOOL)acceptsFirstResponder { return YES; }
- (void)drawRect:(NSRect)rect { dirty = 1; }
- (BOOL)windowShouldClose:(id)sender
{ OSHostEvent e; memset(&e, 0, sizeof(e)); e.type = OS_EVENT_QUIT; Queue(e); return NO; }
- (void)windowDidMove:(NSNotification *)note { OSVideoCapture(0); dirty = 1; }
- (void)windowDidResize:(NSNotification *)note { OSVideoCapture(0); dirty = 1; }
- (void)windowDidBecomeKey:(NSNotification *)note { Focus(1); }
- (void)windowDidResignKey:(NSNotification *)note { Focus(0); }
- (void)windowWillMiniaturize:(NSNotification *)note { Focus(0); }
- (void)windowDidDeminiaturize:(NSNotification *)note { dirty = 1; }
- (void)applicationDidResignActive:(NSNotification *)note
{ Focus(0); if (screen) { [window orderOut:nil]; PSWait(); } }
- (void)applicationDidBecomeActive:(NSNotification *)note
{ if (screen) [window makeKeyAndOrderFront:nil]; dirty = 1; }
- (void)keyDown:(NSEvent *)event { Key(event, 1); }
- (void)keyUp:(NSEvent *)event { Key(event, 0); }
- (void)flagsChanged:(NSEvent *)event { Key(event, 2); }
- (void)mouseDown:(NSEvent *)event { Button(event, 0, 1); }
- (void)mouseUp:(NSEvent *)event { Button(event, 0, 0); }
- (void)rightMouseDown:(NSEvent *)event { Button(event, 1, 1); }
- (void)rightMouseUp:(NSEvent *)event { Button(event, 1, 0); }
- (void)mouseMoved:(NSEvent *)native
{
    OSHostEvent e; NSPoint p;
    if (captured) return;
    p = [self convertPoint:[native locationInWindow] fromView:nil];
    memset(&e, 0, sizeof(e)); e.type = OS_EVENT_MOTION;
    e.x = p.x; e.y = [self bounds].size.height - 1 - p.y; Queue(e);
}
- (void)mouseDragged:(NSEvent *)native { [self mouseMoved:native]; }
- (void)rightMouseDragged:(NSEvent *)native { [self mouseMoved:native]; }
- (void)command:(id)sender
{
    int tag = [sender tag];
    if (tag == 6) { [self windowShouldClose:sender]; return; }
    Action(tag);
}
@end
