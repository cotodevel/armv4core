/*
			Copyright (C) 2017  Coto
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
USA

*/

#ifndef __bios_armv4core_h__
#define __bios_armv4core_h__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();
#include <string.h>

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

//filesystem
#include "fatfslayerTGDS.h"
#include "InterruptsARMCores_h.h"
#include "ipcfifoTGDSUser.h"
#include "ff.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

#include "armv4coreFS.h"
#endif


#ifdef __cplusplus
extern "C"{
#endif

extern s16 sinetable[256];
extern u32 bios_cpureset();						//swi 0
extern u32 bios_registerramreset(u32); 			//swi 1
extern u32 bios_cpuhalt();							//swi 2
extern u32 bios_stopsleep();						//swi 3
extern u32 bios_div();								//swi 6
extern u32 bios_divarm();							//swi 7
extern u32 bios_sqrt();							//swi 8
extern u32 bios_arctan();							//swi 9
extern u32 bios_arctan2();							//swi 0xa
extern u32 bios_cpuset();							//swi 0xb
extern u32 bios_cpufastset();						//swi 0xc
extern u32 bios_getbioschecksum();					//swi 0xd
extern u32 bios_bgaffineset();						//swi 0xe
extern u32 bios_objaffineset();					//swi 0xf
extern u32 bios_bitunpack();						//swi 0x10
extern u32 bios_lz77uncompwram();					//swi 0x11
extern u32 bios_lz77uncompvram();					//swi 0x12
extern u32 bios_huffuncomp();						//swi 0x13
extern u32 bios_rluncompwram();					//swi 0x14
extern u32 bios_rluncompvram();					//swi 0x15
extern u32 bios_diff8bitunfilterwram();			//swi 0x16
extern u32 bios_diff8bitunfiltervram();			//swi 0x17
extern u32 bios_diff16bitunfilter();				//swi 0x18
extern u32 bios_midikey2freq();					//swi 0x1f
extern u32 bios_snddriverjmptablecopy();			//swi 0x2a

#ifdef __cplusplus
}
#endif