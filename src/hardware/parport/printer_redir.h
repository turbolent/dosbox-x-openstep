
// include guard
#ifndef DOSBOX_PRREDIR_H
#define DOSBOX_PRREDIR_H

#include "dosbox.h"
#include "parport.h"
#include "printer_if.h"

class CPrinterRedir : public CParallel {
public:
	CPrinterRedir(Bitu nr, uint8_t initIrq, CommandLine* cmd);
	

	~CPrinterRedir();
	
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	Bitu Read_PR() DBX_OVERRIDE;
	Bitu Read_COM() DBX_OVERRIDE;
	Bitu Read_SR() DBX_OVERRIDE;

	void Write_PR(Bitu) DBX_OVERRIDE;
	void Write_CON(Bitu) DBX_OVERRIDE;
	void Write_IOSEL(Bitu) DBX_OVERRIDE;
	bool Putchar(uint8_t) DBX_OVERRIDE;

	void handleUpperEvent(uint16_t type) DBX_OVERRIDE;
};

#endif	// include guard
