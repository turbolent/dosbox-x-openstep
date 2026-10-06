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

// include guard
#ifndef DOSBOX_FILELPT_H
#define DOSBOX_FILELPT_H

#include "dosbox.h"
#include "parport.h"

typedef enum { FILE_DEV, FILE_CAPTURE, FILE_APPEND } DFTYPE;

class CFileLPT : public CParallel {
public:
	CFileLPT (Bitu nr, uint8_t initIrq, CommandLine* cmd, bool sq = false);

	~CFileLPT();
	
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	
	bool fileOpen;
	DFTYPE filetype;			// which mode to operate in (capture,fileappend,device)
	FILE* file;
	std::string name;			// name of the thing to open
	std::string action1, action2, action3, action4; // open with a program or batch script
	bool shellhide;
	bool addFF;					// add a formfeed character before closing the file/device
	bool addLF;					// if set, add line feed after carriage return if not used by app
    bool squote;

	uint8_t lastChar;				// used to save the previous character to decide whether to add LF
	const uint16_t* codepage_ptr; // pointer to the translation codepage if not null

	bool OpenFile();
	
	bool ack_polarity;

	Bitu Read_PR() DBX_OVERRIDE;
	Bitu Read_COM() DBX_OVERRIDE;
	Bitu Read_SR() DBX_OVERRIDE;

	uint8_t datareg;
	uint8_t controlreg;

	void Write_PR(Bitu) DBX_OVERRIDE;
	void Write_CON(Bitu) DBX_OVERRIDE;
	void Write_IOSEL(Bitu) DBX_OVERRIDE;
	bool Putchar(uint8_t) DBX_OVERRIDE;

	bool autofeed;
	bool ack;
	unsigned int timeout;
	Bitu lastUsedTick;
	void doAction();
	virtual void handleUpperEvent(uint16_t type) DBX_OVERRIDE;
};

#endif	// include guard
