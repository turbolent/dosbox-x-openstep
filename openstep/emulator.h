/* Native application lifecycle, independent of AppKit's Objective-C ABI. */
#ifndef DOSBOX_OPENSTEP_EMULATOR_H
#define DOSBOX_OPENSTEP_EMULATOR_H
#include "dosbox.h"
#include "control.h"
#include "menu.h"
#include "mapper.h"
void OSEmulatorInit(void);
void OSEmulatorRun(void);
void OSEmulatorShutdown(void);
void OSFrontendConfigure(void);
void OSFrontendStart(void);
void OSFrontendShutdown(void);
int OSFrontendRestart(void);
void OSMenuInit(void);
void OSMapperShutdown(void);
bool VM_PowerOn(void);
extern std::string configfile, working_dir, dosboxpath;
#endif
