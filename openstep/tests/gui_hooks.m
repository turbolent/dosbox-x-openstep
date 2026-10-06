/* Separate instrumented emulator build. Uses the real AppKit event queue and
 * menu actions, with no automation code linked into the release executable. */
#import <AppKit/AppKit.h>
#import <AppKit/dpsopsNeXT.h>
#include "host.h"
#include <assert.h>
#include <string.h>

static int panel_action;
static NSButton *FindButton(NSView *view, NSString *title)
{
    NSArray *children = [view subviews];
    unsigned i;
    if ([view isKindOfClass:[NSButton class]] &&
        [[(NSButton *)view title] isEqualToString:title]) return (NSButton *)view;
    for (i = 0; i < [children count]; ++i) {
        NSButton *button = FindButton([children objectAtIndex:i], title);
        if (button) return button;
    }
    return nil;
}

@interface GUIApplication : NSApplication
- (void)drivePanel:(NSTimer *)timer;
@end
@implementation GUIApplication
- (void)drivePanel:(NSTimer *)timer
{
    NSWindow *panel = [self keyWindow];
    unichar ch = 27;
    NSString *text;
    int owner = 0, success = 0;
    DPSContext context;
    (void)timer;
    if (!panel_action || ![panel isKindOfClass:[NSPanel class]]) return;
    text = [NSString stringWithCharacters:&ch length:1];
    [self postEvent:[NSEvent keyEventWithType:NSKeyDown location:NSMakePoint(0, 0)
        modifierFlags:0 timestamp:0 windowNumber:[panel windowNumber]
        context:[NSDPSContext currentContext] characters:text
        charactersIgnoringModifiers:text isARepeat:NO keyCode:0xfe] atStart:NO];
    context = [[NSDPSContext currentContext] DPSContext];
    DPScurrentactiveapp(context, &owner);
    DPSposteventbycontext(context, NSLeftMouseUp, 0, 0, 0, 0,
        [panel windowNumber], 0, 0, 0, owner, &success);
}
- (void)sendEvent:(NSEvent *)event
{
    if ([event type] == NSKeyDown && [event keyCode] == 0xfe) {
        NSWindow *panel = [self keyWindow];
        if (panel_action == 2 && [panel isKindOfClass:[NSOpenPanel class]]) {
            NSForm *form = (NSForm *)[[panel contentView] viewWithTag:NSFileHandlingPanelForm];
            assert(form);
            [(NSOpenPanel *)panel setDirectory:@"/me/dosbox-x-native/openstep"];
            [[form cellAtIndex:0] setStringValue:@"guest-fixtures"];
            panel_action = 0;
            [(NSOpenPanel *)panel ok:nil];
            fprintf(stderr, "GUI test: selected fixture directory\n");
        } else {
            NSButton *button = FindButton([panel contentView], panel_action == 1 ? @"Use C:" : @"Unmount C:");
            assert(button && [button isEnabled]);
            panel_action = panel_action == 1 ? 2 : 0;
            [button performClick:nil];
        }
    } else [super sendEvent:event];
}
@end

static unsigned PixelHash(void)
{
    unsigned pitch, i, hash = 2166136261U;
    OSByte *pixels = OSVideoPixels(&pitch);
    /* A fixed prefix is enough to catch a cleared buffer during a paused switch. */
    for (i = 0; i < pitch * 100; ++i) hash = (hash ^ pixels[i]) * 16777619U;
    return hash;
}
static unsigned paused_pixels;

int OSVideoInitReal(OSEventHandler);
void OSVideoEventsReal(void);
static OSUInt32 started, typed_at;
static int stage;
static const char *typing;

int OSVideoInit(OSEventHandler handler)
{
    int result;
    NSTimer *timer;
    [GUIApplication sharedApplication];
    result = OSVideoInitReal(handler);
    timer = [NSTimer timerWithTimeInterval:0.4 target:NSApp selector:@selector(drivePanel:)
                                userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
    started = OSHostTicks();
    return result;
}

static void Menu(int index)
{
    id item = [[NSApp mainMenu] itemWithTag:index];
    assert(item && [item isEnabled]);
    assert([NSApp sendAction:[item action] to:[item target] from:item]);
}

static void Type(void)
{
    unsigned now = OSHostTicks();
    NSWindow *window = [NSApp keyWindow];
    if (typing && *typing && window && now - typed_at >= 80) {
        unichar ch = *typing++;
        NSString *text = [NSString stringWithCharacters:&ch length:1];
        unsigned modifiers = (ch >= 'A' && ch <= 'Z') ||
            strchr("~!@#$%^&*()_+{}|:\"<>?", ch) ? NSShiftKeyMask : 0;
        int type;
        for (type = NSKeyDown; type <= NSKeyUp; ++type)
            [NSApp postEvent:[NSEvent keyEventWithType:type location:NSMakePoint(0, 0)
                modifierFlags:modifiers timestamp:0 windowNumber:[window windowNumber]
                context:[NSDPSContext currentContext] characters:text
                charactersIgnoringModifiers:text isARepeat:NO keyCode:ch] atStart:NO];
        typed_at = now;
    }
}

void OSVideoEvents(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    unsigned elapsed = OSHostTicks() - started;
    static const unsigned at[] = {1000, 4000, 6000, 8000, 9000, 10000, 11000,
                                 12000, 13000, 17000, 21000, 26000, 30000, 32000, 36000};
    Type();
    if (stage < 15 && elapsed >= at[stage]) {
        fprintf(stderr, "GUI test stage %d at %u ms\n", stage, elapsed);
        switch (stage++) {
        case 0: typing = "echo APPKIT INPUT\r"; break;
        case 1: Menu(0); break;
        case 2: assert(OSVideoFullscreen()); Menu(0); break;
        case 3: assert(!OSVideoFullscreen()); Menu(1); break;
        case 4: assert(OSVideoCaptured()); Menu(1); break;
        case 5: assert(!OSVideoCaptured()); Menu(2); break;
        case 6: assert(OSAudioPaused()); paused_pixels = PixelHash(); Menu(0); break;
        case 7: assert(OSVideoFullscreen() && PixelHash() == paused_pixels); Menu(0); Menu(2); break;
        case 8: assert(!OSAudioPaused()); Menu(3); break;
        case 9: typing = "echo RESET INPUT\r"; break;
        case 10: panel_action = 1; Menu(4); break;
        case 11: assert(!panel_action); typing = "type c:\\alpha.txt\r"; break;
        case 12: panel_action = 3; Menu(5); break;
        case 13: assert(!panel_action); typing = "type c:\\alpha.txt\r"; break;
        case 14: fprintf(stderr, "GUI test: Quit menu\n"); Menu(6); break;
        }
    }
    OSVideoEventsReal();
    [pool release];
}
