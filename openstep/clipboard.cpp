/* AppKit pasteboard to the active DOS code page. GPL-2.0-or-later. */
#include "dosbox.h"
#include "control.h"
#include "cross.h"
#include "host.h"
#include <stdlib.h>

uint8_t *clipAscii = NULL;
uint32_t clipSize = 0;
bool swapad = true;
extern std::string strPasteBuffer;
bool CodePageHostToGuestUTF16(char *, const uint16_t *);

static std::string ReadClipboard(void)
{
    std::string result;
    OSWord *text = OSClipboardRead();
    if (!text) return result;
    for (size_t i = 0; text[i]; ++i) {
        uint16_t character[3] = {text[i], 0, 0};
        if (text[i] >= 0xd800 && text[i] <= 0xdbff &&
            text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
            character[1] = text[++i];
        char converted[CROSS_LEN * 6];
        if (character[0] == '\r' || character[0] == '\n' || character[0] == '\t')
            result += (char)character[0];
        else if (character[0] >= 32)
            result += CodePageHostToGuestUTF16(converted, character) ? converted : "?";
    }
    free(text);
    return result;
}

void PasteClipboard(bool pressed)
{
    if (!pressed || control->SecureMode()) return;
    const std::string text = ReadClipboard();
    for (size_t i = 0; i < text.size(); ++i) {
        if (!swapad) strPasteBuffer += text[i];
        else if (text[i] == '\t') strPasteBuffer += "    ";
        else if (text[i] == '\n') {
            if (!i || text[i - 1] != '\r') strPasteBuffer += '\r';
        } else strPasteBuffer += text[i];
    }
}

bool getClipboard(void)
{
    free(clipAscii); clipAscii = NULL; clipSize = 0;
    if (control->SecureMode()) return false;
    const std::string text = ReadClipboard();
    std::string dos;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (!i || text[i - 1] != '\r')) dos += '\r';
        dos += text[i];
        if (text[i] == '\r' && (i + 1 == text.size() || text[i + 1] != '\n')) dos += '\n';
    }
    clipAscii = (uint8_t *)malloc(dos.size() + 1);
    if (!clipAscii) return false;
    memcpy(clipAscii, dos.c_str(), dos.size() + 1);
    clipSize = (uint32_t)dos.size();
    return clipSize != 0;
}
