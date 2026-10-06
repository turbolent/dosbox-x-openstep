/* Capture the main OPENSTEP screen as binary RGB PPM through Interceptor.
 * Supports the Intel 32-bit BGRX layout; rejects other formats.
 * cc -o screenshot screenshot.m -framework AppKit -framework Foundation \
 *   -F/NextLibrary/PrivateFrameworks -framework Interceptor
 * Run through generic-exec: screenshot output.ppm
 */
#import <AppKit/AppKit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Private method signatures checked on OPENSTEP 4.2. */
@interface NSSimpleBitmap : NSObject
- (char *)data;
- (int)bytesPerRow;
- (int)pixelsWide;
- (int)pixelsHigh;
- pixelEncoding;
@end

@interface NSFramebuffer : NSSimpleBitmap
- (id)initFromScreen:(int)screen andMapIfPossible:(char)flag;
@end

extern void PSWait(void);

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool;
    NSScreen *screen;
    NSFramebuffer *framebuffer = nil;
    id number, encoding;
    const char *format;
    unsigned char *data, *snapshot = NULL, *row = NULL, *source;
    unsigned short endian = 1;
    FILE *output = NULL;
    size_t imageBytes, rowBytes;
    int width, height, stride, x, y, status = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: screenshot output.ppm\n");
        return 2;
    }
    pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    screen = [NSScreen mainScreen];
    if (!screen) {
        fprintf(stderr, "screenshot: no main screen\n");
        goto done;
    }
    number = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
    framebuffer = [[NSFramebuffer alloc]
        initFromScreen:number ? [number intValue] : 0 andMapIfPossible:YES];
    if (!framebuffer) {
        fprintf(stderr, "screenshot: cannot create framebuffer\n");
        goto done;
    }
    width = [framebuffer pixelsWide];
    height = [framebuffer pixelsHigh];
    stride = [framebuffer bytesPerRow];
    data = (unsigned char *)[framebuffer data];
    encoding = [framebuffer pixelEncoding];
    format = encoding ? [encoding cString] : NULL;
    if (!data || !format || width <= 0 || height <= 0 || stride <= 0 ||
        width > INT_MAX / 4 || stride < width * 4 ||
        *(unsigned char *)&endian != 1 ||
        strcmp(format, "--------RRRRRRRRGGGGGGGGBBBBBBBB")) {
        fprintf(stderr, "screenshot: unsupported framebuffer: "
            "%dx%d stride=%d encoding=%s\n", width, height, stride,
            format ? format : "(none)");
        goto done;
    }
    if ((size_t)height > (size_t)-1 / (size_t)stride) {
        fprintf(stderr, "screenshot: framebuffer size overflow\n");
        goto done;
    }
    imageBytes = (size_t)stride * (size_t)height;
    rowBytes = (size_t)width * 3;
    snapshot = (unsigned char *)malloc(imageBytes);
    row = (unsigned char *)malloc(rowBytes);
    if (!snapshot || !row) {
        fprintf(stderr, "screenshot: out of memory\n");
        goto done;
    }
    /* Copy once before conversion and file I/O. This is not an atomic
     * snapshot of other applications that may still be drawing. */
    PSWait();
    memcpy(snapshot, data, imageBytes);
    output = fopen(argv[1], "wb");
    if (!output) {
        perror("screenshot: fopen");
        goto done;
    }
    if (fprintf(output, "P6\n%d %d\n255\n", width, height) < 0)
        goto write_failed;
    for (y = 0; y < height; y++) {
        source = snapshot + (size_t)y * (size_t)stride;
        for (x = 0; x < width; x++) {
            row[x * 3]     = source[x * 4 + 2];
            row[x * 3 + 1] = source[x * 4 + 1];
            row[x * 3 + 2] = source[x * 4];
        }
        if (fwrite(row, 1, rowBytes, output) != rowBytes)
            goto write_failed;
    }
    if (fclose(output) != 0) {
        output = NULL;
        perror("screenshot: fclose");
        goto done;
    }
    output = NULL;
    fprintf(stderr, "screenshot: saved %dx%d to %s\n", width, height, argv[1]);
    status = 0;
    goto done;

write_failed:
    fprintf(stderr, "screenshot: output write failed; file may be incomplete\n");
done:
    if (output) fclose(output);
    free(row);
    free(snapshot);
    [framebuffer release];
    [pool release];
    return status;
}
