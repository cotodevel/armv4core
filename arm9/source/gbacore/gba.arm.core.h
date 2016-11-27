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

//NDS7 clock
//#include <time.h> 

//GBASystem struct declaration
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

//GBAProcess
u32 cpu_fdexecute();
u32 cpu_calculate();
u32 cpuirq(u32 cpumode);

//IO GBA (virtual < -- > hardware) handlers
u32 ndsvcounter();

//GBACore
extern u32  gbachunk;		//gba read chunk
extern u32  dummyreg;		//any purpose destroyable 4 byte dtcm chunk
extern u32  dummyreg2;		//any purpose destroyable 4 byte dtcm chunk
extern u32  dummyreg3;		//any purpose destroyable 4 byte dtcm chunk
extern u32  dummyreg4;		//any purpose destroyable 4 byte dtcm chunk
extern u32  dummyreg5;		//any purpose destroyable 4 byte dtcm chunk
extern u32  bios_irqhandlerstub_C;	//irq handler word aligned pointer for ARM9

//gba virtualized r0-r15 registers
extern u32  exRegs[0x10]; //placeholder for actual CPU mode registers

//each sp,lr for cpu<mode>
extern u32  gbavirtreg_r13usr[0x1];
extern u32  gbavirtreg_r14usr[0x1];
extern u32  gbavirtreg_r13fiq[0x1];
extern u32  gbavirtreg_r14fiq[0x1];
extern u32  gbavirtreg_r13irq[0x1];
extern u32  gbavirtreg_r14irq[0x1];
extern u32  gbavirtreg_r13svc[0x1];
extern u32  gbavirtreg_r14svc[0x1];
extern u32  gbavirtreg_r13abt[0x1];
extern u32  gbavirtreg_r14abt[0x1];
extern u32  gbavirtreg_r13und[0x1];
extern u32  gbavirtreg_r14und[0x1];
//extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1]; //usr/sys uses same stacks
//extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1];

//original registers used by any PSR_MODE that do belong to FIQ r8-r12
extern u32  gbavirtreg_fiq[0x5];

//original registers used by any PSR_MODE that do not belong to FIQ r8-r12
extern u32  gbavirtreg_nonfiq[0x5];


//extern'd addresses for virtualizer calls
// [(u32)&disthumbcode	] 
// [(u32)&disasmcode	]
// [					]
// [					]
// [					]
// [					]
// [					]
// [					]

//GBA Interrupts
//4000200h - IE - Interrupt Enable Register (R/W)
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

extern volatile u16 DISPCNT;

extern memoryMap map[256];
extern bool ioReadable[0x400];
extern bool N_FLAG ;
extern bool C_FLAG ;
extern bool Z_FLAG ;
extern bool V_FLAG ;
extern bool armState ;
extern bool armIrqEnable ;
extern u32 armNextPC ;
extern int armMode ;
extern u32 stop ;
extern int saveType ;
extern bool useBios ;
extern bool skipBios ;
extern int frameSkip ;
extern bool speedup ;
extern bool synchronize ;
extern bool cpuDisableSfx ;
extern bool cpuIsMultiBoot ;
extern bool parseDebug ;
extern int layerSettings;
extern int layerEnable ;
extern bool speedHack ;
extern int cpuSaveType ;
extern bool cheatsEnabled ;
extern bool mirroringEnable;

extern u8 *bios; //calloc
extern u8 *rom; //calc

extern u8 *internalRAM; 
extern u8 *workRAM ;
extern u8 *paletteRAM;

extern u8 *vram;
extern u8 *oam ;

extern u8 ioMem[0x400];

extern u16 DISPSTAT ;
extern u16 VCOUNT   ;
extern u16 BG0CNT   ;
extern u16 BG1CNT   ;
extern u16 BG2CNT   ;
extern u16 BG3CNT   ;
extern u16 BG0HOFS  ;
extern u16 BG0VOFS  ;
extern u16 BG1HOFS  ;
extern u16 BG1VOFS  ;
extern u16 BG2HOFS  ;
extern u16 BG2VOFS  ;
extern u16 BG3HOFS  ;
extern u16 BG3VOFS  ;
extern u16 BG2PA    ;
extern u16 BG2PB    ;
extern u16 BG2PC    ;
extern u16 BG2PD    ;
extern u16 BG2X_L   ;
extern u16 BG2X_H   ;
extern u16 BG2Y_L   ;
extern u16 BG2Y_H   ;
extern u16 BG3PA    ;
extern u16 BG3PB    ;
extern u16 BG3PC    ;
extern u16 BG3PD    ;
extern u16 BG3X_L   ;
extern u16 BG3X_H   ;
extern u16 BG3Y_L   ;
extern u16 BG3Y_H   ;
extern u16 WIN0H    ;
extern u16 WIN1H    ;
extern u16 WIN0V    ;
extern u16 WIN1V    ;
extern u16 WININ    ;
extern u16 WINOUT   ;
extern u16 MOSAIC   ;
extern u16 BLDMOD   ;
extern u16 COLEV    ;
extern u16 COLY     ;
extern u16 DM0SAD_L ;
extern u16 DM0SAD_H ;
extern u16 DM0DAD_L ;
extern u16 DM0DAD_H ;
extern u16 DM0CNT_L ;
extern u16 DM0CNT_H ;
extern u16 DM1SAD_L ;
extern u16 DM1SAD_H ;
extern u16 DM1DAD_L ;
extern u16 DM1DAD_H ;
extern u16 DM1CNT_L ;
extern u16 DM1CNT_H ;
extern u16 DM2SAD_L ;
extern u16 DM2SAD_H ;
extern u16 DM2DAD_L ;
extern u16 DM2DAD_H ;
extern u16 DM2CNT_L ;
extern u16 DM2CNT_H ;
extern u16 DM3SAD_L ;
extern u16 DM3SAD_H ;
extern u16 DM3DAD_L ;
extern u16 DM3DAD_H ;
extern u16 DM3CNT_L ;
extern u16 DM3CNT_H ;
extern u16 TM0D   ;
extern u16 TM0CNT ;
extern u16 TM1D   ;
extern u16 TM1CNT ;
extern u16 TM2D   ;
extern u16 TM2CNT ;
extern u16 TM3D   ;
extern u16 TM3CNT ;
extern u16 P1 ;
extern u16 IE ;
extern u16 IF ;
extern u16 IME;

//gba arm core variables
extern int SWITicks;
extern int IRQTicks;

extern int layerEnableDelay;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern u32 busPrefetchCount;
extern int cpuDmaTicksToUpdate;
extern int cpuDmaCount;
extern bool cpuDmaHack;
extern u32 cpuDmaLast;
extern int dummyAddress;
extern bool cpuBreakLoop;
extern int cpuNextEvent;

extern int gbaSaveType; // used to remember the save type on reset
extern bool intState ;
extern bool stopState ;
extern bool holdState ;
extern int holdType ;
extern bool cpuSramEnabled ;
extern bool cpuFlashEnabled ;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;

extern u32 cpuPrefetch[2];

extern int cpuTotalTicks;                  //cycle count for each opcode processed

extern int lcdTicks;

extern u8 timerOnOffDelay;
extern u16 timer0Value;
extern bool timer0On ;
extern int timer0Ticks ;
extern int timer0Reload;
extern int timer0ClockReload ;
extern u16 timer1Value;
extern bool timer1On ;
extern int timer1Ticks;
extern int timer1Reload;
extern int timer1ClockReload;
extern u16 timer2Value;
extern bool timer2On ;
extern int timer2Ticks;
extern int timer2Reload;
extern int timer2ClockReload ;
extern u16 timer3Value;
extern bool timer3On ;
extern int timer3Ticks ;
extern int timer3Reload ;
extern int timer3ClockReload ;
extern u32 dma0Source ;
extern u32 dma0Dest ;
extern u32 dma1Source ;
extern u32 dma1Dest ;
extern u32 dma2Source ;
extern u32 dma2Dest ;
extern u32 dma3Source ;
extern u32 dma3Dest ;

extern void (*cpuSaveGameFunc)(u32,u8);
//void (*renderLine)() = mode0RenderLine;
extern bool fxOn ;
extern bool windowOn;
extern int frameCount ;
extern char buffer[1024];
extern u8 biosProtected[4];

//thread vectors (for ASM irq)

//input
u32 systemreadjoypad(int which);

//fetch
u32 armnextpc(u32 address);
u32 armfetchpc_arm(u32 address);
u16 armfetchpc_thumb(u32 address);

//swi
u32 swi_virt(u32 swinum);

//LUT that updates the base index depending on the opcode base
extern u8 cpuBitsSet[256];
extern u8 cpuLowestBitSet[256];

//CPU GBA:
extern s16 CPUReadHalfWordSigned_stack(u32 address);
extern s8 CPUReadByteSigned_stack(u32 address);

extern s16 CPUReadHalfWordSigned(u32 address);
extern s8 CPUReadByteSigned(u32 address);

extern u8 CPUReadByte(u32 address);
extern u16 CPUReadHalfWord(u32 address);
extern u32 CPUReadMemory(u32 address);

extern u8 CPUReadByte_stack(u32 address);
extern u16 CPUReadHalfWord_stack(u32 address);
extern u32 CPUReadMemory_stack(u32 address);

extern void CPUWriteByte(u32 address, u8 value);
extern void CPUWriteHalfWord(u32 address, u16 value);
extern void CPUWriteMemory(u32 address, u32 value);

extern void CPUWriteByte_stack(u32 address, u8 value);
extern void CPUWriteHalfWord_stack(u32 address, u16 value);
extern void CPUWriteMemory_stack(u32 address, u32 value);

//stacked is safe but slow (we dont want speed but accuracy for this debugger)
extern u8 gbawram[(256*1024)];
extern u8 palram[0x400];
extern u8 gbabios[0x4000];
extern u8 gbaintram[0x8000];
extern u8 gbaoam[0x400];
extern u8 gbacaioMem[0x400];
//extern u8 iomem[0x400]; //already defined
extern u8 saveram[512*1024]; //512K

extern int hblank_ticks;

extern void  CPUUpdateRegister(u32 address, u16 value);
extern void  CPUCheckDMA(int reason, int dmamask);
extern void  doDMA(u32 * s, u32 * d, u32 si, u32 di, u32 c, int transfer32);

extern u32 myROM[173];

extern int romSize;

#ifdef __cplusplus
}
#endif