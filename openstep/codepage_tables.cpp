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
/* Codepage tables shared by the native filename and DOS text paths. */
#include "dosbox.h"
#include <map>
std::map<int,int> lowboxdrawmap;
struct Init_lowboxdrawmap { Init_lowboxdrawmap() {
    lowboxdrawmap[1] = 201;
    lowboxdrawmap[2] = 187;
    lowboxdrawmap[3] = 200;
    lowboxdrawmap[4] = 188;
    lowboxdrawmap[5] = 186;
    lowboxdrawmap[6] = 205;
    lowboxdrawmap[0xe] = 178;
    lowboxdrawmap[0x10] = 206;
    lowboxdrawmap[0x14] = 177;
    lowboxdrawmap[0x15] = 202;
    lowboxdrawmap[0x16] = 203;
    lowboxdrawmap[0x17] = 185;
    lowboxdrawmap[0x19] = 204;
    lowboxdrawmap[0x1a] = 176;
} };
static Init_lowboxdrawmap init_lowboxdrawmap;
std::map<int,int> pc98boxdrawmap;
struct Init_pc98boxdrawmap { Init_pc98boxdrawmap() {
    pc98boxdrawmap[0xB4] = 0x67;
    pc98boxdrawmap[0xB9] = 0x6e;
    pc98boxdrawmap[0xBA] = 0x46;
    pc98boxdrawmap[0xBB] = 0x56;
    pc98boxdrawmap[0xBC] = 0x5E;
    pc98boxdrawmap[0xB3] = 0x45;
    pc98boxdrawmap[0xBF] = 0x53;
    pc98boxdrawmap[0xC0] = 0x57;
    pc98boxdrawmap[0xC1] = 0x77;
    pc98boxdrawmap[0xC2] = 0x6f;
    pc98boxdrawmap[0xC3] = 0x5f;
    pc98boxdrawmap[0xC4] = 0x43;
    pc98boxdrawmap[0xC5] = 0x7f;
    pc98boxdrawmap[0xC8] = 0x5A;
    pc98boxdrawmap[0xC9] = 0x52;
    pc98boxdrawmap[0xCA] = 0x7e;
    pc98boxdrawmap[0xCB] = 0x76;
    pc98boxdrawmap[0xCC] = 0x66;
    pc98boxdrawmap[0xCD] = 0x44;
    pc98boxdrawmap[0xD9] = 0x5B;
    pc98boxdrawmap[0xDA] = 0x4f;
} };
static Init_pc98boxdrawmap init_pc98boxdrawmap;
uint16_t cpMap_AX[32] = {
	0x0020, 0x00b6, 0x2195, 0x2194, 0x2191, 0x2193, 0x2192, 0x2190, 0x2500, 0x2502, 0x250c, 0x2510, 0x2518, 0x2514, 0x251c, 0x252c,
	0x2524, 0x2534, 0x253c, 0x2550, 0x2551, 0x2554, 0x2557, 0x255d, 0x255a, 0x2560, 0x2566, 0x2563, 0x2569, 0x256c, 0x00ab, 0x00bb
};
