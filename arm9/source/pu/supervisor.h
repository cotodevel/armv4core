//this one handles ALL supervisor both stack references to ITCM/DTCM
//in this priority: (NDS<->GBA)

//all switching functions, IRQ assign, interrupt request, BIOS Calls assignment
//assignments should flow through here

//512 * 2 ^ n
//base address / size (bytes) for DTCM (512<<n )

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

//arm9 main libs
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

//GBA stacks
extern u8 gbastck_usr[0x200];
extern u8 gbastck_fiq[0x200];
extern u8 gbastck_irq[0x200];
extern u8 gbastck_svc[0x200];
extern u8 gbastck_abt[0x200];
extern u8 gbastck_und[0x200];
//extern u8 gbastck_sys[0x200]; //shared with usr

//GBA stack notes: sys is shared with usr

//and cpu<mode> all the other backup registers when restoring from FIQ r8-r12
extern u32  exRegs_cpubup[0x5];


//////////////////////////////////////////////////////////////////////////////////////

//GBA Interrupts
//4000200h - GBAIE - Interrupt Enable Register (R/W)
//  Bit   Expl.
//  0     LCD V-Blank                    (0=Disable)
//  1     LCD H-Blank                    (etc.)
//  2     LCD V-Counter Match            (etc.)
//  3     Timer 0 Overflow               (etc.)
//  4     Timer 1 Overflow               (etc.)
//  5     Timer 2 Overflow               (etc.)
//  6     Timer 3 Overflow               (etc.)
//  7     Serial Communication           (etc.)
//  8     DMA 0                          (etc.)
//  9     DMA 1                          (etc.)
//  10    DMA 2                          (etc.)
//  11    DMA 3                          (etc.)
//  12    Keypad                         (etc.)
//  13    Game Pak (external IRQ source) (etc.)

//virtual environment Interruptable Bits

//CPUtotaltick
extern u32  cputotalticks;

#ifdef __cplusplus
extern "C" {
#endif

//swi
extern u32 swi_virt(u32 swinum);

//IO GBA (virtual < -- > hardware) handlers
extern u32 gbacpu_refreshvcount();	//CPUCompareVCOUNT(gba);
extern u32 cpu_updateregisters(u32 address, u16 value);		//CPUUpdateRegister(0xn, 0xnn);

//IRQ
extern u32 irqbiosinst();

//other
extern int sqrtasm(int);
extern u32 wramtstasm(u32 address,u32 top);
extern u32 debuggeroutput();
extern u32 branchtoaddr(u32 value,u32 address);
extern u32 video_render(u32 * gpubuffer);
extern u32 sound_render();
extern void save_thread(u32 * srambuf);

// NDS Interrupts

//r0    0=Return immediately if an old flag was already set (NDS9: bugged!)
//      1=Discard old flags, wait until a NEW flag becomes set
//r1    Interrupt flag(s) to wait for (same format as GBAIE/IF registers)
extern u32 nds9intrwait(u32 behaviour, u32 GBAIF);
extern u32 setirq(u32 irqtoset);

// end NDS Interrupts

extern u32 cpuloop(int ticks);
extern int cpuupdateticks();
extern u32 cpuirq(u32 cpumode);
extern u32 systemreadjoypad(int which);

//fetch
extern u32 armnextpc(u32 address);
extern u32 armfetchpc(u32 address);

//nds threads
extern void vblank_thread();
extern void hblank_thread();
extern void vcount_thread();
extern void fifo_thread();

//gba threads
extern u32 gbavideorender();

#ifdef __cplusplus
}
#endif