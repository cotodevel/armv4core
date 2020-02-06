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

#ifndef __buffer_armv4core_h__
#define __buffer_armv4core_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
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
#include "gbaemu4ds_fat_ext.h"

#endif

#ifdef __cplusplus 
extern "C" {
#endif

extern u8 * gbawram;	//[256*1024]; //genuine reference - wram that is 32bit per chunk
extern u8 * palram;	//[0x400];
extern u8 * gbabios;	//[0x4000];
extern u8 * gbaintram;	//[0x8000];
extern u8 * gbaoam;	//[0x400];
extern u8 * gbacaioMem;	//[0x400];
extern u8 * saveram;	//[128*1024]; //128K
extern u8 * iomem[0x400];
extern u32 tempbuffer[1024*1]; //1K test
extern u32 tempbuffer2[1024*1]; //1K test
extern volatile u32 disk_buf[chucksize];
extern u8 first32krom[1024*32];

#ifdef __cplusplus
}
#endif
