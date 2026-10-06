/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *//* Native VM lifecycle. Initialization and DOS boot order follow
 * src/gui/sdlmain.cpp at e013b8bf8; no host-window code belongs here. */
#include "emulator.h"
#include "cpu.h"
#include "mem.h"
#include "regs.h"
#include "bios.h"
#include "bios_disk.h"
#include "shell.h"
#include "render.h"
#include "host.h"
#include "input.h"
#include "callback.h"
#include "../src/ints/int10.h"

extern unsigned int maincp;
extern bool guest_machine_power_on;
static bool initialized;
void CPU_Snap_Back_To_Real_Mode(void);
void CPU_Snap_Back_Forget(void);
void ShutDownMemHandles(Section *);
void INT10_Startup(Section *);
void BIOS_Enter_Boot_Phase(void);
extern std::vector<unsigned char> boot_code_image;
extern Bitu boot_code_image_load_to;
extern uint16_t boot_code_image_stack_ss, boot_code_image_stack_sp;

void MSG_Init();
void DOSBOX_RealInit();
void DOSBOX_InitTickLoop();
void TIMER_ShutdownTickHandlers();
void DOSBOX_SetupConfigSections(void);
void PAGING_Init();
void IO_Init();
void Init_VGABIOS();
void Init_AddressLimitAndGateMask();
void Init_RAM();
void Init_MemHandles();
void Init_MemoryAccessArray();
void Init_PCJR_CartridgeROM();
void Init_PS2_Port_92h();
void Init_A20_Gate();
void HARDWARE_Init();
void CAPTURE_Init();
void ROMBIOS_Init();
void CALLBACK_Init();
void Init_DMA();
void Init_PIC();
void PCIBUS_Init();
void PROGRAMS_Init();
void RENDER_Init();
void TIMER_Init();
void CMOS_Init();
void VGA_Init();
void CPU_Init();
void Weitek_Init();
void CPU_PreInit();
void ISAPNP_Cfg_Init();
#if C_FPU
void FPU_Init();
#endif
void KEYBOARD_Init();
void VOODOO_Init();
void GLIDE_Init();
void MIXER_Init();
void MIDI_Init();

/* Init all the sections */
void MPU401_Init();
#if C_DEBUG
void DEBUG_Init();
#endif
void SBLASTER_Init();
void GUS_Init();
void IMFC_Init();
void INNOVA_Init();
void WSS_Init();
void PCSPEAKER_Init();
void TANDYSOUND_Init();
void DISNEY_Init();
void PS1SOUND_Init();
void BIOS_Init();
void INT10_Init();
void JOYSTICK_Init();
void SERIAL_Init();
#if C_PRINTER
void PRINTER_Init();
#endif
void PARALLEL_Init();
void DONGLE_Init();
void DOS_Init();
void XMS_Init();
#if !defined(OSFREE)
void EMS_Init();
#endif
void MOUSE_Init();
#if !defined(OSFREE)
void DOS_KeyboardLayout_Init();
#endif
void CDROM_Image_Init();
void MSCDEX_Init();
void DRIVES_Init();
#if !defined(OSFREE)
void IPX_Init();
#endif
void IDE_Init();
void NE2K_Init();
void FDC_Primary_Init();
void AUTOEXEC_Init();
void DOS_InitClock();

void SHELL_Run();
void DisableINT33();
#if !defined(OSFREE)
void EMS_DoShutDown();
#endif
void XMS_DoShutDown();
void DOS_DoShutDown();
void GUS_DOS_Shutdown();
void SBLASTER_DOS_Shutdown();
void DOS_ShutdownDevices(void);
#if !defined(OSFREE)
void RemoveEMSPageFrame(void);
void RemoveUMBBlock();
#endif
void DOS_GetMemory_unmap();
void VFILE_Shutdown(void);
void PROGRAMS_Shutdown(void);
void DOS_UninstallMisc(void);
void CALLBACK_Shutdown(void);
void DOS_ShutdownDrives();
void VFILE_Shutdown(void);
void DOS_ShutdownFiles();
void FreeBIOSDiskList();
void GFX_ShutDown(void);
void MAPPER_Shutdown();
void RENDER_Shutdown();
void SHELL_Init(void);
void SHELL_MessagesInit(void);
void CopyClipboard(int all);
void CopyAllClipboard(bool bPressed);
void PasteClipboard(bool bPressed);
void PasteClipStop(bool bPressed);
void QuickEdit(bool bPressed);
void ClipKeySelect(int sym);
bool isModifierApplied(void);
bool PasteClipboardNext(void);
void CONFIGSHELL_Init(void);
void CONFIGSHELL_Run(void);

#if C_DYNAMIC_X86
void CPU_Core_Dyn_X86_Shutdown(void);
#endif


bool VM_Boot_DOSBox_Kernel() {
    if (!dos_kernel_disabled) {
#if !defined(OSFREE)
        RemoveEMSPageFrame();
        RemoveUMBBlock();
#endif
        DisableINT33();
        DOS_GetMemory_unmap();
        VFILE_Shutdown();
        PROGRAMS_Shutdown();
        DOS_UninstallMisc();
        SBLASTER_DOS_Shutdown();
        GUS_DOS_Shutdown();
#if !defined(OSFREE)
        EMS_DoShutDown();
#endif
        XMS_DoShutDown();
        DOS_DoShutDown();

        DispatchVMEvent(VM_EVENT_DOS_SURPRISE_REBOOT); // <- apparently we rebooted without any notification (such as jmp'ing to FFFF:0000)

        dos_kernel_disabled = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif
    }

    if (dos_kernel_disabled) {
        /* in case of reboot */
        Init_MemHandles();

        DispatchVMEvent(VM_EVENT_DOS_BOOT); // <- just starting the DOS kernel now

        /* DOS kernel init */
        dos_kernel_disabled = false; // FIXME: DOS_Init should install VM callback handler to set this
        void DOS_Startup(Section* sec);
        DOS_Startup(NULL);
        maincp = 0;

#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        Reflect_Menu();
#endif

        void update_pc98_function_row(unsigned char setting,bool force_redraw=false);

        void DRIVES_Startup(Section *s);
        DRIVES_Startup(NULL);

        /* NEC's function key row seems to be deeply embedded in the CON driver. Am I wrong? */
        if (IS_PC98_ARCH) update_pc98_function_row(1);

        DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY); // <- kernel is ready

        /* Date/time */
        DOS_InitClock();

#if !defined(OSFREE)
        /* keyboard mapping, at this point in CONFIG.SYS parsing, right? */
        void DOS_KeyboardLayout_Startup(Section* sec);
        DOS_KeyboardLayout_Startup(NULL);
#endif

        /* Most MS-DOS installations have a DEVICE=C:\HIMEM.SYS somewhere near the top of their CONFIG.SYS */
        void XMS_Startup(Section *sec);
        XMS_Startup(NULL);

#if !defined(OSFREE)
        /* And then after that, usually a DEVICE=C:\EMM386.EXE just after HIMEM.SYS */
        void EMS_Startup(Section* sec);
        EMS_Startup(NULL);
#endif

        /* possible return to DOSBox-X DOS environment */
        mainMenu.get_item("HelpCommandMenu").enable();

        SHELL_MessagesInit();
        CONFIGSHELL_Init();
        CONFIGSHELL_Run();

        DispatchVMEvent(VM_EVENT_DOS_INIT_CONFIG_SYS_DONE); // <- we just finished executing CONFIG.SYS
        SHELL_Init(); // <- NTS: this will change CPU instruction pointer!
        DispatchVMEvent(VM_EVENT_DOS_INIT_SHELL_READY); // <- we just finished loading the shell (COMMAND.COM)

        /* it's time to init parsing AUTOEXEC.BAT */
        void AUTOEXEC_Startup(Section *sec);
        AUTOEXEC_Startup(NULL);

        /* Most MS-DOS installations run MSCDEX.EXE from somewhere in AUTOEXEC.BAT. We do the same here, in a fashion. */
        /* TODO: Can we make this an OPTION if the user doesn't want to make MSCDEX.EXE resident? */
        /* TODO: When we emulate executing AUTOEXEC.BAT between INIT_SHELL_READY and AUTOEXEC_BAT_DONE, can we make a fake MSCDEX.EXE within drive Z:\
         *       and auto-add a Z:\DOS\MSCDEX.EXE to the top of AUTOEXEC.BAT, command line switches and all. if the user has not already added it? */
        void MSCDEX_Startup(Section* sec);
        MSCDEX_Startup(NULL);

        /* Some installations load the MOUSE.COM driver from AUTOEXEC.BAT as well */
        /* TODO: Can we make this an option? Can we add a fake MOUSE.COM to the Z: drive as well? */
        void MOUSE_Startup(Section *sec);
        MOUSE_Startup(NULL);

        DispatchVMEvent(VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE); // <- we just finished executing AUTOEXEC.BAT
        DispatchVMEvent(VM_EVENT_DOS_INIT_AT_PROMPT); // <- now, we're at the DOS prompt
        SHELL_Run();
    }

    return true;
}

bool VM_PowerOn() {
    if (!guest_machine_power_on) {
        // powering on means power on event, followed by reset assert, then reset deassert
        guest_machine_power_on = true;
        DispatchVMEvent(VM_EVENT_POWERON);
        DispatchVMEvent(VM_EVENT_RESET);
        DispatchVMEvent(VM_EVENT_RESET_END);
    }

    return true;
}

void OSEmulatorInit(void)
{
    DOSBOX_InitTickLoop();
    DOSBOX_RealInit();
    RENDER_Init(); CAPTURE_Init(); IO_Init(); HARDWARE_Init(); CPU_PreInit();
    Init_AddressLimitAndGateMask(); Init_MemoryAccessArray(); Init_A20_Gate();
    Init_PS2_Port_92h(); Init_RAM(); Init_DMA(); Init_PIC(); TIMER_Init();
    PCIBUS_Init(); PAGING_Init(); CMOS_Init(); ROMBIOS_Init(); CALLBACK_Init();
    Init_VGABIOS(); PROGRAMS_Init(); PCSPEAKER_Init(); TANDYSOUND_Init();
    MPU401_Init(); MIXER_Init(); MIDI_Init(); CPU_Init(); Weitek_Init(); FPU_Init();
    VGA_Init(); ISAPNP_Cfg_Init(); FDC_Primary_Init(); KEYBOARD_Init();
    SBLASTER_Init(); JOYSTICK_Init(); PS1SOUND_Init(); DISNEY_Init(); GUS_Init();
    IDE_Init(); INNOVA_Init(); WSS_Init(); BIOS_Init(); INT10_Init();
    SERIAL_Init(); DONGLE_Init(); PARALLEL_Init();
    if (machine == MCH_PCJR) Init_PCJR_CartridgeROM();
    MEM_A20_Enable(true);
    DOS_Init(); DRIVES_Init(); DOS_KeyboardLayout_Init(); MOUSE_Init();
    XMS_Init(); EMS_Init(); AUTOEXEC_Init(); MSCDEX_Init(); CDROM_Image_Init();
    Init_MemHandles(); MAPPER_Init(); MSG_Init();
    control->initialised = true;
    initialized = true;
}

static void StopDOSKernel(bool reboot)
{
    if (dos_kernel_disabled) return;
    extern bool keep_umb_on_boot, keep_private_area_on_boot;
    extern bool inshell;
    inshell = false;
    maincp = dos.loaded_codepage;
    DispatchVMEvent(reboot ? VM_EVENT_DOS_EXIT_REBOOT_BEGIN : VM_EVENT_DOS_EXIT_BEGIN);
    RemoveEMSPageFrame();
    if (reboot || !keep_umb_on_boot) RemoveUMBBlock();
    DisableINT33();
    if (reboot || !keep_private_area_on_boot || DOS_PRIVATE_SEGMENT < 0xA000)
        DOS_GetMemory_unmap();
    if (!reboot) {
        real_writed(0, 0x01 * 4, (uint32_t)BIOS_DEFAULT_HANDLER_LOCATION);
        real_writed(0, 0x03 * 4, (uint32_t)BIOS_DEFAULT_HANDLER_LOCATION);
    }
    VFILE_Shutdown(); PROGRAMS_Shutdown(); DOS_UninstallMisc();
    SBLASTER_DOS_Shutdown(); GUS_DOS_Shutdown(); EMS_DoShutDown();
    XMS_DoShutDown(); DOS_DoShutDown(); ShutDownMemHandles(NULL);
    dos_kernel_disabled = true;
    DispatchVMEvent(reboot ? VM_EVENT_DOS_EXIT_REBOOT_KERNEL : VM_EVENT_DOS_EXIT_KERNEL);
    CPU_Snap_Back_To_Real_Mode(); CPU_Snap_Back_Forget();
}

void OSEmulatorRun(void)
{
    if (!VM_PowerOn()) E_Exit("OPENSTEP: VM power-on failed");
    for (;;) {
        int signal = 0;
        try { DOSBOX_RunMachine(); return; }
        catch (int value) { signal = value; }
        OSInputRelease();
        if (signal == 0 || signal == 1) return;
        if (signal == 2) {
            StopDOSKernel(false);
            Section_prop *p = static_cast<Section_prop *>(control->GetSection("dosbox"));
            if (p->Get_bool("turn off a20 gate on boot")) MEM_A20_Enable(false);
            mainMenu.get_item("HelpCommandMenu").enable(false);
            if (!boot_code_image.empty()) {
                MEM_BlockWrite(boot_code_image_load_to, &boot_code_image[0], boot_code_image.size());
                boot_code_image_load_to = 0; boot_code_image.clear();
            }
            DispatchVMEvent(VM_EVENT_GUEST_OS_BOOT);
            SegSet16(ss, boot_code_image_stack_ss); reg_esp = boot_code_image_stack_sp;
        } else if (signal == 3 || signal == 9 || signal == 6 || signal == 7) {
            StopDOSKernel(true);
            CPU_Snap_Back_To_Real_Mode(); CPU_Snap_Back_Forget();
            if (signal == 6 || signal == 7) BIOS_Enter_Boot_Phase();
            else {
                DispatchVMEvent(VM_EVENT_RESET);
                DispatchVMEvent(VM_EVENT_RESET_END);
                if (IS_EGAVGA_ARCH) INT10_Startup(NULL);
            }
        } else E_Exit("OPENSTEP: unsupported VM transition %d", signal);
    }
}

void OSEmulatorShutdown(void)
{
    if (!initialized) return;
    OSInputRelease(); OSMapperShutdown(); OSAudioPause(1);
    CPU_Snap_Back_To_Real_Mode(); CPU_Snap_Back_Forget();
    DOS_ShutdownDrives(); DOS_ShutdownFiles(); DOS_ShutdownDevices();
    CALLBACK_Shutdown(); CPU_Core_Dyn_X86_Shutdown(); FreeBIOSDiskList();
    RENDER_Shutdown(); VFILE_Shutdown(); PROGRAMS_Shutdown(); TIMER_ShutdownTickHandlers();
    while (!exitfunctions.empty()) {
        Function_wrapper entry = exitfunctions.front();
        exitfunctions.pop_front(); entry.function(NULL);
    }
    initialized = false;
}

