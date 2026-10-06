/* Separate AppKit test: exercise real modal panels and instrument only Cancel. */
#import <AppKit/AppKit.h>
#import <AppKit/dpsopsNeXT.h>
#include "host.h"
#include <assert.h>
#include <string.h>

static NSButton *CancelButton(NSView *view)
{
    NSArray *children;
    unsigned i;
    if ([view isKindOfClass:[NSButton class]] &&
        [[(NSButton *)view title] isEqualToString:@"Cancel"])
        return (NSButton *)view;
    children = [view subviews];
    for (i = 0; i < [children count]; ++i) {
        NSButton *button = CancelButton([children objectAtIndex:i]);
        if (button) return button;
    }
    return nil;
}

@interface DialogApplication : NSApplication
@end
@implementation DialogApplication
- (void)sendEvent:(NSEvent *)event
{
    if ([event type] == NSKeyDown && [event keyCode] == 0xfe) {
        NSWindow *panel = [self keyWindow];
        fprintf(stderr, "dialog: performing Cancel during event dispatch\n");
        [CancelButton([panel contentView]) performClick:nil];
    } else [super sendEvent:event];
}
@end

@interface DialogTest : NSObject
- (void)cancel:(NSTimer *)timer;
@end
static void RunDialogs(void *userdata);
@implementation DialogTest
- (void)cancel:(NSTimer *)timer
{
    NSOpenPanel *panel = nil;
    NSArray *windows = [NSApp windows];
    unsigned i;
    (void)timer;
    /* openPanel creates a new instance while another panel is modal. */
    for (i = 0; i < [windows count]; ++i) {
        id candidate = [windows objectAtIndex:i];
        if ([candidate isKindOfClass:[NSOpenPanel class]] && [candidate isVisible]) {
            panel = candidate;
            break;
        }
    }
    assert([panel isVisible]);
    assert(![panel allowsMultipleSelection]);
    fprintf(stderr, "dialog: cancelling visible AppKit panel\n");
    assert(CancelButton([panel contentView]));
    NSLog(@"dialog panel %@ key %@ button target %@ action %@ enabled %d",
        panel, [NSApp keyWindow],
        [CancelButton([panel contentView]) target],
        NSStringFromSelector([CancelButton([panel contentView]) action]),
        [CancelButton([panel contentView]) isEnabled]);
    {
        unichar ch = 27;
        NSString *text = [NSString stringWithCharacters:&ch length:1];
        [NSApp postEvent:[NSEvent keyEventWithType:NSKeyDown location:NSMakePoint(0, 0)
            modifierFlags:0 timestamp:0 windowNumber:[panel windowNumber]
            context:[NSDPSContext currentContext] characters:text
            charactersIgnoringModifiers:text isARepeat:NO keyCode:0xfe] atStart:NO];
        DPSContext context = [[NSDPSContext currentContext] DPSContext];
        int owner = 0, success = 0;
        DPScurrentactiveapp(context, &owner);
        DPSposteventbycontext(context, NSLeftMouseUp, 0, 0, 0, 0,
            [panel windowNumber], 0, 0, 0, owner, &success);
    }
    fprintf(stderr, "dialog: queued Cancel, application running=%d\n", [NSApp isRunning]); fflush(stderr);
}
@end

static void Events(const OSHostEvent *event) { (void)event; }
static void RunDialogs(void *userdata)
{
    DialogTest *test = (DialogTest *)userdata;
    int stage, x, y;
    unsigned pitch;
    OSByte *pixels;
    const char *patterns[] = { "*.iso", "*.cue" };
    for (stage = 0; stage < 3; ++stage) {
        NSTimer *timer;
        assert(OSVideoSetMode(320, 200, 32, 4.0/3.0, stage == 1));
        OSVideoBuffered(stage == 2);
        pixels = OSVideoPixels(&pitch);
        for (y = 0; y < 200; ++y) for (x = 0; x < 320; ++x) {
            OSByte *p = pixels + y * pitch + x * 4;
            p[0] = x; p[1] = y; p[2] = 120; p[3] = 0;
        }
        OSVideoPresent();
        fprintf(stderr, "dialog: stage %d open\n", stage); fflush(stderr);
        timer = [NSTimer timerWithTimeInterval:4 target:test selector:@selector(cancel:)
                                     userInfo:nil repeats:NO];
        [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
        assert(OSVideoChoosePaths("DOSBox-X file chooser test", 0, 0, 2, patterns) == NULL);
        assert(OSVideoPixels(NULL) == pixels && OSVideoFullscreen() == (stage == 1));
        assert(pixels[2] == 120 && pixels[pitch + 1] == 1);
        fprintf(stderr, "dialog: stage %d restored\n", stage); fflush(stderr);
        {
            OSUInt32 start = OSHostTicks();
            while (OSHostTicks() - start < 2000) { OSVideoEvents(); OSVideoPresent(); OSHostSleep(10); }
        }
    }
    puts("OPENSTEP modal dialog lifecycle passed");
}

int main(void)
{
    DialogTest *test;
    [DialogApplication sharedApplication];
    assert(OSVideoInit(Events));
    test = [[DialogTest alloc] init];
    OSApplicationRun(RunDialogs, test);
    [test release];
    OSVideoQuit();
    return 0;
}
