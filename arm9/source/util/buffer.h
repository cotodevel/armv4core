#ifndef bufferGBAdefs
#define bufferGBAdefs

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();

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
#include "gbaemu4ds_fat_ext.h"

#ifdef __cplusplus 
extern "C" {
#endif

extern volatile u8 gbawram[256*1024]; //genuine reference - wram that is 32bit per chunk
extern volatile u8 __attribute__ ((aligned (1))) palram[0x400];
extern volatile u8 __attribute__ ((aligned (1))) gbabios[0x4000];
extern volatile u8 __attribute__ ((aligned (1))) gbaintram[0x8000];
extern volatile u8 __attribute__ ((aligned (1))) gbaoam[0x400];
extern volatile u8 __attribute__ ((aligned (1))) gbacaioMem[0x400];
extern volatile u8 __attribute__ ((aligned (1))) iomem[0x400];
extern volatile u8 __attribute__ ((aligned (1))) saveram[128*1024]; //128K

//tests
extern u32 tempbuffer[1024*1]; //1K test
extern u32 tempbuffer2[1024*1]; //1K test


extern volatile u32 disk_buf[chucksize];
extern u8 first32krom[1024*32];

#ifdef __cplusplus
}
#endif

#endif