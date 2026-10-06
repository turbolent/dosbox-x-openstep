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

#ifndef DOSBOX_IMAGE_DISK_INT13_H
#define DOSBOX_IMAGE_DISK_INT13_H

#include "bios_disk.h"

#if !defined(OSFREE)
class imageDiskINT13Drive : public imageDisk {
public:
	Int13Status Read_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,void * data,unsigned int req_sector_size=0) DBX_OVERRIDE;
	Int13Status Write_Sector(uint32_t head,uint32_t cylinder,uint32_t sector,const void * data,unsigned int req_sector_size=0) DBX_OVERRIDE;
	Int13Status Read_AbsoluteSector(uint32_t sectnum, void * data) DBX_OVERRIDE;
	Int13Status Write_AbsoluteSector(uint32_t sectnum, const void * data) DBX_OVERRIDE;

	void UpdateFloppyType(void) DBX_OVERRIDE;
	void Set_Reserved_Cylinders(Bitu resCyl) DBX_OVERRIDE;
	uint32_t Get_Reserved_Cylinders() DBX_OVERRIDE;
	void Set_Geometry(uint32_t setHeads, uint32_t setCyl, uint32_t setSect, uint32_t setSectSize) DBX_OVERRIDE;
	void Get_Geometry(uint32_t * getHeads, uint32_t *getCyl, uint32_t *getSect, uint32_t *getSectSize) DBX_OVERRIDE;
	uint8_t GetBiosType(void) DBX_OVERRIDE;
	uint32_t getSectSize(void) DBX_OVERRIDE;
	bool detectDiskChange(void) DBX_OVERRIDE;

	imageDiskINT13Drive(imageDisk *sdisk);
	virtual ~imageDiskINT13Drive();

	uint8_t bios_disk;
	bool enable_int13;
	imageDisk* subdisk;
	bool busy;
};
#endif

#endif
