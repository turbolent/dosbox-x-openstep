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
 */
/* Native state catalogue and DOS helpers, derived from menu_callback.cpp at
 * e013b8bf8. The emulator updates these named records even when a platform
 * does not expose the corresponding command. Only AppKit's implemented actions
 * are displayed; unsupported stock callbacks are deliberately not attached.
 * Lookup remains strict, so missing initialization is an error.
 */
#include "emulator.h"
#include "mem.h"
#include "regs.h"
#include "shell.h"
#include "jfont.h"
#include "ide.h"
#include "menudef.h"
#include "callback.h"
#include "../src/ints/int10.h"
#include <sstream>
extern unsigned int page;
extern bool noremark_save_state, force_load_state, use_save_file, mountfro[26], mountiro[26];
extern int enablelfn;
extern bool winrun, clearline;
extern bool video_debug_overlay;
extern SHELL_Cmd cmd_list[];
size_t GetGameState_Run(void);
void DOSV_FillScreen(void);
static std::string Number(unsigned long v) { std::ostringstream s; s << v; return s.str(); }
const char *scaler_menu_opts[][2] = {
    { "none",                   "None" },
    { "normal2x",               "Normal 2X" },
    { "normal3x",               "Normal 3X" },
    { "normal4x",               "Normal 4X" },
    { "normal5x",               "Normal 5X" },
    { "hardware_none",          "Hardware None" },
    { "hardware2x",             "Hardware 2X" },
    { "hardware3x",             "Hardware 3X" },
    { "hardware4x",             "Hardware 4X" },
    { "hardware5x",             "Hardware 5X" },
    { "gray",                   "Grayscale" },
    { "gray2x",                 "Grayscale 2X" },
    { "tv2x",                   "TV 2X" },
    { "tv3x",                   "TV 3X" },
    { "scan2x",                 "Scan 2X" },
    { "scan3x",                 "Scan 3X" },
    { "rgb2x",                  "RGB 2X" },
    { "rgb3x",                  "RGB 3X" },
    { "advmame2x",              "Advanced MAME 2X" },
    { "advmame3x",              "Advanced MAME 3X" },
    { "hq2x",                   "HQ 2X" },
    { "hq3x",                   "HQ 3X" },
    { "advinterp2x",            "Advanced Interpolation 2X" },
    { "advinterp3x",            "Advanced Interpolation 3X" },
    { "2xsai",                  "2xSai" },
    { "super2xsai",             "Super2xSai" },
    { "supereagle",             "SuperEagle" },
#if C_XBRZ
    { "xbrz",                   "xBRZ" },
    { "xbrz_bilinear",          "xBRZ Bilinear" },
#endif
    { NULL, NULL }
};
const char *drive_opts[][2] = {
#if defined(WIN32)
	{ "mountauto",              "Auto-mount Windows drive" },
#endif
	{ "mounthd",                "Mount folder as hard drive" },
	{ "mountcd",                "Mount folder as CD drive" },
	{ "mountfd",                "Mount folder as floppy drive" },
	{ "mountfro",               "Option: mount folder read-only" },
	{ "div1",                   "--" },
	{ "mountarc",               "Mount an archive file (ZIP/7Z)" },
	{ "mountimg",               "Mount a disk or CD image file" },
	{ "mountimgs",              "Mount multiple disk/CD images" },
	{ "mountiro",               "Option: mount images read-only" },
    { "div2",                   "--" },
    { "unmount",                "Unmount drive" },
    { "rescan",                 "Rescan drive" },
    { "swap",                   "Swap disk" },
    { "info",                   "Drive information" },
    { "div3",                   "--" },
    { "boot",                   "Boot from drive" },
    { "bootimg",                "Boot from disk image" },
    { "saveimg",                "Save to disk image" },
    { NULL, NULL }
};
void update_dos_ems_menu(void) {
    Section_prop * dos_section = static_cast<Section_prop *>(control->GetSection("dos"));
    const char *ems = dos_section->Get_string("ems");
    if (ems == NULL) return;
    mainMenu.get_item("dos_ems_true").check(!strcmp(ems, "true")||!strcmp(ems, "1")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_board").check(!strcmp(ems, "emsboard")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_emm386").check(!strcmp(ems, "emm386")).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ems_false").check(!strcmp(ems, "false")||!strcmp(ems, "0")).enable(true).refresh_item(mainMenu);
}

void dos_ver_menu(bool start) {
    mainMenu.get_item("dos_ver_330").check(dos.version.major==3&&dos.version.minor==30).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_500").check(dos.version.major==5&&dos.version.minor==00).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_622").check(dos.version.major==6&&dos.version.minor==22).enable(true).refresh_item(mainMenu);
    mainMenu.get_item("dos_ver_710").check(dos.version.major==7&&dos.version.minor==10).enable(true).refresh_item(mainMenu);
    if (start || enablelfn != -2) uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && (dos.version.major>6 || winrun));
}

bool clear_screen() {
    if (CurMode->mode>7&&CurMode->mode!=0x0019&&CurMode->mode!=0x0043&&CurMode->mode!=0x0054&&CurMode->mode!=0x0055&&CurMode->mode!=0x0064)
        return false;
    if (CurMode->type==M_TEXT || dos_kernel_disabled) {
        const uint8_t rows = (IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS):24);
        const uint16_t cols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
        INT10_ScrollWindow(0, 0, rows, static_cast<uint8_t>(cols), -rows, 0x7, 0xff);
        INT10_SetCursorPos(0, 0, 0);
    } else if (IS_PC98_ARCH) {
        char msg[]="\033[2J";
        uint16_t s = (uint16_t)strlen(msg);
        DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
    } else {
        uint16_t oldax=reg_ax;
        reg_ax=(uint16_t)CurMode->mode;
        CALLBACK_RunRealInt(0x10);
        if (IS_DOSV && DOSV_CheckCJKVideoMode()) DOSV_FillScreen();
        reg_ax = oldax;
    }
    clearline=true;
    return true;
}

void show_prompt() {
    if (!dos_kernel_disabled && RunningProgram == "COMMAND") {
        DOS_Shell temp;
        temp.exit = true;
        temp.ShowPrompt();
    }
}

bool setlines(const char *mname) {
    uint16_t oldax=reg_ax, oldbx=reg_bx, oldcx=reg_cx;
    if (!strcmp(mname, "line_80x25")) {
        reg_ax = 0x0003;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 25);
#endif
    } else if (!strcmp(mname, "line_80x43")) {
        reg_ax = 0x0019;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 43);
#endif
    } else if (!strcmp(mname, "line_80x50")) {
        reg_ax = 0x1202;
        reg_bl = 0x30;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 3;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x1112;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x100;
        reg_cx = 0x808;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 50);
#endif
    } else if (!strcmp(mname, "line_80x60")) {
        reg_ax = 0x0043;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(80, 60);
#endif
    } else if (!strcmp(mname, "line_132x25")) {
        reg_ax = 0x0055;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 25);
#endif
    } else if (!strcmp(mname, "line_132x43")) {
        reg_ax = 0x0054;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 43);
#endif
    } else if (!strcmp(mname, "line_132x50")) {
        reg_ax = 0x0055;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x1112;
        CALLBACK_RunRealInt(0x10);
        reg_ax = 0x100;
        reg_cx = 0x808;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 50);
#endif
    } else if (!strcmp(mname, "line_132x60")) {
        reg_ax = 0x0064;
        CALLBACK_RunRealInt(0x10);
#if defined(USE_TTF)
        if (ttf.inUse) ttf_setlines(132, 60);
#endif
    } else
        return false;
    reg_ax = oldax;
    reg_bx = oldbx;
    reg_cx = oldcx;
    return true;
}

void refresh_slots() {
    std::string text=mainMenu.get_item("current_page").get_text();
    std::size_t found = text.find(":");
    if (found!=std::string::npos) text = text.substr(0, found);
    mainMenu.get_item("current_page").set_text(text+": "+Number(page+1)+"/10").refresh_item(mainMenu);
	for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
		char name[6]="slot0";
		name[4]='0'+i;
		std::string command=SaveState::instance().getName(page*SaveState::SLOT_COUNT+i);
		std::string str=std::string(MSG_Get("SLOT"))+" "+Number(page*SaveState::SLOT_COUNT+i+1)+(command==""?"":" "+command);
		mainMenu.get_item(name).set_text(str.c_str()).refresh_item(mainMenu);
	}
}
void AllocCallback1() {
        /* stock top-level menu items */
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainMenu");
            item.set_text("Main");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainSendKey");
                item.set_text("Send special key");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"MainHostKey");
                item.set_text("Select host key");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"WheelToArrow");
                item.set_text("Mouse wheel movements");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"SharedClipboard");
                item.set_text("Shared clipboard functions");
            }
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuMenu");
            item.set_text("CPU");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuCoreMenu");
                item.set_text("CPU core");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuTypeMenu");
                item.set_text("CPU type");
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CpuSpeedMenu");
                item.set_text("Emulate CPU speed");
            }
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu88-4").set_text("8088 XT 4.77MHz (~240 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-8").set_text("286 8MHz (~750 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-12").set_text("286 12MHz (~1510 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu286-25").set_text("286 25MHz (~3300 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu386-25").set_text("386DX 25MHz (~4595 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu386-33").set_text("386DX 33MHz (~6075 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-33").set_text("486DX 33MHz (~12010 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-66").set_text("486DX2 66MHz (~23880 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-100").set_text("486DX4 100MHz (~33445 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu486-133").set_text("486DX5 133MHz (~47810 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-60").set_text("Pentium 60MHz (~32090 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-66").set_text("Pentium 66MHz (~35620 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-75").set_text("Pentium 75MHz (~40072 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-90").set_text("Pentium 90MHz (~48087 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-100").set_text("Pentium 100MHz (~53430 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-120").set_text("Pentium 120MHz (~64180 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-133").set_text("Pentium 133MHz (~71240 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-166").set_text("Pentium 166MHz MMX (~95548 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu586-200").set_text("Pentium 200MHz MMX (~114657 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuak6-200").set_text("AMD K6 200MHz (~130000 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuak6-300").set_text("AMD K6-2 300MHz (~193000 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpuath-600").set_text("AMD Athlon 600MHz (~306000 cycles)").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"cpu686-866").set_text("Pentium III 866MHz EB (~407000 cycles)").enable(false);
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoMenu");
            item.set_text("Video");
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoFrameskipMenu");
                item.set_text("Frameskip");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"frameskip_0").set_text("Off").enable(false);

                for (unsigned int f=1;f <= 10;f++) {
                    char tmp1[64],tmp2[64];

                    sprintf(tmp1,"frameskip_%u",f);
                    sprintf(tmp2,"%u frame",f);

                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,tmp1).set_text(tmp2).enable(false);
                }
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoRatioMenu");
                item.set_text("Aspect ratio");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_1_1").set_text("1:1").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_3_2").set_text("3:2").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_4_3").set_text("4:3").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_16_9").set_text("16:9").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_16_10").set_text("16:10").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_18_10").set_text("18:10").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_original").set_text("Original ratio").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_ratio_set").set_text("Set ratio").enable(false);
            }
#if defined(WIN32) || defined(MACOSX)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoPreventCaptureMenu");
                item.set_text("Screen capture control");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"prevcap_none").set_text("Allow").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"prevcap_blank").set_text("Show as blank").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"prevcap_invisible").set_text("Make invisible").enable(false);
            }
#endif
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoScalerMenu");
                item.set_text("Scaler");

                for (size_t i=0;scaler_menu_opts[i][0] != NULL;i++) {
                    const std::string name = std::string("scaler_set_") + scaler_menu_opts[i][0];

                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(scaler_menu_opts[i][1]).enable(false);
                }

#if defined(C_SDL2)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"center_window").set_text("Center window").enable(false);
#endif

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"set_titletext").set_text("Set title bar text...").enable(false);

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"set_transparency").set_text("Set transparency...").enable(false);

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"refresh_rate").set_text("Adjust refresh rate...").enable(false);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoOutputMenu");
                item.set_text("Output");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_surface").set_text("Software (Surface)").enable(false);
#if C_DIRECT3D
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_direct3d").set_text("Direct3D").enable(false);
#if defined(C_SDL2)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "output_direct3d11").set_text("Direct3D11(Experimental)").enable(false);
#endif
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_opengl").set_text("OpenGL").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_openglnb").set_text("OpenGL nearest").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_openglpp").set_text("OpenGL perfect").enable(false);
#if defined(USE_TTF)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_ttf").set_text("TrueType font").enable(false);
#endif
#if C_GAMELINK
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"output_gamelink").set_text("Game Link").enable(false);
#endif
#if defined(MACOSX) && defined(C_SDL2) && C_METAL
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "output_metal").set_text("Metal(Experimental)").enable(false);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"doublescan").set_text("Doublescan").enable(false);
#if C_SDL2
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"modeswitch").set_text("Modeswitch").enable(false);
#endif
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoVsyncMenu");
                item.set_text("V-Sync");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_on").set_text("On").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_force").set_text("Force").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_host").set_text("Host").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_off").set_text("Off").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vsync_set_syncrate").set_text("Set syncrate").enable(false);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoOverscanMenu");
                item.set_text("Overscan");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"overscan_0").set_text("Off").enable(false);

                for (size_t i=1;i <= 10;i++) {
                    char tmp1[64],tmp2[64];

                    snprintf(tmp1,sizeof(tmp1),"overscan_%u",(unsigned)i);
                    snprintf(tmp2,sizeof(tmp2),"%u",(unsigned)i);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,tmp1).set_text(tmp2).enable(false);
                }
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoTextmodeMenu");
                item.set_text("Text-mode");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clear_screen").set_text("Clear the screen").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"vga_9widetext").set_text("Allow 9-pixel wide fonts").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"text_background").set_text("High intensity: background color").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"text_blinking").set_text("High intensity: blinking text").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x25").set_text("Screen: 80 columns x 25 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x43").set_text("Screen: 80 columns x 43 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x50").set_text("Screen: 80 columns x 50 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_80x60").set_text("Screen: 80 columns x 60 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x25").set_text("Screen: 132 columns x 25 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x43").set_text("Screen: 132 columns x 43 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x50").set_text("Screen: 132 columns x 50 lines").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"line_132x60").set_text("Screen: 132 columns x 60 lines").enable(false);
            }
#if defined(USE_TTF)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoTTFMenu");
                item.set_text("TTF options");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showbold").set_text("Display bold text in TTF").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showital").set_text("Display italic text in TTF").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showline").set_text("Display underlined text in TTF").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_showsout").set_text("Display strikeout text in TTF").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpno").set_text("TTF word processor: None").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpwp").set_text("TTF word processor: WordPerfect").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpws").set_text("TTF word processor: WordStar").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpxy").set_text("TTF word processor: XyWrite").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_wpfe").set_text("TTF word processor: FastEdit").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_blinkc").set_text("Display TTF blinking cursor").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_right_left").set_text("Display text from right to left").enable(false);
#if C_PRINTER
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_printfont").set_text("Use current TTF font for printing").enable(false);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_halfwidthkana").set_text("CJK: Allow half-width Japanese Katakana").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"ttf_extcharset").set_text("CJK: Enable extended Chinese character set").enable(false);
            }
#endif
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"VideoPC98Menu");
                item.set_text("PC-98 options");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_5mhz_gdc").set_text("5MHz GDC clock").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_allow_200scanline").set_text("Allow 200-line scanline effect").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_allow_4partitions").set_text("Allow 4 display partitions in graphics layer").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_egc").set_text("Enable EGC").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_grcg").set_text("Enable GRCG").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_analog").set_text("Enable analog display").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_analog256").set_text("Enable analog 256-color display").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_enable_188user").set_text("Enable 188+ user CG cells").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_clear_text").set_text("Clear text layer").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"pc98_clear_graphics").set_text("Clear graphics layer").enable(false);
            }
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"Video3dfxMenu");
                item.set_text("3dfx emulation");

                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"3dfx_voodoo").set_text("Internal Voodoo card").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"3dfx_glide").set_text("Glide passthrough").enable(false);
            }
#ifdef C_D3DSHADERS
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_d3d_shader").set_text("Select Direct3D pixel shader...").enable(false);
            }
#endif
#ifdef C_OPENGL
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_glsl_shader").set_text("Select OpenGL (GLSL) shader...").enable(false);
            }
#endif
#ifdef USE_TTF
            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id, "load_ttf_font").set_text("Select TrueType font (TTF/OTF)...").enable(false);
            }
#endif
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"SoundMenu");
            item.set_text("Sound");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_swapstereo").set_text("Swap stereo").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_mute").set_text("Mute").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"mixer_info").set_text("Show sound mixer volumes").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"sb_info").set_text("Show Sound Blaster configuration").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"midi_info").set_text("Show MIDI/OPL device configuration").enable(false);
            }
        }
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSMenu");
            item.set_text("DOS");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"enable_a20gate").set_text("Enable A20 gate").enable(false).check(MEM_A20_Enabled());
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"change_currentcd").set_text("Change current CD image...").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"change_currentfd").set_text("Change current floppy image...").enable(false);
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSMouseMenu");
                item.set_text("Mouse emulation");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_enable_int33").set_text("Internal Emulation").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_y_axis_reverse").set_text("Y-axis Reverse").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_mouse_sensitivity").set_text("Sensitivity").enable(false);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSVerMenu");
                item.set_text("Reported DOS version");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_330").set_text("3.30").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_500").set_text("5.00").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_622").set_text("6.22").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_710").set_text("7.10").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ver_edit").set_text("Edit").enable(false);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSLFNMenu");
                item.set_text("Long filename support");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_auto").set_text("Auto per reported DOS version").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_enable").set_text("Enable long filename emulation").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_lfn_disable").set_text("Disable long filename emulation").enable(false);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSPC98Menu");
                item.set_text("PC-98 PIT master clock");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_pc98_pit_4mhz").set_text("4MHz/8MHz PIT master clock").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_pc98_pit_5mhz").set_text("5MHz/10MHz PIT master clock").enable(false);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSEMSMenu");
                item.set_text("Expanded memory (EMS)");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_true").set_text("Enable EMS emulation").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_board").set_text("EMS board emulation").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_emm386").set_text("EMM386 emulation").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_ems_false").set_text("Disable EMS emulation").enable(false);
                }
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSDiskRateMenu");
                item.set_text("Limit disk transfer speed");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"limit_hdd_rate").set_text("Limit hard disk data rate").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"limit_floppy_rate").set_text("Limit floppy disk data rate").enable(false);
                }
            }

#if defined(WIN32) && !defined(HX_DOS) || defined(LINUX) || defined(MACOSX)
            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DOSWinMenu");
                item.set_text("Host system applications");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_autorun").set_text("Launch to run on the Windows host").enable(false)
#if defined(LINUX) || defined(MACOSX)
                    .enable(false)
#endif
                    ;
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_transpath").set_text("Translate paths to host system paths").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_wait").set_text("Wait for the application if possible").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"dos_win_quiet").set_text("Quiet mode - no start messages").enable(false);
                }
            }
#endif
        }
#if !defined(C_EMSCRIPTEN)
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CaptureMenu");
            item.set_text("Capture");
        }
#endif
# if (C_SSHOT)
        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"CaptureFormatMenu");
            item.set_text("Capture format");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"capture_fmt_avi_zmbv").set_text("AVI + ZMBV").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"capture_fmt_mpegts_h264").set_text("MPEG-TS + H.264").enable(false).
#  if (C_AVCODEC)
                enable(true);
#  else
                enable(false);
#  endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"capture_fmt_mpegts_h265").set_text("MPEG-TS + H.265").enable(false).
#  if (C_AVCODEC)
                enable(true);
#  else
                enable(false);
#  endif
            }
        }
# endif
		{
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"saveoptionmenu");
            item.set_text("Save/load options");
        }
		{
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"saveslotmenu");
            item.set_text("Select save slot");
		}
        {
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"enable_autosave").set_text("Enable auto-saving state").enable(false).enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"noremark_savestate").set_text("No remark when saving state").enable(false).check(noremark_save_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"force_loadstate").set_text("No warning when loading state").enable(false).check(force_load_state);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"removestate").set_text("Remove state in slot").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"refreshslot").set_text("Refresh display status").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id,"lastautosaveslot").set_text("Select last auto-saved slot").enable(false).enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "usesavefile").set_text("Use save file instead of save slot").enable(false).check(use_save_file);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "autosavecfg").set_text("Auto-save settings...").enable(false);
            mainMenu.alloc_item(DOSBoxMenu::item_type_id, "browsesavefile").set_text("Browse save file...").enable(false).enable(use_save_file);
            {
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"current_page").set_text("Current page: 1/10").enable(false).enable(false);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"prev_page").set_text("Previous page").enable(false);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"next_page").set_text("Next page").enable(false);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"first_page").set_text("Go to first page").enable(false);
				mainMenu.alloc_item(DOSBoxMenu::item_type_id,"last_page").set_text("Go to last page").enable(false);
				char name[6]="slot0";
				for (unsigned int i=0; i<SaveState::SLOT_COUNT; i++) {
					name[4]='0'+i;
					std::string str=std::string(MSG_Get("SLOT"))+" "+Number(page*SaveState::SLOT_COUNT+i+1);
					mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(str.c_str()).enable(false);
				}
            }
            if (page!=GetGameState_Run()/SaveState::SLOT_COUNT) {
                page=(unsigned int)(GetGameState_Run()/SaveState::SLOT_COUNT);
                refresh_slots();
            }
			char name[6]="slot0";
			name[4]='0'+(char)(GetGameState_Run()%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DriveMenu");
            item.set_text("Drive");

            for (char c='A';c <= 'Z';c++) {
                mountfro[c-'A']=false;
                mountiro[c-'A']=false;
                std::string dmenu = "Drive";
                dmenu += c;

                std::string dmenut;
                dmenut = c;

                DOSBoxMenu::item &ditem = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,dmenu.c_str());
                ditem.set_text(dmenut.c_str());

                for (size_t i=0;drive_opts[i][0] != NULL;i++) {
                    const std::string name = std::string("drive_") + c + "_" + drive_opts[i][0];
                    if ((!strcmp(drive_opts[i][0], "boot")||!strcmp(drive_opts[i][0], "bootimg")||!strcmp(drive_opts[i][0], "div3"))&&(c!='A'&&c!='C'&&c!='D'))
                        continue;
                    if (!strcmp(drive_opts[i][1], "--"))
                        mainMenu.alloc_item(DOSBoxMenu::separator_type_id,name);
                    else
                        mainMenu.alloc_item(DOSBoxMenu::item_type_id,name).set_text(drive_opts[i][1]).enable(false);
                }
            }

	    {
                char name[128],tmp[128];

                /* Additional menus for IDE-only device mounts */
                for (unsigned int ide=0;ide < MAX_IDE_CONTROLLERS;ide++) {
                    for (unsigned int ms=0;ms < 2;ms++) {
                        sprintf(name,"IDEDrive%u%c",ide+1,ms?'s':'m');
                        sprintf(tmp,"IDE %u%c",ide+1,ms?'s':'m');

                        DOSBoxMenu::item &ditem = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,name);
                        ditem.set_text(tmp);

                        for (size_t i=0;drive_opts[i][0] != NULL;i++) {
	                    const std::string sname = std::string(name) + "_" + drive_opts[i][0];
                            if (!strcmp(drive_opts[i][1], "--"))
                                mainMenu.alloc_item(DOSBoxMenu::separator_type_id,sname);
                            else
                                mainMenu.alloc_item(DOSBoxMenu::item_type_id,sname).set_text(drive_opts[i][1]).enable(false);
	                }
	            }
	        }
	    }

        }

        {
            DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpMenu");
            item.set_text("Help");

            {
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_intro").set_text("Introduction").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_homepage").set_text("DOSBox-X homepage").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_wiki").set_text("DOSBox-X Wiki guide").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_issue").set_text("DOSBox-X support").enable(false);
#if C_PCAP
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_nic").set_text("List network interfaces").enable(false);
#endif
#if C_PRINTER && defined(WIN32)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_prt").set_text("List printer devices").enable(false);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"help_about").set_text("About DOSBox-X").enable(false);
#if !defined(C_EMSCRIPTEN)
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_console").set_text("Show logging console").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"clear_console").set_text("Clear logging console").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"disable_logging").set_text("Disable logging output").enable(false).check(control->opt_nolog);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"wait_on_error").set_text("Console wait on error").enable(false).check(false);
#endif
#if C_DEBUG
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_codetext").set_text("Show code overview").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"show_logtext").set_text("Show logging text").enable(false);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"save_logas").set_text("Save logging as...").enable(false);

                debugrunmode = debuggerrun;
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_rundebug").set_text("Debugger option: Run debugger").enable(false).check(debugrunmode==0);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_runnormal").set_text("Debugger option: Run normal").enable(false).check(debugrunmode==1);
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debugger_runwatch").set_text("Debugger option: Run watch").enable(false).check(debugrunmode==2);
#endif
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"video_debug_overlay").set_text("Video debug overlay").enable(false).check(video_debug_overlay);
            }

            {
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpCommandMenu");
                item.set_text("DOS commands");

                {
                    uint32_t cmd_index=0;
                    while (cmd_list[cmd_index].name) {
                        if (!cmd_list[cmd_index].flags)
                            mainMenu.alloc_item(DOSBoxMenu::item_type_id,("command_"+std::string(cmd_list[cmd_index].name)).c_str()).set_text(cmd_list[cmd_index].name).enable(false);
                        cmd_index++;
                    }
                    cmd_index=0;
                    while (cmd_list[cmd_index].name) {
                        if (cmd_list[cmd_index].flags && strcmp(cmd_list[cmd_index].name, "CHDIR") && strcmp(cmd_list[cmd_index].name, "ERASE") && strcmp(cmd_list[cmd_index].name, "LOADHIGH") && strcmp(cmd_list[cmd_index].name, "MKDIR") && strcmp(cmd_list[cmd_index].name, "RMDIR") && strcmp(cmd_list[cmd_index].name, "RENAME") && strcmp(cmd_list[cmd_index].name, "DX-CAPTURE") && strcmp(cmd_list[cmd_index].name, "DEBUGBOX"))
                            mainMenu.alloc_item(DOSBoxMenu::item_type_id,("command_"+std::string(cmd_list[cmd_index].name)).c_str()).set_text(cmd_list[cmd_index].name).enable(false);
                        cmd_index++;
                    }
                }
            }

            {
#if C_DEBUG
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"DebugMenu");
                item.set_text("Debug");
                mainMenu.alloc_item(DOSBoxMenu::item_type_id,"HelpDebugMenu")
#else
                DOSBoxMenu::item &item = mainMenu.alloc_item(DOSBoxMenu::submenu_type_id,"HelpDebugMenu");
                item
#endif
                .set_text("Logging console");

                {
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_blankrefreshtest").set_text("Refresh test (blank display)").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_generatenmi").set_text("Generate NMI interrupt").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_int2fhook").set_text("Hook INT 2Fh calls").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logint21").set_text("Log INT 21h calls").enable(false);
                    mainMenu.alloc_item(DOSBoxMenu::item_type_id,"debug_logfileio").set_text("Log file I/O").enable(false);
                }
            }
        }
}

static void AllocHostState(void)
{
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "hostkey_mapper").set_text("Mapper-defined").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "showdetails").set_text("Show FPS and RT speed in title bar").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "restartinst").set_text("Restart DOSBox-X instance").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "restartconf").set_text("Restart DOSBox-X with config file...").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "loadlang").set_text("Load language file...").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "auto_lock_mouse").set_text("Autolock mouse").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_right").set_text("Via right mouse button").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_middle").set_text("Via middle mouse button").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_arrows").set_text("Via arrow keys (Home=start, End=end)").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "screen_to_clipboard").set_text("Copy all text on the DOS screen").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_device").set_text("Enable DOS clipboard device access").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_dosapi").set_text("Enable DOS clipboard API for applications").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "clipboard_biospaste").set_text("Use BIOS function for clipboard pasting").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_winlogo").set_text("Send logo key").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_winmenu").set_text("Send menu key").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_alttab").set_text("Send Alt+Tab").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_altsysrq").set_text("Send Alt+SysRq/PrtScr").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_ctrlesc").set_text("Send Ctrl+Esc").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_ctrlbreak").set_text("Send Ctrl+Break").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_cad").set_text("Send Ctrl+Alt+Del").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "hostkey_ctrlalt").set_text("Ctrl+Alt").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "hostkey_ctrlshift").set_text("Ctrl+Shift").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "hostkey_altshift").set_text("Alt+Shift").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_winlogo").set_text("Mapper \"Send special key\": logo key").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_winmenu").set_text("Mapper \"Send special key\": menu key").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_alttab").set_text("Mapper \"Send special key\": Alt+Tab").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_ctrlesc").set_text("Mapper \"Send special key\": Ctrl+Esc").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_ctrlbreak").set_text("Mapper \"Send special key\": Ctrl+Break").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sendkey_mapper_cad").set_text("Mapper \"Send special key\": Ctrl+Alt+Del").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_none").set_text("Do not convert to arrow keys").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_updown").set_text("Convert to up/down arrows").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_leftright").set_text("Convert to left/right arrows").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_pageupdown").set_text("Convert to PgUp/PgDn keys").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_ctrlupdown").set_text("Convert to Ctrl+up/down arrows").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_ctrlleftright").set_text("Convert to Ctrl+left/right arrows").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_ctrlpageupdown").set_text("Convert to Ctrl+PgUp/PgDn keys").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_ctrlwz").set_text("Convert to Ctrl+W/Z keys").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "wheel_guest").set_text("Enable for guest systems also").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "doublebuf").set_text("Double buffering (fullscreen)").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "alwaysontop").set_text("Always on top").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "highdpienable").set_text("High DPI enable").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "sync_host_datetime").set_text("Synchronize host date/time").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "shell_config_commands").set_text("Config options as commands").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "quick_reboot").set_text("Enable quick reboot").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "make_diskimage").set_text("Create blank disk image...").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "list_drivenum").set_text("Show mounted drive numbers").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "list_ideinfo").set_text("Show IDE disk or CD status").enable(false);
    mainMenu.alloc_item(DOSBoxMenu::item_type_id, "pc98_use_uskb").set_text("Use US keyboard layout").enable(false);
}

void OSMenuInit(void) { AllocCallback1(); AllocHostState(); }
