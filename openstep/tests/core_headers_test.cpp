/* Check ABI-sensitive backports with the actual target compiler. */
#include "dosbox.h"
#include "fpu.h"
#include "paging.h"
#include "inout.h"
#include "control.h"
#include "mapper.h"
#include "bios_disk.h"
#include "imagedisk_teledisk.h"
#include <cassert>

unsigned char FPU_ArchitectureType = FPU_ARCHTYPE_387;

int main()
{
    assert(sizeof(Int13Status) == 1);
    assert(sizeof(imageDiskVHD::ErrorCodes) == 4);
    assert(sizeof(imageDiskVHD::VHDTypes) == 4);
    Int13Status disk_status = Int13Status::SectorNotFound;
    assert(static_cast<uint8_t>(disk_status) == 4);
    disk_status = Int13Status::NoError;
    assert(disk_status == Int13Status::NoError);
    td0_sector_flags sector_flags = static_cast<td0_sector_flags>(uint8_t(6));
    assert(sector_flags & td0_sector_flags::crc_error);
    assert(sector_flags & td0_sector_flags::deleted_mark);
    assert(!(sector_flags & td0_sector_flags::dos_skipped));
    DOS_Block dos_defaults;
    assert(dos_defaults.date.year == 0 && dos_defaults.return_code == 0);
    assert(!dos_defaults.echo && !dos_defaults.set_jdosv_enabled);
    assert(dos_defaults.tables.country == NULL && dos_defaults.tables.dpb == 0);
    assert(dos_defaults.tables.dpb_size == 0x21 && dos_defaults.tables.mediaid_offset == 0x17);
    DOS_ParamBlock parameters(0);
    assert(parameters.exec.cmdtail == 0 && parameters.exec.initcsip == 0);
    assert(parameters.overlay.loadseg == 0 && parameters.overlay.relocation == 0);
    assert(sizeof(DOS_ParamBlock::sExec) == 22 && sizeof(DOS_ParamBlock::sOverlay) == 4);
    ClockDomain clock(14318180), divided(100000000, 3);
    assert(clock.freq_div == 1 && clock.counter == 0);
    assert(divided.freq_div == 3 && divided.counter == 0);
    FPUControlWord cw;
    FPUStatusWord sw;
    assert(cw.reg == 0x37f && sw.reg == 0);
    cw.RC = FPUControlWord::Chop;
    assert((cw.reg & 0xc00) == 0xc00 && cw.roundMode() == FPUControlWord::Chop);
    cw = 0;
    assert(cw.reg == 0x40 && cw.allMasked().reg == 0x7f);
    sw.top = 7; sw.C3 = true; sw.IE = true;
    assert(sw.reg == 0x7801 && unsigned(sw.top) == 7);
    FPUStatusWord copy(sw);
    copy.clearExceptions();
    assert(copy.reg == 0x7800 && sw.reg == 0x7801);
    FPU_Reg_80 fp = {{UINT64_C(0x8000000000000000), 0x3fff, 0}};
    assert(sizeof(fp) == 16 && fp.raw.l == UINT64_C(0x8000000000000000));
    assert(fp.raw.h == 0x3fff && IsNormal(fp));
#if defined(HAS_LONG_DOUBLE)
    assert(fp.v == 1.0L);
#endif
    fp.f.exponent = 0x7fff;
    assert(IsInfinity(fp) && !IsNaN(fp));
    fp.f.mantissa |= 1;
    assert(IsNaN(fp) && IsSNaN(fp));
    fp.f.mantissa |= UINT64_C(0x4000000000000000);
    assert(!IsSNaN(fp));
    assert((bitop::bitcount2masklsb<32,0,uint32_t>() == UINT32_MAX));
    assert((bitop::bitcount2masklsb<4,4>() == 0xf0));
    assert((bitop::bitcount2maskmsb<4,4>() == 0x0f000000));
    assert(bitop::rounduppow2mask(0x7000U) == 0x7fff);
    assert(bitop::rounduppow2mask(0x2000U) == 0x3fff);
    assert(bitop::log2(0x80000000U) == 31);
    assert(PHYSPAGE_ACCESS_BITS == 0xc0000000U && PHYSPAGE_DIRTY == 0x20000000U);
    assert(htobe32(0x12345678U) == 0x78563412U);
    assert(htobe64(UINT64_C(0x0123456789abcdef)) == UINT64_C(0xefcdab8967452301));
    MEM_CalloutObject memory;
    IO_CalloutObject io;
    assert(!memory.installed && !memory.alloc && memory.m_handler == NULL);
    assert(!io.isInstalled() && !io.alloc && io.m_r_handler == NULL);
    puts("OPENSTEP core header tests passed");
    return 0;
}
