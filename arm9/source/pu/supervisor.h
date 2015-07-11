//this one handles ALL supervisor both stack references to ITCM/DTCM
//in this priority: (NDS<->GBA)

//all switching functions, IRQ assign, interrupt request, BIOS Calls assignment
//assignments should flow through here

//512 * 2 ^ n
//base address / size (bytes) for DTCM (512<<n )

//arm9 main libs
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
//#include <dswifi9.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif


//IRQ
extern u32 irqbiosinst();

//other
int sqrtasm(int);
u32 wramtstasm(u32 address,u32 top);
u32 debuggeroutput();
u32 branchtoaddr(u32 value,u32 address);
u32 video_render(u32 * gpubuffer);
u32 sound_render();
void save_thread(u32 * srambuf);

//nds threads
void vblank_thread();
void hblank_thread();
void vcount_thread();
void fifo_thread();

//gba threads
u32 gbavideorender(u32 address, u32 offset);

#ifdef __cplusplus
}
#endif