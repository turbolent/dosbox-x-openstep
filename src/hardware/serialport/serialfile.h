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


#ifndef INCLUDEGUARD_SERIALFILE_H
#define INCLUDEGUARD_SERIALFILE_H

#include <string>

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialFile : public CSerial {
public:
	CSerialFile(Bitu id, CommandLine* cmd, bool sq = false);
	virtual ~CSerialFile();

	void setRTSDTR(bool rts, bool dtr) DBX_OVERRIDE;
	void setRTS(bool val) DBX_OVERRIDE;
	void setDTR(bool val) DBX_OVERRIDE;

	void updatePortConfig(uint16_t, uint8_t lcr) DBX_OVERRIDE;
	void updateMSR() DBX_OVERRIDE;
	void transmitByte(uint8_t val, bool first) DBX_OVERRIDE;
	void setBreak(bool value) DBX_OVERRIDE;
	void doAction();
	void handleUpperEvent(uint16_t type) DBX_OVERRIDE;

	FILE* fp;
	bool squote;
	bool shellhide;
	unsigned int timeout;
	Bitu lastUsedTick;
	std::string filename;
	std::string actstd, acterr; // open with a program or batch script
};

#endif // INCLUDEGUARD
