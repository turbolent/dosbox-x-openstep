#ifndef DOSBOX_OPENSTEP_INPUT_H
#define DOSBOX_OPENSTEP_INPUT_H
#include "host.h"
#include "keyboard.h"

/* Character/function-key translation does not depend on SDL key symbols. */
KBD_KEYS OSInputTranslate(int character, unsigned modifiers);
void OSInputEvent(const OSHostEvent *event);
void OSInputRelease(void);
void OSInputSensitivity(int x_percent, int y_percent);
/* Frontend owns quit/reset/pause and drive actions; borrowed paths are synchronous. */
typedef void (*OSInputActionHandler)(const OSHostEvent *event);
void OSInputActions(OSInputActionHandler handler);
#endif
