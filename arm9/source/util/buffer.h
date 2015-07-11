//memory buffers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/system.h>

#include "..\disk\stream_disk.h"

#ifdef __cplusplus 
extern "C" {
#endif

//extern volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4]; //genuine reference - wram that is 32bit per chunk / ori specs
extern volatile u32 gbawram[(256*1024)/4];

extern volatile u8   palram[0x400];
extern volatile u8 gbabios[0x4000];
extern volatile u8 gbaintram[0x8000];
extern volatile u8 gbaoam[0x400];
extern volatile u8 gbacaioMem[0x400];
extern volatile u8 iomem[0x400];
extern volatile u8 saveram[512*1024];

//disk buffer
extern volatile u32 disk_buf32[(sectorsize_int32units)]; 	//32 reads //0x80
extern volatile u16 disk_buf16[(sectorsize_int16units)];	//16 reads
extern volatile u8 disk_buf8[(sectorsize_int8units)];		//8 reads

//tests
extern volatile u32 tempbuffer[1024*1]; //1K test
extern volatile u32 tempbuffer2[1024*1]; //1K test

#ifdef __cplusplus
}
#endif