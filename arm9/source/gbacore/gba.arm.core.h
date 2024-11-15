#ifndef armv4coregbaarmcore
#define armv4coregbaarmcore

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#define bios (u8*)(gbabios)
#define internalRAM (u8*)(gbaintram)
#define workRAM (u8*)(gbawram)
#define paletteRAM (u8*)(palram)
#define gba_vram (u8*)(0x06200000)	//VRAM_C_0x06200000_ENGINE_B_BG;	//NDS BMP rgb15 mode + keyboard -> see TGDS gui_console_connector.c
#define oam (u8*)(gbaoam)

typedef struct{
	u32 Version;
	u32 listentr;
} __attribute__ ((__packed__)) patch_t;

typedef struct{
	u32 gamecode;
	u8 homebrew;
	u64 crc;
	char patchpath[512 * 2];
	u8 swaplcd;
	u8 savfetype;
	u8 frameskip;
	u8 frameskipauto;
	u16 frameline;
	u8 fastpu;
	u8 mb;
	u8 loadertype;
} __attribute__ ((__packed__)) patch2_t;
	
struct DirEntry {
	char name[512];
	bool isDirectory;
};

typedef int (*intfuncptr)();
typedef u32 (*u32funcptr)();
typedef void (*voidfuncptr)();

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern patch_t patchheader;

//GBAProcess
extern void cpu_fdexecute();
extern u32 cpu_calculate();	//todo
extern u32 cpuirq(u32 cpumode);

//IO GBA (virtual < -- > hardware) handlers
extern u32 ndsvcounter();

//GBACore

//GBA CPU mode registers:
//r0 - r15 	-- 	0x0 - 0xf
//CPSR 		--	0x10
extern u32  exRegs[16 + 1];

//each sp,lr for cpu<mode>
extern u32  exRegs_r13usr[0x1];
extern u32  exRegs_r14usr[0x1];
extern u32  exRegs_r13fiq[0x1];
extern u32  exRegs_r14fiq[0x1];
extern u32  exRegs_r13irq[0x1];
extern u32  exRegs_r14irq[0x1];
extern u32  exRegs_r13svc[0x1];
extern u32  exRegs_r14svc[0x1];
extern u32  exRegs_r13abt[0x1];
extern u32  exRegs_r14abt[0x1];
extern u32  exRegs_r13und[0x1];
extern u32  exRegs_r14und[0x1];
//extern u32  exRegs_r14sys[0x1]; //usr/sys uses same stacks
//extern u32  exRegs_r14sys[0x1];

//original registers used by any PSR_MODE that do belong to FIQ r8-r12
extern u32  exRegs_fiq[0x5];	//+2 fiq regs from above

//"banked" SPSR register for every PSR mode, except the USR/SYS psr, which has NOT a SPSR and read/writes to it everywhere (from/to CPSR directly or through opcodes) are ignored
extern u32  SPSR_fiq[0x1];
extern u32  SPSR_svc[0x1];
extern u32  SPSR_abt[0x1];
extern u32  SPSR_irq[0x1];
extern u32  SPSR_und[0x1];

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

extern bool ioReadable[0x400];
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

extern u8 ioMem[0x400];
extern u16 GBAIE;
extern u16 GBAIF;
extern u16 GBAIME;

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
extern bool windowOn;
extern int frameCount ;
extern char buffer[1024];

//input
extern u32 systemreadjoypad(int which);

extern u8 cpuBitsSet[256];
extern u8 cpuLowestBitSet[256];

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
extern int hblank_ticks;
extern void  CPUUpdateRegister(u32 address, u16 value);
extern void  CPUCheckDMA(int reason, int dmamask);
extern void  doDMA(u32 * s, u32 * d, u32 si, u32 di, u32 c, int transfer32);
extern int romSize;
extern int   sound_clock_ticks;
extern int   soundtTicks;

extern u16 GBADISPSTAT;
extern u16 GBAVCOUNT;
extern u16 GBABG0CNT;
extern u16 GBABG1CNT;
extern u16 GBABG2CNT;
extern u16 GBABG3CNT;
extern u16 GBABG0HOFS;
extern u16 GBABG0VOFS;
extern u16 GBABG1HOFS;
extern u16 GBABG1VOFS;
extern u16 GBABG2HOFS;
extern u16 GBABG2VOFS;
extern u16 GBABG3HOFS;
extern u16 GBABG3VOFS;
extern u16 GBABG2PA;
extern u16 GBABG2PB;
extern u16 GBABG2PC;
extern u16 GBABG2PD;
extern u16 GBABG2X_L;
extern u16 GBABG2X_H;
extern u16 GBABG2Y_L;
extern u16 GBABG2Y_H;
extern u16 GBABG3PA;
extern u16 GBABG3PB;
extern u16 GBABG3PC;
extern u16 GBABG3PD;
extern u16 GBABG3X_L;
extern u16 GBABG3X_H;
extern u16 GBABG3Y_L;
extern u16 GBABG3Y_H;
extern u16 GBAWIN0H;
extern u16 GBAWIN1H;
extern u16 GBAWIN0V;
extern u16 GBAWIN1V;
extern u16 GBAWININ;
extern u16 GBAWINOUT;
extern u16 GBAMOSAIC;
extern u16 GBABLDMOD;
extern u16 GBACOLEV;
extern u16 GBACOLY;
extern u16 GBADM0SAD_L;
extern u16 GBADM0SAD_H;
extern u16 GBADM0DAD_L;
extern u16 GBADM0DAD_H;
extern u16 GBADM0CNT_L;
extern u16 GBADM0CNT_H;
extern u16 GBADM1SAD_L;
extern u16 GBADM1SAD_H;
extern u16 GBADM1DAD_L;
extern u16 GBADM1DAD_H;
extern u16 GBADM1CNT_L;
extern u16 GBADM1CNT_H;
extern u16 GBADM2SAD_L;
extern u16 GBADM2SAD_H;
extern u16 GBADM2DAD_L;
extern u16 GBADM2DAD_H;
extern u16 GBADM2CNT_L;
extern u16 GBADM2CNT_H;
extern u16 GBADM3SAD_L;
extern u16 GBADM3SAD_H;
extern u16 GBADM3DAD_L;
extern u16 GBADM3DAD_H;
extern u16 GBADM3CNT_L;
extern u16 GBADM3CNT_H;
extern u16 GBATM0D;
extern u16 GBATM0CNT;
extern u16 GBATM1D;
extern u16 GBATM1CNT;
extern u16 GBATM2D;
extern u16 GBATM2CNT;
extern u16 GBATM3D;
extern u16 GBATM3CNT;
extern u16 GBAP1;
extern u16 GBADISPCNT;
extern u16 GBAVCOUNT;

extern u8 memoryWait[16];
extern u8 memoryWait32[16];
extern u8 memoryWaitSeq[16];
extern u8 memoryWaitSeq32[16];

extern bool cpuStart;

#ifdef __cplusplus
}
#endif

