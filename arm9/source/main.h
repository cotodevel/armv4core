//arm9 main libs
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>
#include <assert.h>
#include <nds/arm9/dldi.h>
#include <nds/disc_io.h>



#ifndef GBARAMDEF
#define GBARAMDEF

//reflect this from ld
#define  ewram_start 	(0x02000000)
#define  ewram_size		((gbawram_start-0x02000000))
#define  gbawram_start	(0x02000000 + (4*(1024*1024)) - 0x4d000 - (1024*4))
#define  gbawram_size 	((256*1024))
#define  vectors_start  (0x00000000)
#define  vectors_size 	(1024*1)

#endif

#ifdef __cplusplus
extern "C" {
#endif

// The only built in driver
extern DLDI_INTERFACE _io_dldi_stub;


//linker (bin) objects
extern unsigned int rom_pl_bin;
extern unsigned int rom_pl_bin_size;
//extern unsigned int __ewram_start;
//extern unsigned int __ewram_end;

//stack info from ld
//__sp_svc	=	__dtcm_top - 0x08 - 0x400;
//__sp_irq	=	__sp_svc - 0x400;
//__sp_usr	=	__sp_irq - 0x400;
//__sp_abort	=	__sp_usr - 0x400;

extern u32 SPswi __attribute__((section(".vectors")));
extern u32 SPirq __attribute__((section(".vectors")));
extern u32 SPusr __attribute__((section(".vectors")));
extern u32 SPdabt __attribute__((section(".vectors")));

//DTCM TOP full memory
extern u32 dtcm_top_ld __attribute__((section(".dtcm")));
//DTCM TOP reserved by compiler/user memory
extern u32 dtcm_end_alloced __attribute__((section(".dtcm")));

typedef int (*intfuncptr)();
typedef u32 (*u32funcptr)();
typedef void (*voidfuncptr)();

extern u32 cpucore_tick;

extern bool dldiload;

#ifdef __cplusplus
}
#endif