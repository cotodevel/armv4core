#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>
//#include "../common/Types.h"

#include "buffer.h"

//volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4]; //ori specs
volatile u32 gbawram[(256*1024)/4];

volatile u8 palram[0x400];
volatile u8 gbabios[0x4000];
volatile u8 gbaintram[0x8000];
volatile u8 gbaoam[0x400];
volatile u8 gbacaioMem[0x400];
volatile u8 iomem[0x400]; //requires filling up! on util.c
volatile u8 saveram[512*1024]; //512K

//[nds] disk sector ram buffer
volatile u32 disk_buf32[(sectorsize_int32units)]; 	//32 reads //0x80
volatile u16 disk_buf16[(sectorsize_int16units)];	//16 reads
volatile u8 disk_buf8[(sectorsize_int8units)];	//16 reads

//const u32 __attribute__ ((aligned (4))) gbaheaderbuf[0x200/4]; //reads must be u32, but allocate the correct size (of missing GBAROM which is 0x200 bytes from start)

//tests
volatile u32 tempbuffer[1024*1]; //1K test
volatile u32 tempbuffer2[1024*1]; //1K test
