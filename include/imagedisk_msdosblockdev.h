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

#ifndef DOSBOX_IMAGE_DISK_MSDOSBLOCKDEV_H
#define DOSBOX_IMAGE_DISK_MSDOSBLOCKDEV_H

#include "bios_disk.h"

#if !defined(OSFREE)
class imageDiskMSDOSBlockDevice : public imageDisk {
public:
	Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) DBX_OVERRIDE;
	Int13Status Write_AbsoluteSector(uint32_t sectnum, const void * data) DBX_OVERRIDE;

	bool detectDiskChange(void) DBX_OVERRIDE;

	imageDiskMSDOSBlockDevice();
	virtual ~imageDiskMSDOSBlockDevice();

	uint8_t media_dpb;
	uint8_t unit_code;
	uint16_t devseg;
	uint16_t attr;
	PhysPt devhdr;
	PhysPt bpbptr;
};
#endif

#endif
