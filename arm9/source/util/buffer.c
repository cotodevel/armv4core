#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include "gbaemu4ds_fat_ext.h"
#include "buffer.h"

u8 * gbawram = NULL;	//[256*1024]; //genuine reference - wram that is 32bit per chunk
u8 * palram = NULL;	//[0x400];
u8 * gbabios = NULL;	//[0x4000];
u8 * gbaintram = NULL;	//[0x8000];
u8 * gbaoam = NULL;	//[0x400];
u8 * gbacaioMem = NULL;	//[0x400];
u8 * saveram = NULL;	//[128*1024]; //128K
u8 * iomem[0x400];

//disk buffer
volatile u32 disk_buf[chucksize]; 

u8 first32krom[1024*32];

//tests
u32 tempbuffer[1024*1]; //1K test
u32 tempbuffer2[1024*1]; //1K test
