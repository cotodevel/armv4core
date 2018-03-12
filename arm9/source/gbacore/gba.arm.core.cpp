#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include "gba.arm.core.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

//NDS7 clock
#include <time.h> 
#include "clock.h"
#include "gba_ipc.h"


//emu clock
#include "RTC.h"

#include "supervisor.h"
#include "pu.h"
#include "opcode.h"
#include "Util.h"
#include "translator.h"
#include "bios.h"

//filesystem
#include "fsfatlayerTGDS.h"
#include "fileHandleTGDS.h"
#include "InterruptsARMCores_h.h"
#include "specific_shared.h"
#include "ff.h"
#include "memoryHandleTGDS.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"

//player instance
#include "main.h"

//save
#include "savechip.h"

#include "EEprom.h"
#include "Flash.h"
#include "Sram.h"
#include "System.h"

#include "settings.h"
#include "buffer.h"
#include "keypadTGDS.h"

//the gbasystem global instance
struct GBASystem gbaSystemGlobal;


//volatile u32 __attribute__((section(".gbawram"))) __attribute__ ((aligned (4))) gbawram[(256*1024)/4]; //ori specs
/*
u8 gbawram[(256*1024)];
u8 palram[0x400];
u8 gbabios[0x4000];
u8 gbaintram[0x8000];
u8 gbaoam[0x400];
u8 gbacaioMem[0x400];
//u8 iomem[0x400];  //already defined
u8 saveram[512*1024]; //sram 512K
*/

//gba vars
//volatile u16 gbaSystemGlobal.GBADISPCNT;

__attribute__((section(".dtcm")))
memoryMap map[256];
__attribute__((section(".dtcm")))
bool ioReadable[0x400];
__attribute__((section(".dtcm")))
bool N_FLAG = 0;
__attribute__((section(".dtcm")))
bool C_FLAG = 0;
__attribute__((section(".dtcm")))
bool Z_FLAG = 0;
__attribute__((section(".dtcm")))
bool V_FLAG = 0;
__attribute__((section(".dtcm")))

__attribute__((section(".dtcm")))
bool armState = true;               //true = ARM / false = thumb
__attribute__((section(".dtcm")))
bool armIrqEnable = true;
__attribute__((section(".dtcm")))
u32 armNextPC = 0x00000000;
__attribute__((section(".dtcm")))
int armMode = 0x1f;
__attribute__((section(".dtcm")))
u32 stop = 0x08000568;              //stop address
__attribute__((section(".dtcm")))
int saveType = 0;
__attribute__((section(".dtcm")))
bool useBios = false;
__attribute__((section(".dtcm")))
bool skipBios = false;
__attribute__((section(".dtcm")))
int frameSkip = 1;
__attribute__((section(".dtcm")))
bool speedup = false;
__attribute__((section(".dtcm")))
bool synchronize = true;
__attribute__((section(".dtcm")))
bool cpuDisableSfx = false;
__attribute__((section(".dtcm")))
bool cpuIsMultiBoot = false;
__attribute__((section(".dtcm")))
bool parseDebug = true;
__attribute__((section(".dtcm")))
int layerSettings = 0xff00;
__attribute__((section(".dtcm")))
int layerEnable = 0xff00;
__attribute__((section(".dtcm")))
bool speedHack = false;
__attribute__((section(".dtcm")))
int cpuSaveType = 0;
__attribute__((section(".dtcm")))
bool cheatsEnabled = true;
__attribute__((section(".dtcm")))
bool mirroringEnable = false;

__attribute__((section(".dtcm")))
u8 *bios = (uint8*)gbabios; 

__attribute__((section(".dtcm")))
u8 *internalRAM = (u8*)gbaintram; 
__attribute__((section(".dtcm")))
u8 *workRAM = (u8*)gbawram;

__attribute__((section(".dtcm")))
//u8 *paletteRAM = (u8*)palram;
u8 *paletteRAM = (u8*)0x05000000;

__attribute__((section(".dtcm")))
u8 *vram = (u8*)0x06000000;

__attribute__((section(".dtcm")))
//u8 *oam = (u8*)gbaoam;
u8 *oam = (u8*)0x07000000;

__attribute__((section(".dtcm")))
u8 ioMem[0x400];

/*
u16 gbaSystemGlobal.GBADISPSTAT = 0x0000;
u16 gbaSystemGlobal.GBAVCOUNT   = 0x0000;
u16 gbaSystemGlobal.GBABG0CNT   = 0x0000;
u16 gbaSystemGlobal.GBABG1CNT   = 0x0000;
u16 gbaSystemGlobal.GBABG2CNT   = 0x0000;
u16 gbaSystemGlobal.GBABG3CNT   = 0x0000;
u16 gbaSystemGlobal.GBABG0HOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG0VOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG1HOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG1VOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG2HOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG2VOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG3HOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG3VOFS  = 0x0000;
u16 gbaSystemGlobal.GBABG2PA    = 0x0100;
u16 gbaSystemGlobal.GBABG2PB    = 0x0000;
u16 gbaSystemGlobal.GBABG2PC    = 0x0000;
u16 gbaSystemGlobal.GBABG2PD    = 0x0100;
u16 gbaSystemGlobal.GBABG2X_L   = 0x0000;
u16 gbaSystemGlobal.GBABG2X_H   = 0x0000;
u16 gbaSystemGlobal.GBABG2Y_L   = 0x0000;
u16 gbaSystemGlobal.GBABG2Y_H   = 0x0000;
u16 gbaSystemGlobal.GBABG3PA    = 0x0100;
u16 gbaSystemGlobal.GBABG3PB    = 0x0000;
u16 gbaSystemGlobal.GBABG3PC    = 0x0000;
u16 gbaSystemGlobal.GBABG3PD    = 0x0100;
u16 gbaSystemGlobal.GBABG3X_L   = 0x0000;
u16 gbaSystemGlobal.GBABG3X_H   = 0x0000;
u16 gbaSystemGlobal.GBABG3Y_L   = 0x0000;
u16 gbaSystemGlobal.GBABG3Y_H   = 0x0000;
u16 gbaSystemGlobal.GBAWIN0H    = 0x0000;
u16 gbaSystemGlobal.GBAWIN1H    = 0x0000;
u16 gbaSystemGlobal.GBAWIN0V    = 0x0000;
u16 gbaSystemGlobal.GBAWIN1V    = 0x0000;
u16 gbaSystemGlobal.GBAWININ    = 0x0000;
u16 gbaSystemGlobal.GBAWINOUT   = 0x0000;
u16 gbaSystemGlobal.GBAMOSAIC   = 0x0000;
u16 gbaSystemGlobal.GBABLDMOD   = 0x0000;
u16 gbaSystemGlobal.GBACOLEV    = 0x0000;
u16 gbaSystemGlobal.GBACOLY     = 0x0000;
u16 gbaSystemGlobal.GBADM0SAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM0SAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM0DAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM0DAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM0CNT_L = 0x0000;
u16 gbaSystemGlobal.GBADM0CNT_H = 0x0000;
u16 gbaSystemGlobal.GBADM1SAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM1SAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM1DAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM1DAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM1CNT_L = 0x0000;
u16 gbaSystemGlobal.GBADM1CNT_H = 0x0000;
u16 gbaSystemGlobal.GBADM2SAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM2SAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM2DAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM2DAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM2CNT_L = 0x0000;
u16 gbaSystemGlobal.GBADM2CNT_H = 0x0000;
u16 gbaSystemGlobal.GBADM3SAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM3SAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM3DAD_L = 0x0000;
u16 gbaSystemGlobal.GBADM3DAD_H = 0x0000;
u16 gbaSystemGlobal.GBADM3CNT_L = 0x0000;
u16 gbaSystemGlobal.GBADM3CNT_H = 0x0000;
u16 gbaSystemGlobal.GBATM0D     = 0x0000;
u16 gbaSystemGlobal.GBATM0CNT   = 0x0000;
u16 gbaSystemGlobal.GBATM1D     = 0x0000;
u16 gbaSystemGlobal.GBATM1CNT   = 0x0000;
u16 gbaSystemGlobal.GBATM2D     = 0x0000;
u16 gbaSystemGlobal.GBATM2CNT   = 0x0000;
u16 gbaSystemGlobal.GBATM3D     = 0x0000;
u16 gbaSystemGlobal.GBATM3CNT   = 0x0000;
u16 gbaSystemGlobal.GBAP1       = 0xFFFF;

*/

u16 GBAIE       = 0x0000;
u16 GBAIF       = 0x0000;
u16 GBAIME      = 0x0000;

//gba arm core variables
__attribute__((section(".dtcm")))
int SWITicks = 0;
__attribute__((section(".dtcm")))
int IRQTicks = 0;

__attribute__((section(".dtcm")))
int layerEnableDelay = 0;
__attribute__((section(".dtcm")))
bool busPrefetch = false;
__attribute__((section(".dtcm")))
bool busPrefetchEnable = false;
__attribute__((section(".dtcm")))
u32 busPrefetchCount = 0;
__attribute__((section(".dtcm")))
int cpuDmaTicksToUpdate = 0;
__attribute__((section(".dtcm")))
int cpuDmaCount = 0;
__attribute__((section(".dtcm")))
bool cpuDmaHack = false;
__attribute__((section(".dtcm")))
u32 cpuDmaLast = 0;
__attribute__((section(".dtcm")))
int dummyAddress = 0;
__attribute__((section(".dtcm")))
bool cpuBreakLoop = false;
__attribute__((section(".dtcm")))
int cpuNextEvent = 0;

__attribute__((section(".dtcm")))
int gbaSaveType = 0; // used to remember the save type on reset
__attribute__((section(".dtcm")))
bool intState = false;
__attribute__((section(".dtcm")))
bool stopState = false;
__attribute__((section(".dtcm")))
bool holdState = false;
__attribute__((section(".dtcm")))
int holdType = 0;
__attribute__((section(".dtcm")))
bool cpuSramEnabled = true;
__attribute__((section(".dtcm")))
bool cpuFlashEnabled = true;
__attribute__((section(".dtcm")))
bool cpuEEPROMEnabled = true;
__attribute__((section(".dtcm")))
bool cpuEEPROMSensorEnabled = false;

__attribute__((section(".dtcm")))
u32 cpuPrefetch[2];

__attribute__((section(".dtcm")))
int cpuTotalTicks = 0;                  //cycle count for each opcode processed
#ifdef PROFILING
int profilingTicks = 0;
int profilingTicksReload = 0;
static profile_segment *profilSegment = NULL;
#endif

__attribute__((section(".dtcm")))
int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
__attribute__((section(".dtcm")))
u8 timerOnOffDelay = 0;
__attribute__((section(".dtcm")))
u16 timer0Value = 0;
__attribute__((section(".dtcm")))
bool timer0On = false;
__attribute__((section(".dtcm")))
int timer0Ticks = 0;
__attribute__((section(".dtcm")))
int timer0Reload = 0;
__attribute__((section(".dtcm")))
int timer0ClockReload  = 0;
__attribute__((section(".dtcm")))
u16 timer1Value = 0;
__attribute__((section(".dtcm")))
bool timer1On = false;
__attribute__((section(".dtcm")))
int timer1Ticks = 0;
__attribute__((section(".dtcm")))
int timer1Reload = 0;
__attribute__((section(".dtcm")))
int timer1ClockReload  = 0;
__attribute__((section(".dtcm")))
u16 timer2Value = 0;
__attribute__((section(".dtcm")))
bool timer2On = false;
__attribute__((section(".dtcm")))
int timer2Ticks = 0;
__attribute__((section(".dtcm")))
int timer2Reload = 0;
__attribute__((section(".dtcm")))
int timer2ClockReload  = 0;
__attribute__((section(".dtcm")))
u16 timer3Value = 0;
__attribute__((section(".dtcm")))
bool timer3On = false;
__attribute__((section(".dtcm")))
int timer3Ticks = 0;
__attribute__((section(".dtcm")))
int timer3Reload = 0;
__attribute__((section(".dtcm")))
int timer3ClockReload  = 0;
__attribute__((section(".dtcm")))
u32 dma0Source = 0;
__attribute__((section(".dtcm")))
u32 dma0Dest = 0;
__attribute__((section(".dtcm")))
u32 dma1Source = 0;
__attribute__((section(".dtcm")))
u32 dma1Dest = 0;
__attribute__((section(".dtcm")))
u32 dma2Source = 0;
__attribute__((section(".dtcm")))
u32 dma2Dest = 0;
__attribute__((section(".dtcm")))
u32 dma3Source = 0;
__attribute__((section(".dtcm")))
u32 dma3Dest = 0;

void (*cpuSaveGameFunc)(u32,u8) = flashSaveDecide;
//void (*renderLine)() = mode0RenderLine;
__attribute__((section(".dtcm")))
bool fxOn = false;
__attribute__((section(".dtcm")))
bool windowOn = false;
__attribute__((section(".dtcm")))
int frameCount = 0;
__attribute__((section(".dtcm")))
char buffer[1024];

FILE *out = NULL;

//u32 lastTime = 0;
__attribute__((section(".dtcm")))
int count = 0;

__attribute__((section(".dtcm")))
int capture = 0;
__attribute__((section(".dtcm")))
int capturePrevious = 0;
__attribute__((section(".dtcm")))
int captureNumber = 0;

__attribute__((section(".dtcm")))
int TIMER_TICKS[4] = {
  0,
  6,
  8,
  10
};

__attribute__((section(".dtcm")))
u32  objTilesAddress [3] = {0x010000, 0x014000, 0x014000};
__attribute__((section(".dtcm")))
u8 gamepakRamWaitState[4] = { 4, 3, 2, 8 };
__attribute__((section(".dtcm")))
u8 gamepakWaitState[4] =  { 4, 3, 2, 8 };
__attribute__((section(".dtcm")))
u8 gamepakWaitState0[2] = { 2, 1 };
__attribute__((section(".dtcm")))
u8 gamepakWaitState1[2] = { 4, 1 };
__attribute__((section(".dtcm")))
u8 gamepakWaitState2[2] = { 8, 1 };

__attribute__((section(".dtcm")))
u8 memoryWait[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 };
__attribute__((section(".dtcm")))
u8 memoryWait32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };
__attribute__((section(".dtcm")))
u8 memoryWaitSeq[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 };
__attribute__((section(".dtcm")))
u8 memoryWaitSeq32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const u8 videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

__attribute__((section(".dtcm")))
u8 biosProtected[4];
__attribute__((section(".dtcm")))
u32 myROM[173] = {
0xEA000006,
0xEA000093,
0xEA000006,
0x00000000,
0x00000000,
0x00000000,
0xEA000088,
0x00000000,
0xE3A00302,
0xE1A0F000,
0xE92D5800,
0xE55EC002,
0xE28FB03C,
0xE79BC10C,
0xE14FB000,
0xE92D0800,
0xE20BB080,
0xE38BB01F,
0xE129F00B,
0xE92D4004,
0xE1A0E00F,
0xE12FFF1C,
0xE8BD4004,
0xE3A0C0D3,
0xE129F00C,
0xE8BD0800,
0xE169F00B,
0xE8BD5800,
0xE1B0F00E,
0x0000009C,
0x0000009C,
0x0000009C,
0x0000009C,
0x000001F8,
0x000001F0,
0x000000AC,
0x000000A0,
0x000000FC,
0x00000168,
0xE12FFF1E,
0xE1A03000,
0xE1A00001,
0xE1A01003,
0xE2113102,
0x42611000,
0xE033C040,
0x22600000,
0xE1B02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE1A01000,
0xE1A00003,
0xE1B0C08C,
0x22600000,
0x42611000,
0xE12FFF1E,
0xE92D0010,
0xE1A0C000,
0xE3A01001,
0xE1500001,
0x81A000A0,
0x81A01081,
0x8AFFFFFB,
0xE1A0000C,
0xE1A04001,
0xE3A03000,
0xE1A02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE0811003,
0xE1B010A1,
0xE1510004,
0x3AFFFFEE,
0xE1A00004,
0xE8BD0010,
0xE12FFF1E,
0xE0010090,
0xE1A01741,
0xE2611000,
0xE3A030A9,
0xE0030391,
0xE1A03743,
0xE2833E39,
0xE0030391,
0xE1A03743,
0xE2833C09,
0xE283301C,
0xE0030391,
0xE1A03743,
0xE2833C0F,
0xE28330B6,
0xE0030391,
0xE1A03743,
0xE2833C16,
0xE28330AA,
0xE0030391,
0xE1A03743,
0xE2833A02,
0xE2833081,
0xE0030391,
0xE1A03743,
0xE2833C36,
0xE2833051,
0xE0030391,
0xE1A03743,
0xE2833CA2,
0xE28330F9,
0xE0000093,
0xE1A00840,
0xE12FFF1E,
0xE3A00001,
0xE3A01001,
0xE92D4010,
0xE3A03000,
0xE3A04001,
0xE3500000,
0x1B000004,
0xE5CC3301,
0xEB000002,
0x0AFFFFFC,
0xE8BD4010,
0xE12FFF1E,
0xE3A0C301,
0xE5CC3208,
0xE15C20B8,
0xE0110002,
0x10222000,
0x114C20B8,
0xE5CC4208,
0xE12FFF1E,
0xE92D500F,
0xE3A00301,
0xE1A0E00F,
0xE510F004,
0xE8BD500F,
0xE25EF004,
0xE59FD044,
0xE92D5000,
0xE14FC000,
0xE10FE000,
0xE92D5000,
0xE3A0C302,
0xE5DCE09C,
0xE35E00A5,
0x1A000004,
0x05DCE0B4,
0x021EE080,
0xE28FE004,
0x159FF018,
0x059FF018,
0xE59FD018,
0xE8BD5000,
0xE169F00C,
0xE8BD5000,
0xE25EF004,
0x03007FF0,
0x09FE2000,
0x09FFC000,
0x03007FE0
};

__attribute__((section(".dtcm"))) 
u8 cpuBitsSet[256];
__attribute__((section(".dtcm"))) 
u8 cpuLowestBitSet[256];


__attribute__((section(".dtcm")))
int romSize = 0x200000; //test normal 0x2000000 current 1/10 oh no only 2.4 MB

int hblank_ticks = 0;

//gba vars end

//GBA CPU opcodes
u32 __attribute__ ((hot)) ndsvcounter(){
	return (u32read(0x04000006)&0x1ff);
}


//CPULoop-> processes registers from various mem and generate threads for each one
u32 __attribute__ ((hot)) cpu_calculate(){
	int clockticks=0; //actual CPU ticking (heartbeat)
		
		//***********************(CPU prefetch cycle lapsus)********************************/
		// variable used by the CPU core
		cpuBreakLoop = false;
		
		//cause spammed cpu_xxxreads: any broken PC address, will abuse cpureads.
		#ifdef PREFETCH_ENABLED
		
			//prefetch part
			//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
			u32 fetchnextpc=armnextpc(rom);
		
			//idles in this state because swi idle and CPU enabled
			if(!holdState && !SWITicks) {
				if ((fetchnextpc & 0x0803FFFF) == 0x08020000) //armNextPC
					gba.busprefetchcount=0x100;
			}
		#endif
		
		/////////////////////////(CPU prefetch cycle lapsus end)///////////////////////////
        
        /* nds has that on lcd hardware
        if(gba.soundticks < lcdTicks){
            gba.soundticks+=lcdTicks; //element thread is set to zero when thread is executed.
        }
        */
        if (SWITicks < lcdTicks){
            SWITicks+=lcdTicks;
        }
            
        if (IRQTicks < lcdTicks){
            IRQTicks+=lcdTicks;
        }
        
        ////////////////////////////////////////THREADS//////////////////////////////////////////
		
        //swi acknowledges main clock
		if (SWITicks){
			SWITicks-=cpuTotalTicks;
			if (SWITicks<0)
				SWITicks = 0;
		}
			
		//irq acknowledges main clock
		if (IRQTicks){
			IRQTicks -= cpuTotalTicks;
			if (IRQTicks<0)
				IRQTicks = 0;
		}
		
        cpuTotalTicks = (cpuTotalTicks - cpuNextEvent);	//delta thread time post prefetch
		
		//LCD execute
		lcdTicks -= cpuTotalTicks;
		
		///////////////////////////////////// Interrupt thread //////////////////////////////////////
		if(GBAIF && (GBAIME & 1) && armIrqEnable){
			int res = GBAIF & GBAIE;
			if(stopState)
				res &= 0x3080;
			if(res) {
            
				if (intState){
					if (!IRQTicks){
						cpuirq(0x12); //CPUInterrupt();
						intState = false;
						holdState = false;
						stopState = false;
						holdType = 0;
					}
				}
				else{
					if (!holdState){
						intState = true;
						IRQTicks=7;
					if (cpuNextEvent> IRQTicks)
						cpuNextEvent = IRQTicks;
					}
					else{
						cpuirq(0x12); //CPUInterrupt();
						holdState = false;
						stopState = false;
						holdType = 0;
					}
				}
				// Stops the SWI Ticks emulation if an IRQ is executed
				//(to avoid problems with nested IRQ/SWI)
				if (SWITicks)
					SWITicks = 0;
			}
		}
		
		
		/////////////////////////////////VBLANK/HBLANK THREAD/////////////////////////////////////////////////////////
		//not this one
		
		if(gbaSystemGlobal.GBADISPSTAT & 2) { //HBLANK period
			//draw
			//gbavideorender((u32)gba.vidram, (u32)hblankdelta);
		
			// if in H-Blank, leave it and move to drawing mode (deadzone ticks even while hblank is 0)
			lcdTicks += 1008;
			
			//last (visible) vertical line
			if(gbaSystemGlobal.GBAVCOUNT == 160) {
				frameCount++;	//framecount
				u32 joy = 0;	// update joystick information
			
				joy = systemreadjoypad(-1);
				gbaSystemGlobal.GBAP1 = 0x03FF ^ (joy & 0x3FF);
				if(cpuEEPROMSensorEnabled){
					//systemUpdateMotionSensor(); //
				}
				UPDATE_REG(0x130 , gbaSystemGlobal.GBAP1);	//UPDATE_REG(0x130, gbaSystemGlobal.GBAP1);
				u16 P1CNT =	CPUReadHalfWord(((u32)ioMem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
				// this seems wrong, but there are cases where the game
				// can enter the stop state without requesting an IRQ from
				// the joypad.
				if((P1CNT & 0x4000) || stopState) {
					u16 p1 = (0x3FF ^ gbaSystemGlobal.GBAP1) & 0x3FF;
					if(P1CNT & 0x8000) {
						if(p1 == (P1CNT & 0x3FF)) {
							GBAIF |= 0x1000;
							UPDATE_REG(0x202 , GBAIF);	
						}
					} 
					else {
						if(p1 & P1CNT) {
							GBAIF |= 0x1000;
							UPDATE_REG(0x202 , GBAIF);
						}
					}
				}
				
				gbaSystemGlobal.GBADISPSTAT |= 1; 			//V-BLANK flag set (period 160..226)
				gbaSystemGlobal.GBADISPSTAT &= 0xFFFD;		//UNSET V-COUNTER FLAG
				//UPDATE_REG(0x04 , gbaSystemGlobal.GBADISPSTAT);
				
				if(gbaSystemGlobal.GBADISPSTAT & 0x0008) {
					GBAIF |= 1;
					UPDATE_REG(0x202 , GBAIF);
				}
				
				//CPUCheckDMA(1, 0x0f); 	//DMA is HBLANK period now
			}
			
			//Reaching last (non-visible) line
			else if(gbaSystemGlobal.GBAVCOUNT >= 228){ 	
				gbaSystemGlobal.GBADISPSTAT &= 0xFFFC;
				gbaSystemGlobal.GBAVCOUNT = 0;
			}
			
			
			if(hblank_ticks>=240){
				gbaSystemGlobal.GBADISPSTAT &= 0xFFFD; //(unset) toggle HBLANK flag only when entering/exiting VCOUNTer
				hblank_ticks=0;
			}
		}
		
		//Entering H-Blank (gbaSystemGlobal.GBAVCOUNT time) 
		//note: (can't set gbaSystemGlobal.GBAVCOUNT ticking on HBLANK because this is always HBLANK time on NDS interrupts (stuck gbaSystemGlobal.GBAVCOUNT from 0x04000006)
		else{
			
			gbaSystemGlobal.GBADISPSTAT |= 2;					//(set) toggle HBLANK flag only when entering/exiting VCOUNTer
			lcdTicks += 224;
			gbaSystemGlobal.GBAVCOUNT=ndsvcounter();
			//gba.gbaSystemGlobal.GBAVCOUNT++; 					//vcount method read from hardware from now on
			
			//CPUCheckDMA(2, 0x0f); 	//DMA is HBLANK period now
			
			if(gbaSystemGlobal.GBADISPSTAT & 16) {
				GBAIF |= 2;
				UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
			}
			
			//////////////////////////////////////VCOUNTER Thread ////////////////////////////////////////////////////////////
			if( (gbaSystemGlobal.GBAVCOUNT) == (gbaSystemGlobal.GBADISPSTAT >> 8) ) { //V-Count Setting (LYC) == gbaSystemGlobal.GBAVCOUNT = IRQ VBLANK
				gbaSystemGlobal.GBADISPSTAT |= 4;
				
				if(gbaSystemGlobal.GBADISPSTAT & 0x20) {
					GBAIF |= 4;
					UPDATE_REG(0x202, GBAIF);		 //UPDATE_REG(0x202, gba->GBAIF);
				}
			}
			else {
				gbaSystemGlobal.GBADISPSTAT &= 0xFFFB;
				//UPDATE_REG(0x4, gba->gbaSystemGlobal.GBADISPSTAT);
			}		
		}
		
		/*
		if((gba.layerenabledelay) >0){
			gba.layerenabledelay--;
			if (gba.layerenabledelay==1)
				gba.layerenable = gba.layersettings & gba.gbaSystemGlobal.GBADISPCNT;
		}
		*/
		
		UPDATE_REG(0x04, gbaSystemGlobal.GBADISPSTAT);	
		UPDATE_REG(0x06, gbaSystemGlobal.GBAVCOUNT);
		
		//**********************************DMA THREAD runs on blank irq*************************************/
		
return 0;
}

u32 __attribute__ ((hot)) cpu_fdexecute(){

//1) read GBAROM entrypoint
//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
//3) get pointers from all reserved memory areas (allocated)
//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]

//btw entrypoint is always ARM code 
	
	if(armState){
		//Fetch Decode & Execute ARM
		//Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever Word is)
		u32 new_instr=armfetchpc_arm(exRegs[0xf]&0xfffffffe); 
		#ifdef DEBUGEMU
			printf("/******ARM******/");
			printf("\n rom:%x [%x]\n",(unsigned int)exRegs[0xf],(unsigned int)new_instr);
		#endif
		disarmcode(new_instr);
		
	}
	else{
		//Fetch Decode & Execute THUMB
		//Half Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever HalfWord is)
		u16 new_instr=armfetchpc_thumb((exRegs[0xf])&0xfffffffe);
		#ifdef DEBUGEMU
			printf("/******THUMB******/");
			printf("\n rom:%x [%x]\n",(unsigned int)exRegs[0xf],(unsigned int)new_instr);
		#endif
		disthumbcode(new_instr);
		
	}
	//HBLANK Handles CPU_EMULATE(); //checks and interrupts all the emulator kind of requests (IRQ,DMA)

/*
if(rom==0x081e2b31){
	printf("opcode for PC:(%x) [%x] \n",(unsigned int)rom,(unsigned int)armfetchpc_thumb((rom&0xfffffffe)));
	while(1);
}
*/

//read input is done already -> gba.gbaSystemGlobal.GBAP1

//increase PC depending on CPUmode
if (armState)
	exRegs[0xf]+=4;
else
	exRegs[0xf]+=2;

//before anything, interrupts (GBA generated) are checked on NDS9 IRQ.s PU.C exceptirq()

return 0;
}

//cpu interrupt
u32 cpuirq(u32 cpumode){

		//Enter cpu<mode>
		updatecpuflags(1,cpsrvirt,cpumode);
		exRegs[0xe]=exRegs[0xf]=(u32)(u8*)rom; //gba->reg[14].I = PC;
		
		if(!armState)//if(!savedState)
			exRegs[0xe]+=2; //gba->reg[14].I += 2;
		armState=true;
		armIrqEnable = false;
		
		//refresh jump opcode in biosprotected vector
		biosProtected[0] = 0x02;
		biosProtected[1] = 0xc0;
		biosProtected[2] = 0x5e;
		biosProtected[3] = 0xe5;

		//set PC
		exRegs[0xf]=(u32)0x18;
		rom=(u8*)(u32)exRegs[0xf];
        
		//Restore CPU<mode>
		updatecpuflags(1,cpsrvirt,spsr_last&0x1F);

return 0;
}





//(CPUinterrupts)
//software interrupts service (GBA BIOS calls!)
u32 __attribute__ ((hot)) swi_virt(u32 swinum){
    switch(swinum) {
        case 0x00:
            //BIOS_SoftReset();
		break;
        case 0x01:
            bios_registerramreset(exRegs[0]);
		break;
        case 0x02:
        {
            #ifdef DEV_VERSION
                printf("Halt: GBAIE %x\n",(unsigned int)GBAIE);
            #endif
            //holdState = true;
            //holdType = -1;
            //cpuNextEvent = cpuTotalTicks;
            
            //GBA game ended all tasks, CPU is idle now.
            //gba4ds_swiHalt();
		}
		break;
        case 0x03:
        {
            #ifdef DEV_VERSION
                printf("Stop\n");
            #endif
			//holdState = true;
			//holdType = -1;
			//stopState = true;
			//cpuNextEvent = cpuTotalTicks;
			
            //ori
            //gba4ds_swiIntrWait(1,(GBAIE & 0x6080));
            
            //coto: new safe GBA SWI sleepmode
            ////execute_arm7_command(0xc0700103,0x0,0x0);
		}
        break;
        case 0x04:
        {
            #ifdef DEV_VERSION
            printf("IntrWait: 0x%08x,0x%08x\n",(unsigned int)R[0],(unsigned int)R[1]);      
            #endif
            
            //gba4ds_swiIntrWait(R[0],R[1]);
            //CPUSoftwareInterrupt();
        
		}
        break;    
        case 0x05:
        {
            #ifdef DEV_VERSION
                //printf("VBlankIntrWait: 0x%08X 0x%08X\n",REG_IE,anytimejmpfilter);
                //VblankHandler(); //todo
            #endif
            //if((REG_DISPSTAT & DISP_IN_VBLANK)) while((REG_DISPSTAT & DISP_IN_VBLANK)); //workaround
            //while(!(REG_DISPSTAT & DISP_IN_VBLANK));
            
            //send cmd
            ////execute_arm7_command(0xc0700100,0x1FFFFFFB,0x0);
            //gba4ds_swiWaitForVBlank();
            
            //coto: new sleep mode 
            //gba4ds_swiWaitForVBlank();
            ////execute_arm7_command(0xc3730100,0x0,0x0);
            
            //coto: speedup games that run on vblank (breaks vblank dependant games) 
            //asm("mov r0,#1");
            //asm("mov r1,#1");
            //HALTCNT_ARM9OPT();
            //VblankHandler();
        }
		break;
        case 0x06:
            bios_div();
		break;
        case 0x07:
            bios_divarm();
		break;
        case 0x08:
            bios_sqrt();
		break;
        case 0x09:
            bios_arctan();
		break;
        case 0x0A:
            bios_arctan2();
		break;
        case 0x0B:
            bios_cpuset();
		break;
        case 0x0C:
            bios_cpufastset();
		break;
        case 0x0D:
            bios_getbioschecksum();
		break;
        case 0x0E:
            bios_bgaffineset();
		break;
        case 0x0F:
            bios_objaffineset();
		break;
        case 0x10:
            bios_bitunpack();
		break;
        case 0x11:
            bios_lz77uncompwram();
		break;
        case 0x12:
            bios_lz77uncompvram();
		break;
        case 0x13:
            bios_huffuncomp();
		break;
        case 0x14:
            bios_rluncompwram();
		break;
        case 0x15:
            bios_rluncompvram();
		break;
        case 0x16:
            bios_diff8bitunfilterwram();
		break;
        case 0x17:
            bios_diff8bitunfiltervram();
		break;
        case 0x18:
            bios_diff16bitunfilter();
		break;
        
        //coto: added soundbias support
        case 0x19:
        {
            //#ifdef DEV_VERSION
            printf("SoundBiasSet: 0x%08x \n",(unsigned int)exRegs[0]);
            //#endif    
            //if(reg[0].I) //ichfly sound todo
            //systemSoundPause(); //ichfly sound todo
            //else //ichfly sound todo
            //systemSoundResume(); //ichfly sound todo
            
            //SWI 19h (GBA) or SWI 08h (NDS7/DSi7) - SoundBias
            //r0   BIAS level (0=Level 000h, any other value=Level 200h)
            //r1   Delay Count (NDS/DSi only) (GBA uses a fixed delay count of 8)
            ////execute_arm7_command(0xc0700104,(u32)R[0],(u32)0x00000008);
            
		}
        break;
        case 0x1F:
           bios_midikey2freq();
		break;
        case 0x2A:
            bios_snddriverjmptablecopy();
		break;
		// let it go, because we don't really emulate this function
        case 0x2D: 
            
		break;
        case 0x2F: 
        {
            //REG_IME = IME_DISABLE;
            //while(1);
		}
        break;
        default:
            if((swinum & 0x30) >= 0x30)
            {
                printf("r%x ",(unsigned int)(swinum));
            }
            else
            {
                printf("Unsupported BIOS function %02x. A BIOS file is needed in order to get correct behaviour.",(unsigned int)swinum);
                
            }
		break;
    }
    
return 0;
}

//current CPU mode working set of registers
u32  __attribute__((section(".dtcm"))) exRegs[0x10]; //placeholder for actual CPU mode registers

//r13, r14 for each mode (sp and lr)
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13usr[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14usr[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13fiq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14fiq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13irq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14irq[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13svc[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14svc[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14abt[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13abt[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r14und[0x1];
u32  __attribute__((section(".dtcm"))) gbavirtreg_r13und[0x1];
//u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1]; //stack shared with usr
//u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1];

//original registers used by any PSR_MODE that do belong to FIQ r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_fiq[0x5];

//original registers used by any PSR_MODE that do not belong to FIQ r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_nonfiq[0x5];
u32  __attribute__((section(".dtcm"))) gbavirtreg_cpu[0x10];

__attribute__((section(".itcm")))
__attribute__ ((hot))
void doDMA(u32 &s, u32 &d, u32 si, u32 di, u32 c, int transfer32) 
{

	cpuDmaCount = c;
  if(transfer32) {
    s &= 0xFFFFFFFC;
    if(s < 0x02000000 && (exRegs[15] >> 24)) {
      while(c != 0) {
        CPUWriteMemory(d, 0);
        d += di;
        c--;
      }
    } else {
      while(c != 0) {
        cpuDmaLast = CPUReadMemory(s);
        CPUWriteMemory(d, cpuDmaLast);
        d += di;
        s += si;
        c--;
      }
    }
  } else {
    s &= 0xFFFFFFFE;
    si = (int)si >> 1;
    di = (int)di >> 1;
    if(s < 0x02000000 && (exRegs[15] >> 24)) {
      while(c != 0) {
        CPUWriteHalfWord(d, 0);
        d += di;
        c--;
      }
    } else {
      while(c != 0) {
        cpuDmaLast = CPUReadHalfWord(s);
        CPUWriteHalfWord(d, cpuDmaLast);
        cpuDmaLast |= (cpuDmaLast<<16);
        d += di;
        s += si;
        c--;
      }
    }
  }

}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
__attribute__ ((hot))
void  __attribute__ ((hot)) CPUCheckDMA(int reason, int dmamask)
{
  // DMA 0
  if((gbaSystemGlobal.GBADM0CNT_H & 0x8000) && (dmamask & 1)) {
    if(((gbaSystemGlobal.GBADM0CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((gbaSystemGlobal.GBADM0CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((gbaSystemGlobal.GBADM0CNT_H >> 5) & 3) {
      case 0:
        break;
      case 1:
        destIncrement = (u32)-4;
        break;
      case 2:
        destIncrement = 0;
        break;
      }      
#ifdef DEV_VERSION
      if(systemVerbose & VERBOSE_DMA0) {
        int count = (gbaSystemGlobal.GBADM0CNT_L ? gbaSystemGlobal.GBADM0CNT_L : 0x4000) << 1;
        if(gbaSystemGlobal.GBADM0CNT_H & 0x0400)
          count <<= 1;
        printf("DMA0: s=%08x d=%08x c=%04x count=%08x\n", dma0Source, dma0Dest, 
            gbaSystemGlobal.GBADM0CNT_H,
            count);
      }
#endif
      doDMA(dma0Source, dma0Dest, sourceIncrement, destIncrement,
            gbaSystemGlobal.GBADM0CNT_L ? gbaSystemGlobal.GBADM0CNT_L : 0x4000,
            gbaSystemGlobal.GBADM0CNT_H & 0x0400);
      cpuDmaHack = true;

      /*if(gbaSystemGlobal.GBADM0CNT_H & 0x4000) {
        GBAIF |= 0x0100;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo
      
      if(((gbaSystemGlobal.GBADM0CNT_H >> 5) & 3) == 3) {
        dma0Dest = gbaSystemGlobal.GBADM0DAD_L | (gbaSystemGlobal.GBADM0DAD_H << 16);
      }
      
      if(!(gbaSystemGlobal.GBADM0CNT_H & 0x0200) || (reason == 0)) {
        gbaSystemGlobal.GBADM0CNT_H &= 0x7FFF;
        UPDATE_REG(0xBA, gbaSystemGlobal.GBADM0CNT_H);
      }
    }
  }
  
  // DMA 1
  if((gbaSystemGlobal.GBADM1CNT_H & 0x8000) && (dmamask & 2)) {
    if(((gbaSystemGlobal.GBADM1CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((gbaSystemGlobal.GBADM1CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((gbaSystemGlobal.GBADM1CNT_H >> 5) & 3) {
      case 0:
        break;
      case 1:
        destIncrement = (u32)-4;
        break;
      case 2:
        destIncrement = 0;
        break;
      }      
      if(reason == 3) {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA1) {
          printf("DMA1: s=%08x d=%08x c=%04x count=%08x\n", dma1Source, dma1Dest,
              gbaSystemGlobal.GBADM1CNT_H,
              16);
        }
#endif  
        doDMA(dma1Source, dma1Dest, sourceIncrement, 0, 4,
              0x0400);
      } else {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA1) {
          int count = (gbaSystemGlobal.GBADM1CNT_L ? gbaSystemGlobal.GBADM1CNT_L : 0x4000) << 1;
          if(gbaSystemGlobal.GBADM1CNT_H & 0x0400)
            count <<= 1;
          printf("DMA1: s=%08x d=%08x c=%04x count=%08x\n", dma1Source, dma1Dest,
              gbaSystemGlobal.GBADM1CNT_H,
              count);
        }
#endif          
        doDMA(dma1Source, dma1Dest, sourceIncrement, destIncrement,
              gbaSystemGlobal.GBADM1CNT_L ? gbaSystemGlobal.GBADM1CNT_L : 0x4000,
              gbaSystemGlobal.GBADM1CNT_H & 0x0400);
      }
      cpuDmaHack = true;

      /*if(gbaSystemGlobal.GBADM1CNT_H & 0x4000) {
        GBAIF |= 0x0200;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo
      
      if(((gbaSystemGlobal.GBADM1CNT_H >> 5) & 3) == 3) {
        dma1Dest = gbaSystemGlobal.GBADM1DAD_L | (gbaSystemGlobal.GBADM1DAD_H << 16);
      }
      
      if(!(gbaSystemGlobal.GBADM1CNT_H & 0x0200) || (reason == 0)) {
        gbaSystemGlobal.GBADM1CNT_H &= 0x7FFF;
        UPDATE_REG(0xC6, gbaSystemGlobal.GBADM1CNT_H);
      }
    }
  }
  
  // DMA 2
  if((gbaSystemGlobal.GBADM2CNT_H & 0x8000) && (dmamask & 4)) {
    if(((gbaSystemGlobal.GBADM2CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((gbaSystemGlobal.GBADM2CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((gbaSystemGlobal.GBADM2CNT_H >> 5) & 3) {
      case 0:
        break;
      case 1:
        destIncrement = (u32)-4;
        break;
      case 2:
        destIncrement = 0;
        break;
      }      
      if(reason == 3) {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA2) {
          int count = (4) << 2;
          printf("DMA2: s=%08x d=%08x c=%04x count=%08x\n", dma2Source, dma2Dest,
              gbaSystemGlobal.GBADM2CNT_H,
              count);
        }
#endif                  
        doDMA(dma2Source, dma2Dest, sourceIncrement, 0, 4,
              0x0400);
      } else {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA2) {
          int count = (gbaSystemGlobal.GBADM2CNT_L ? gbaSystemGlobal.GBADM2CNT_L : 0x4000) << 1;
          if(gbaSystemGlobal.GBADM2CNT_H & 0x0400)
            count <<= 1;
          printf("DMA2: s=%08x d=%08x c=%04x count=%08x\n", dma2Source, dma2Dest,
              gbaSystemGlobal.GBADM2CNT_H,
              count);
        }
#endif                  
        doDMA(dma2Source, dma2Dest, sourceIncrement, destIncrement,
              gbaSystemGlobal.GBADM2CNT_L ? gbaSystemGlobal.GBADM2CNT_L : 0x4000,
              gbaSystemGlobal.GBADM2CNT_H & 0x0400);
      }
      cpuDmaHack = true;

      /*if(gbaSystemGlobal.GBADM2CNT_H & 0x4000) {
        GBAIF |= 0x0400;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo

      if(((gbaSystemGlobal.GBADM2CNT_H >> 5) & 3) == 3) {
        dma2Dest = gbaSystemGlobal.GBADM2DAD_L | (gbaSystemGlobal.GBADM2DAD_H << 16);
      }
      
      if(!(gbaSystemGlobal.GBADM2CNT_H & 0x0200) || (reason == 0)) {
        gbaSystemGlobal.GBADM2CNT_H &= 0x7FFF;
        UPDATE_REG(0xD2, gbaSystemGlobal.GBADM2CNT_H);
      }
    }
  }

  // DMA 3
  if((gbaSystemGlobal.GBADM3CNT_H & 0x8000) && (dmamask & 8)) {
    if(((gbaSystemGlobal.GBADM3CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((gbaSystemGlobal.GBADM3CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((gbaSystemGlobal.GBADM3CNT_H >> 5) & 3) {
      case 0:
        break;
      case 1:
        destIncrement = (u32)-4;
        break;
      case 2:
        destIncrement = 0;
        break;
      }      
#ifdef DEV_VERSION
      if(systemVerbose & VERBOSE_DMA3) {
        int count = (gbaSystemGlobal.GBADM3CNT_L ? gbaSystemGlobal.GBADM3CNT_L : 0x10000) << 1;
        if(gbaSystemGlobal.GBADM3CNT_H & 0x0400)
          count <<= 1;
        printf("DMA3: s=%08x d=%08x c=%04x count=%08x\n", dma3Source, dma3Dest,
            gbaSystemGlobal.GBADM3CNT_H,
            count);
      }
#endif                
      doDMA(dma3Source, dma3Dest, sourceIncrement, destIncrement,
            gbaSystemGlobal.GBADM3CNT_L ? gbaSystemGlobal.GBADM3CNT_L : 0x10000,
            gbaSystemGlobal.GBADM3CNT_H & 0x0400);
      /*if(gbaSystemGlobal.GBADM3CNT_H & 0x4000) {
        GBAIF |= 0x0800;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo

      if(((gbaSystemGlobal.GBADM3CNT_H >> 5) & 3) == 3) {
        dma3Dest = gbaSystemGlobal.GBADM3DAD_L | (gbaSystemGlobal.GBADM3DAD_H << 16);
      }
      
      if(!(gbaSystemGlobal.GBADM3CNT_H & 0x0200) || (reason == 0)) {
        gbaSystemGlobal.GBADM3CNT_H &= 0x7FFF;
        UPDATE_REG(0xDE, gbaSystemGlobal.GBADM3CNT_H);
      }
    }
  }
}

//IO opcodes
__attribute__((section(".itcm")))
__attribute__ ((hot))
void  CPUUpdateRegister(u32 address, u16 value)
{
    //coto
  switch(address) {
    
  #ifdef own_bg_render
  
  case 0x00:
  
    
        //well, this part 1/2: setup vram and engines
        //  part 2/2 redirects all writes to each tile sections / map sections of current gba video mode
		
		//coto: get tile/map screen sizes
		
		switch(value & 0x7){
		
		
		//In this mode, four text background layers can be shown. In this mode backgrounds 0 - 3 all count as "text" backgrounds, and cannot be scaled or rotated. 
		//Check out the section on text backgrounds for details on this. 
		case(0):{
			
			set_gba_bgmode(0);
            
			//(LCD) blend control register
			//higher prio bg against lower prio, for alpha / blend / transparency (sprite pixels)
			REG_BLDCNT = gbaSystemGlobal.GBABLDMOD;
			
			//window
			WIN_IN = gbaSystemGlobal.GBAWININ;
			WIN_OUT = gbaSystemGlobal.GBAWINOUT;
			
			//scrolling
			//bg0
			bgSetScroll(bg_0_setting,gbaSystemGlobal.GBABG0HOFS,gbaSystemGlobal.GBABG0VOFS);
			//bg1
			bgSetScroll(bg_1_setting,gbaSystemGlobal.GBABG1HOFS,gbaSystemGlobal.GBABG1VOFS);
			//bg2
			bgSetScroll(bg_2_setting,gbaSystemGlobal.GBABG2HOFS,gbaSystemGlobal.GBABG2VOFS);
			//bg3
			bgSetScroll(bg_3_setting,gbaSystemGlobal.GBABG3HOFS,gbaSystemGlobal.GBABG3VOFS);
			
			//mode 0 does not use rotscale registers
			
			}
			break;
			
			//This mode is similar in most respects to Mode 0, the main difference being that only 3 backgrounds are accessible -- 0, 1, and 2. 
			//Bgs 0 and 1 are text backgrounds, while bg 2 is a rotation/scaling background.
			case(1):
			{
                set_gba_bgmode(1);
				
                //not yet
                //bgSetControlBits(bg_0_setting,gbaSystemGlobal.GBABG0CNT);
				//bgSetControlBits(bg_1_setting,gbaSystemGlobal.GBABG0CNT);
				//bgSetControlBits(bg_2_setting,gbaSystemGlobal.GBABG2CNT);
                    //bgSetControlBits(bg_3_setting,gbaSystemGlobal.GBABG3CNT); //bg 0,1 & 2 only
				
				//(LCD) blend control register
				//higher prio bg against lower prio, for alpha / blend / transparency (sprite pixels)
				REG_BLDCNT = gbaSystemGlobal.GBABLDMOD;
				
				//window
				WIN_IN = gbaSystemGlobal.GBAWININ;
				WIN_OUT = gbaSystemGlobal.GBAWINOUT;
				
				//scrolling
                bgSetScroll(bg_0_setting,gbaSystemGlobal.GBABG0HOFS,gbaSystemGlobal.GBABG0VOFS);
                bgSetScroll(bg_1_setting,gbaSystemGlobal.GBABG1HOFS,gbaSystemGlobal.GBABG1VOFS);
				
				//libnds rotscale/affine
				//bg 2
				bgSetAffineMatrixScroll (
					bg_2_setting, 
					(int)gbaSystemGlobal.GBABG2PA, 
					(int)gbaSystemGlobal.GBABG2PB, 
					(int)gbaSystemGlobal.GBABG2PC, 
					(int)gbaSystemGlobal.GBABG2PD, 
					(int)(gbaSystemGlobal.GBABG2X_L | (gbaSystemGlobal.GBABG2X_H << 16)), 	//scroll x 
					(int)(gbaSystemGlobal.GBABG2Y_L | (gbaSystemGlobal.GBABG2Y_H << 16))		//scroll y
				);
				
			}
			break;
			
			
			//Like modes 0 and 1, this uses tiled backgrounds. It uses backgrounds 2 and 3, both of which are rotate/scale backgrounds. 
			case(2):
			{
				
				//handled directly BG CNT because engine 0:0 is used
				REG_BG0CNT = gbaSystemGlobal.GBABG0CNT;
				bgSetControlBits(bg_1_setting,gbaSystemGlobal.GBABG1CNT);	//bg 0 only text
				bgSetControlBits(bg_2_setting,gbaSystemGlobal.GBABG2CNT);
				bgSetControlBits(bg_3_setting,gbaSystemGlobal.GBABG3CNT);
				
				//libnds rotscale/affine
				//bg2
				bgSetAffineMatrixScroll (bg_2_setting, 
				(int)gbaSystemGlobal.GBABG2PA, 
				(int)gbaSystemGlobal.GBABG2PB, 
				(int)gbaSystemGlobal.GBABG2PC, 
				(int)gbaSystemGlobal.GBABG2PD, 
				(int)(gbaSystemGlobal.GBABG2X_L | (gbaSystemGlobal.GBABG2X_H << 16)), 	//scroll x 
				(int)(gbaSystemGlobal.GBABG2Y_L | (gbaSystemGlobal.GBABG2Y_H << 16))		//scroll y
				);
				
				//bg3
				bgSetAffineMatrixScroll (bg_3_setting, 
				(int)gbaSystemGlobal.GBABG3PA, 
				(int)gbaSystemGlobal.GBABG3PB, 
				(int)gbaSystemGlobal.GBABG3PC, 
				(int)gbaSystemGlobal.GBABG3PD, 
				(int)(gbaSystemGlobal.GBABG3X_L | (gbaSystemGlobal.GBABG3X_H << 16)), 	//scroll x 
				(int)(gbaSystemGlobal.GBABG3Y_L | (gbaSystemGlobal.GBABG3Y_H << 16))		//scroll y
				);
				
				//(LCD) blend control register
				//higher prio bg against lower prio, for alpha / blend / transparency (sprite pixels)
				REG_BLDCNT = gbaSystemGlobal.GBABLDMOD;
				
				//window
				WIN_IN = gbaSystemGlobal.GBAWININ;
				WIN_OUT = gbaSystemGlobal.GBAWINOUT;
				
				//scrolling
				//bg zero is direct reg access
				REG_BG0HOFS = gbaSystemGlobal.GBABG0HOFS;
				REG_BG0VOFS = gbaSystemGlobal.GBABG0VOFS;
				//bgsetscroll
				bgSetScroll(bg_1_setting,gbaSystemGlobal.GBABG1HOFS,gbaSystemGlobal.GBABG1VOFS);
				bgSetScroll(bg_2_setting,gbaSystemGlobal.GBABG2HOFS,gbaSystemGlobal.GBABG2VOFS);
				bgSetScroll(bg_3_setting,gbaSystemGlobal.GBABG3HOFS,gbaSystemGlobal.GBABG3VOFS);
			}
			break;
			
			
			//The screen is a 16 bit linear buffer. Meaning every pixel has two bytes of information. Those two bytes are made up of red, green, and blue component. 
			//Each of those basic three colors can have a value from 0..31 resulting in a maximum of 32,768 colors. Mode 3 has a resolution of 240x160. 
			case(3):
			{			
			}	
			break;
			
			//Mode 4:8-Bit paletted bitmapped mode at 240x160. The bitmap starts at either 0x06000000 or 0x0600A000, depending on bit 4 of REG_DISPCNT. 
			//Swapping the map and drawing in the one that isn't displayed allows for page flipping techniques to be used. The palette is at 0x5000000, 
			//and contains 256 16-bit color entries.
			case(4):
			{
			}
			break;
			
			case(5):
            {
            }
			break;
		}
        
     
    break;
    
    //ori bg code
    #else
    case 0:
    
    {		
		if((value & 7) < 3)
		{
			if(value != gbaSystemGlobal.GBADISPCNT)
			{
				if(!((gbaSystemGlobal.GBADISPCNT & 7) < 3))
				{
					//reset gbaSystemGlobal.GBABG3HOFS and gbaSystemGlobal.GBABG3VOFS
					REG_BG3HOFS = gbaSystemGlobal.GBABG3HOFS;
					REG_BG3VOFS = gbaSystemGlobal.GBABG3VOFS;

					//reset
					REG_BG3CNT = gbaSystemGlobal.GBABG3CNT;
					REG_BG2CNT = gbaSystemGlobal.GBABG2CNT;
					REG_BLDCNT = gbaSystemGlobal.GBABLDMOD;
					WIN_IN = gbaSystemGlobal.GBAWININ;
					WIN_OUT = gbaSystemGlobal.GBAWINOUT;

					REG_BG2PA = gbaSystemGlobal.GBABG2PA;
					REG_BG2PB = gbaSystemGlobal.GBABG2PB;
					REG_BG2PC = gbaSystemGlobal.GBABG2PC;
					REG_BG2PD = gbaSystemGlobal.GBABG2PD;
					REG_BG2X = (gbaSystemGlobal.GBABG2X_L | (gbaSystemGlobal.GBABG2X_H << 16));
					REG_BG2Y = (gbaSystemGlobal.GBABG2Y_L | (gbaSystemGlobal.GBABG2Y_H << 16));

					REG_BG3PA = gbaSystemGlobal.GBABG3PA;
					REG_BG3PB = gbaSystemGlobal.GBABG3PB;
					REG_BG3PC = gbaSystemGlobal.GBABG3PC;
					REG_BG3PD = gbaSystemGlobal.GBABG3PD;
					REG_BG3X = (gbaSystemGlobal.GBABG3X_L | (gbaSystemGlobal.GBABG3X_H << 16));
					REG_BG3Y = (gbaSystemGlobal.GBABG3Y_L | (gbaSystemGlobal.GBABG3Y_H << 16));
				}

				u32 dsValue;
				dsValue  = value & 0xFF87;
				dsValue |= (value & (1 << 5)) ? (1 << 23) : 0;	/* oam hblank access */
				dsValue |= (value & (1 << 6)) ? (1 << 4) : 0;	/* obj mapping 1d/2d */
				dsValue |= (value & (1 << 7)) ? 0 : (1 << 16);	/* forced blank => no display mode (both)*/
				REG_DISPCNT = dsValue; 
			}
		}
		else
		{
			if((value & 0xFFEF) != (gbaSystemGlobal.GBADISPCNT & 0xFFEF))
			{

				u32 dsValue;
				dsValue  = value & 0xF087;
				dsValue |= (value & (1 << 5)) ? (1 << 23) : 0;	/* oam hblank access */
				dsValue |= (value & (1 << 6)) ? (1 << 4) : 0;	/* obj mapping 1d/2d */
				dsValue |= (value & (1 << 7)) ? 0 : (1 << 16);	/* forced blank => no display mode (both)*/
				REG_DISPCNT = (dsValue | BIT(11)); //enable BG3
				if((gbaSystemGlobal.GBADISPCNT & 7) != (value & 7))
				{
					//coto: adjust the enum definitions to an addressable u32 by the ARMv5 core.
					//8<<2 = bit 32 (but it is bit 31)
					if((value & 7) == 4)
					{
						//bgInit_call(3, BgType_Bmp8, BgSize_B8_256x256,8,8);
						//ori: REG_BG3CNT = (u32)(BG_MAP_BASE(/*mapBase*/8) | (u32)(BG_TILE_BASE(/*tileBase*/8)-1) | (u32)BgSize_B8_256x256);
						REG_BG3CNT = (u32)(BG_MAP_BASE(/*mapBase*/1) | (u32)BG_TILE_BASE(/*tileBase*/2) | (u16)BgSize_B8_256x256);
					}
					else 
					{
						//bgInit_call(3, BgType_Bmp16, BgSize_B16_256x256,8,8);
						//ori: REG_BG3CNT = (u32)BG_MAP_BASE(/*mapBase*/8) | (u32)BG_TILE_BASE(/*tileBase*/8) | (u32)BgSize_B16_256x256;
						REG_BG3CNT = (u32)BG_MAP_BASE(/*mapBase*/1) | (u32)BG_TILE_BASE(/*tileBase*/2) | (u16)BgSize_B16_256x256;
					}
					if((gbaSystemGlobal.GBADISPCNT & 7) < 3)
					{
						//reset gbaSystemGlobal.GBABG3HOFS and gbaSystemGlobal.GBABG3VOFS
						REG_BG3HOFS = 0;
						REG_BG3VOFS = 0;

						//BLDCNT(2 enabeled bits)
						int tempBLDMOD = gbaSystemGlobal.GBABLDMOD & ~0x404;
						tempBLDMOD = tempBLDMOD | ((gbaSystemGlobal.GBABLDMOD & 0x404) << 1);
						REG_BLDCNT = tempBLDMOD;

						//gbaSystemGlobal.GBAWINOUT(2 enabeled bits)
						int tempWINOUT = gbaSystemGlobal.GBAWINOUT & ~0x404;
						tempWINOUT = tempWINOUT | ((gbaSystemGlobal.GBAWINOUT & 0x404) << 1);
						WIN_OUT = tempWINOUT;

						//gbaSystemGlobal.GBAWININ(2 enabeled bits)
						int tempWININ = gbaSystemGlobal.GBAWININ & ~0x404;
						tempWININ = tempWININ | ((gbaSystemGlobal.GBAWININ & 0x404) << 1);
						WIN_IN = tempWININ;

						//swap LCD I/O BG Rotation/Scaling

						REG_BG3PA = gbaSystemGlobal.GBABG2PA;
						REG_BG3PB = gbaSystemGlobal.GBABG2PB;
						REG_BG3PC = gbaSystemGlobal.GBABG2PC;
						REG_BG3PD = gbaSystemGlobal.GBABG2PD;
						REG_BG3X = (gbaSystemGlobal.GBABG2X_L | (gbaSystemGlobal.GBABG2X_H << 16));
						REG_BG3Y = (gbaSystemGlobal.GBABG2Y_L | (gbaSystemGlobal.GBABG2Y_H << 16));
						REG_BG3CNT = REG_BG3CNT | (gbaSystemGlobal.GBABG2CNT & 0x43); //swap gbaSystemGlobal.GBABG2CNT (BG Priority and Mosaic) 
					}
				}
			}
		}
		gbaSystemGlobal.GBADISPCNT = value & 0xFFF7;
		UPDATE_REG(0x00, gbaSystemGlobal.GBADISPCNT);
    }
  
    
    break;
    
    #endif
  case 0x04:
    gbaSystemGlobal.GBADISPSTAT = (value & 0xFF38) | (gbaSystemGlobal.GBADISPSTAT & 7);
    UPDATE_REG(0x04, gbaSystemGlobal.GBADISPSTAT);

    break;
  case 0x06:
    // not writable in NDS mode bzw not possible todo
    break;
  case 0x08:
    gbaSystemGlobal.GBABG0CNT = (value & 0xDFCF);
    UPDATE_REG(0x08, gbaSystemGlobal.GBABG0CNT);
	*(u16 *)(0x4000008) = gbaSystemGlobal.GBABG0CNT;
    break;
  case 0x0A:
    gbaSystemGlobal.GBABG1CNT = (value & 0xDFCF);
    UPDATE_REG(0x0A, gbaSystemGlobal.GBABG1CNT);
    *(u16 *)(0x400000A) = gbaSystemGlobal.GBABG1CNT;
	break;
  case 0x0C:
    gbaSystemGlobal.GBABG2CNT = (value & 0xFFCF);
    UPDATE_REG(0x0C, gbaSystemGlobal.GBABG2CNT);
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400000C) = gbaSystemGlobal.GBABG2CNT;
	else //ichfly some extra handling 
	{
		REG_BG3CNT = REG_BG3CNT | (gbaSystemGlobal.GBABG2CNT & 0x43);
	}
    break;
  case 0x0E:
    gbaSystemGlobal.GBABG3CNT = (value & 0xFFCF);
    UPDATE_REG(0x0E, gbaSystemGlobal.GBABG3CNT);
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400000E) = gbaSystemGlobal.GBABG3CNT;
    break;
  case 0x10:
    gbaSystemGlobal.GBABG0HOFS = value & 511;
    UPDATE_REG(0x10, gbaSystemGlobal.GBABG0HOFS);
    *(u16 *)(0x4000010) = value;
	break;
  case 0x12:
    gbaSystemGlobal.GBABG0VOFS = value & 511;
    UPDATE_REG(0x12, gbaSystemGlobal.GBABG0VOFS);
    *(u16 *)(0x4000012) = value;
	break;
  case 0x14:
    gbaSystemGlobal.GBABG1HOFS = value & 511;
    UPDATE_REG(0x14, gbaSystemGlobal.GBABG1HOFS);
	*(u16 *)(0x4000014) = value;
    break;
  case 0x16:
    gbaSystemGlobal.GBABG1VOFS = value & 511;
    UPDATE_REG(0x16, gbaSystemGlobal.GBABG1VOFS);
    *(u16 *)(0x4000016) = value;
	break;      
  case 0x18:
    gbaSystemGlobal.GBABG2HOFS = value & 511;
    UPDATE_REG(0x18, gbaSystemGlobal.GBABG2HOFS);
	*(u16 *)(0x4000018) = value;
    break;
  case 0x1A:
    gbaSystemGlobal.GBABG2VOFS = value & 511;
    UPDATE_REG(0x1A, gbaSystemGlobal.GBABG2VOFS);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400001A) = value; //ichfly only update if it is save
	break;
  case 0x1C:
    gbaSystemGlobal.GBABG3HOFS = value & 511;
    UPDATE_REG(0x1C, gbaSystemGlobal.GBABG3HOFS);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400001C) = value; //ichfly only update if it is save
	break;
  case 0x1E:
    gbaSystemGlobal.GBABG3VOFS = value & 511;
    UPDATE_REG(0x1E, gbaSystemGlobal.GBABG3VOFS);
    *(u16 *)(0x400001E) = value;
	break;      
  case 0x20:
    gbaSystemGlobal.GBABG2PA = value;
    UPDATE_REG(0x20, gbaSystemGlobal.GBABG2PA);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000020) = value;
	else *(u16 *)(0x4000030) = value;
	break;
  case 0x22:
    gbaSystemGlobal.GBABG2PB = value;
    UPDATE_REG(0x22, gbaSystemGlobal.GBABG2PB);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000022) = value;
	else *(u16 *)(0x4000032) = value;
	break;
  case 0x24:
    gbaSystemGlobal.GBABG2PC = value;
    UPDATE_REG(0x24, gbaSystemGlobal.GBABG2PC);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000024) = value;
	else *(u16 *)(0x4000034) = value;
	break;
  case 0x26:
    gbaSystemGlobal.GBABG2PD = value;
    UPDATE_REG(0x26, gbaSystemGlobal.GBABG2PD);
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000026) = value;
	else *(u16 *)(0x4000036) = value;
	break;
  case 0x28:
    gbaSystemGlobal.GBABG2X_L = value;
    UPDATE_REG(0x28, gbaSystemGlobal.GBABG2X_L);
    //gfxBG2Changed |= 1;
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000028) = value;
	else *(u16 *)(0x4000038) = value;
    break;
  case 0x2A:
    gbaSystemGlobal.GBABG2X_H = (value & 0xFFF);
    UPDATE_REG(0x2A, gbaSystemGlobal.GBABG2X_H);
    //gfxBG2Changed |= 1;
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400002A) = value;
	else *(u16 *)(0x400003A) = value;
    break;
  case 0x2C:
    gbaSystemGlobal.GBABG2Y_L = value;
    UPDATE_REG(0x2C, gbaSystemGlobal.GBABG2Y_L);
    //gfxBG2Changed |= 2;
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400002C) = value;
	else *(u16 *)(0x400003C) = value;
    break;
  case 0x2E:
    gbaSystemGlobal.GBABG2Y_H = value & 0xFFF;
    UPDATE_REG(0x2E, gbaSystemGlobal.GBABG2Y_H);
    //gfxBG2Changed |= 2;
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400002E) = value;
	else *(u16 *)(0x400003E) = value;
    break;
  case 0x30:
    gbaSystemGlobal.GBABG3PA = value;
    UPDATE_REG(0x30, gbaSystemGlobal.GBABG3PA);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000030) = value;
	break;
  case 0x32:
    gbaSystemGlobal.GBABG3PB = value;
    UPDATE_REG(0x32, gbaSystemGlobal.GBABG3PB);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000032) = value;
	break;
  case 0x34:
    gbaSystemGlobal.GBABG3PC = value;
    UPDATE_REG(0x34, gbaSystemGlobal.GBABG3PC);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000034) = value;
	break;
  case 0x36:
    gbaSystemGlobal.GBABG3PD = value;
    UPDATE_REG(0x36, gbaSystemGlobal.GBABG3PD);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000036) = value;
	break;
  case 0x38:
    gbaSystemGlobal.GBABG3X_L = value;
    UPDATE_REG(0x38, gbaSystemGlobal.GBABG3X_L);
    //gfxBG3Changed |= 1;
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000038) = value;
	break;
  case 0x3A:
    gbaSystemGlobal.GBABG3X_H = value & 0xFFF;
    UPDATE_REG(0x3A, gbaSystemGlobal.GBABG3X_H);
    //gfxBG3Changed |= 1;    
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400003A) = value;
	break;
  case 0x3C:
    gbaSystemGlobal.GBABG3Y_L = value;
    UPDATE_REG(0x3C, gbaSystemGlobal.GBABG3Y_L);
    //gfxBG3Changed |= 2;    
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400003C) = value;
	break;
  case 0x3E:
    gbaSystemGlobal.GBABG3Y_H = value & 0xFFF;
    UPDATE_REG(0x3E, gbaSystemGlobal.GBABG3Y_H);
    //gfxBG3Changed |= 2;    
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400003E) = value;
	break;
  case 0x40:
    gbaSystemGlobal.GBAWIN0H = value;
    UPDATE_REG(0x40, gbaSystemGlobal.GBAWIN0H);
    //CPUUpdateWindow0();
    *(u16 *)(0x4000040) = value;
	break;
  case 0x42:
    gbaSystemGlobal.GBAWIN1H = value;
    UPDATE_REG(0x42, gbaSystemGlobal.GBAWIN1H);
	*(u16 *)(0x4000042) = value;
    //CPUUpdateWindow1();    
    break;      
  case 0x44:
    gbaSystemGlobal.GBAWIN0V = value;
    UPDATE_REG(0x44, gbaSystemGlobal.GBAWIN0V);
    *(u16 *)(0x4000044) = value;
	break;
  case 0x46:
    gbaSystemGlobal.GBAWIN1V = value;
    UPDATE_REG(0x46, gbaSystemGlobal.GBAWIN1V);
    *(u16 *)(0x4000046) = value;
	break;
  case 0x48:
    gbaSystemGlobal.GBAWININ = value & 0x3F3F;
    UPDATE_REG(0x48, gbaSystemGlobal.GBAWININ);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000048) = value;
	else
	{
		int tempWININ = gbaSystemGlobal.GBAWININ & ~0x404;
		tempWININ = tempWININ | ((gbaSystemGlobal.GBAWININ & 0x404) << 1);
		WIN_IN = tempWININ;
	}
	break;
  case 0x4A:
    gbaSystemGlobal.GBAWINOUT = value & 0x3F3F;
    UPDATE_REG(0x4A, gbaSystemGlobal.GBAWINOUT);
    if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x400004A) = value;
	else
	{
		int tempWINOUT = gbaSystemGlobal.GBAWINOUT & ~0x404;
		tempWINOUT = tempWINOUT | ((gbaSystemGlobal.GBAWINOUT & 0x404) << 1);
		WIN_OUT = tempWINOUT;
	}
	break;
  case 0x4C:
    gbaSystemGlobal.GBAMOSAIC = value;
    UPDATE_REG(0x4C, gbaSystemGlobal.GBAMOSAIC);
    *(u16 *)(0x400004C) = value;
	break;
  case 0x50:
    gbaSystemGlobal.GBABLDMOD = value & 0x3FFF;
    UPDATE_REG(0x50, gbaSystemGlobal.GBABLDMOD);
    //fxOn = ((gbaSystemGlobal.GBABLDMOD>>6)&3) != 0;
    //CPUUpdateRender();
	if((gbaSystemGlobal.GBADISPCNT & 7) < 3)*(u16 *)(0x4000050) = value;
	else
	{
		int tempBLDMOD = gbaSystemGlobal.GBABLDMOD & ~0x404;
		tempBLDMOD = tempBLDMOD | ((gbaSystemGlobal.GBABLDMOD & 0x404) << 1);
		REG_BLDCNT = tempBLDMOD;
	}
    break;
  case 0x52:
    gbaSystemGlobal.GBACOLEV = value & 0x1F1F;
    UPDATE_REG(0x52, gbaSystemGlobal.GBACOLEV);
    *(u16 *)(0x4000052) = value;
	break;
  case 0x54:
    gbaSystemGlobal.GBACOLY = value & 0x1F;
    UPDATE_REG(0x54, gbaSystemGlobal.GBACOLY);
	*(u16 *)(0x4000054) = value;
    break;
	
//SOUND EVENT: ORI
  case 0x60:
  case 0x62:
  case 0x64:
  case 0x68:
  case 0x6c:
  case 0x70:
  case 0x72:
  case 0x74:
  case 0x78:
  case 0x7c:
  case 0x80:
  case 0x84:
    //soundEvent(address&0xFF, (u8)(value & 0xFF));
    //soundEvent((address&0xFF)+1, (u8)(value>>8));
    //break; //ichfly disable sound
  case 0x82:
  //case 0x88: //bias setting is handled separately
  case 0xa0:
  case 0xa2:
  case 0xa4:
  case 0xa6:
  case 0x90:
  case 0x92:
  case 0x94:
  case 0x96:
  case 0x98:
  case 0x9a:
  case 0x9c:
  case 0x9e:{
        //soundEvent(address&0xFF, value);  //ichfly send sound to arm7
        
        #ifdef soundwriteprint
            printf("ur %04x to %08x\r\n",value,address);
        #endif
        
        //sound command writes
        //execute_arm7_command(0xc0700100,address,value);
        
        UPDATE_REG(address,value);
    }
    break;
    
    //bias setting //Delay Count (NDS/DSi only) (GBA uses a fixed delay count of 8)
    case 0x88:{
        //execute_arm7_command(0xc0700104,value,0x8);
    }

  case 0xB0:
    gbaSystemGlobal.GBADM0SAD_L = value;
    UPDATE_REG(0xB0, gbaSystemGlobal.GBADM0SAD_L);
    break;
  case 0xB2:
    gbaSystemGlobal.GBADM0SAD_H = value & 0x07FF;
    UPDATE_REG(0xB2, gbaSystemGlobal.GBADM0SAD_H);
    break;
  case 0xB4:
    gbaSystemGlobal.GBADM0DAD_L = value;
    UPDATE_REG(0xB4, gbaSystemGlobal.GBADM0DAD_L);
    break;
  case 0xB6:
    gbaSystemGlobal.GBADM0DAD_H = value & 0x07FF;
    UPDATE_REG(0xB6, gbaSystemGlobal.GBADM0DAD_H);
    break;
  case 0xB8:
    gbaSystemGlobal.GBADM0CNT_L = value & 0x3FFF;
    UPDATE_REG(0xB8, 0);
    break;
  case 0xBA:
    {
      bool start = ((gbaSystemGlobal.GBADM0CNT_H ^ value) & 0x8000) ? true : false;
      value &= 0xF7E0;

      gbaSystemGlobal.GBADM0CNT_H = value;
      UPDATE_REG(0xBA, gbaSystemGlobal.GBADM0CNT_H);    
    
      if(start && (value & 0x8000)) {
        dma0Source = gbaSystemGlobal.GBADM0SAD_L | (gbaSystemGlobal.GBADM0SAD_H << 16);
        dma0Dest = gbaSystemGlobal.GBADM0DAD_L | (gbaSystemGlobal.GBADM0DAD_H << 16);
        CPUCheckDMA(0, 1);
      }
    }
    break;      
  case 0xBC:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);    
	
    gbaSystemGlobal.GBADM1SAD_L = value;
    UPDATE_REG(0xBC, gbaSystemGlobal.GBADM1SAD_L);
    break;
  case 0xBE:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM1SAD_H = value & 0x0FFF;
    UPDATE_REG(0xBE, gbaSystemGlobal.GBADM1SAD_H);
    break;
  case 0xC0:
#ifdef dmawriteprint
    printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes 
    //execute_arm7_command(0xc0700100,address,value);
    
	gbaSystemGlobal.GBADM1DAD_L = value;
    UPDATE_REG(0xC0, gbaSystemGlobal.GBADM1DAD_L);
    break;
  case 0xC2:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM1DAD_H = value & 0x07FF;
    UPDATE_REG(0xC2, gbaSystemGlobal.GBADM1DAD_H);
    break;
  case 0xC4:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
	gbaSystemGlobal.GBADM1CNT_L = value & 0x3FFF;
    UPDATE_REG(0xC4, 0);
    break;
  case 0xC6:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    {
        bool start = ((gbaSystemGlobal.GBADM1CNT_H ^ value) & 0x8000) ? true : false;
        value &= 0xF7E0;
      
        gbaSystemGlobal.GBADM1CNT_H = value;
        UPDATE_REG(0xC6, gbaSystemGlobal.GBADM1CNT_H);
      
        if(start && (value & 0x8000)) {
            dma1Source = gbaSystemGlobal.GBADM1SAD_L | (gbaSystemGlobal.GBADM1SAD_H << 16);
            dma1Dest = gbaSystemGlobal.GBADM1DAD_L | (gbaSystemGlobal.GBADM1DAD_H << 16);
            CPUCheckDMA(0, 2);
        }
    }
    
    break;
  case 0xC8:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM2SAD_L = value;
    UPDATE_REG(0xC8, gbaSystemGlobal.GBADM2SAD_L);
    break;
  case 0xCA:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
	gbaSystemGlobal.GBADM2SAD_H = value & 0x0FFF;
    UPDATE_REG(0xCA, gbaSystemGlobal.GBADM2SAD_H);
    break;
  case 0xCC:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM2DAD_L = value;
    UPDATE_REG(0xCC, gbaSystemGlobal.GBADM2DAD_L);
    break;
  case 0xCE:
#ifdef dmawriteprint
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM2DAD_H = value & 0x07FF;
    UPDATE_REG(0xCE, gbaSystemGlobal.GBADM2DAD_H);
    break;
  case 0xD0:
#ifdef dmawriteprint

	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    gbaSystemGlobal.GBADM2CNT_L = value & 0x3FFF;
    UPDATE_REG(0xD0, 0);
    break;
  case 0xD2:
#ifdef dmawriteprint

	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
        
    {
        bool start = ((gbaSystemGlobal.GBADM2CNT_H ^ value) & 0x8000) ? true : false;
        value &= 0xF7E0;
      
        gbaSystemGlobal.GBADM2CNT_H = value;
        UPDATE_REG(0xD2, gbaSystemGlobal.GBADM2CNT_H);
        UPDATE_REG(0xD2, gbaSystemGlobal.GBADM2CNT_H);
      
        if(start && (value & 0x8000)) {
            dma2Source = gbaSystemGlobal.GBADM2SAD_L | (gbaSystemGlobal.GBADM2SAD_H << 16);
            dma2Dest = gbaSystemGlobal.GBADM2DAD_L | (gbaSystemGlobal.GBADM2DAD_H << 16);
            CPUCheckDMA(0, 4);
        }       
    }
    
    break;
  case 0xD4:
    gbaSystemGlobal.GBADM3SAD_L = value;
    UPDATE_REG(0xD4, gbaSystemGlobal.GBADM3SAD_L);
    break;
  case 0xD6:
    gbaSystemGlobal.GBADM3SAD_H = value & 0x0FFF;
    UPDATE_REG(0xD6, gbaSystemGlobal.GBADM3SAD_H);
    break;
  case 0xD8:
    gbaSystemGlobal.GBADM3DAD_L = value;
    UPDATE_REG(0xD8, gbaSystemGlobal.GBADM3DAD_L);
    break;
  case 0xDA:
    gbaSystemGlobal.GBADM3DAD_H = value & 0x0FFF;
    UPDATE_REG(0xDA, gbaSystemGlobal.GBADM3DAD_H);
    break;
  case 0xDC:
    gbaSystemGlobal.GBADM3CNT_L = value;
    UPDATE_REG(0xDC, 0);
    break;
  case 0xDE:
    {
        bool start = ((gbaSystemGlobal.GBADM3CNT_H ^ value) & 0x8000) ? true : false;
        
        value &= 0xFFE0;
        
        gbaSystemGlobal.GBADM3CNT_H = value;
        UPDATE_REG(0xDE, gbaSystemGlobal.GBADM3CNT_H);
        
        if(start && (value & 0x8000)) {
            dma3Source = gbaSystemGlobal.GBADM3SAD_L | (gbaSystemGlobal.GBADM3SAD_H << 16);
            dma3Dest = gbaSystemGlobal.GBADM3DAD_L | (gbaSystemGlobal.GBADM3DAD_H << 16);
            CPUCheckDMA(0,8);
        }
    }
    break;
 case 0x100:
    timer0Reload = value;
#ifdef printsoundtimer
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    UPDATE_REG(0x100, value);
    break;
  case 0x102:
    timer0Value = value;
    //timerOnOffDelay|=1;
    //cpuNextEvent = cpuTotalTicks;
	UPDATE_REG(0x102, value);
#ifdef printsoundtimer
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);

	/*if(timer0Reload & 0x8000)
	{
		if((value & 0x3) == 0)
		{
			*(u16 *)(0x4000100) = timer0Reload >> 5;
			*(u16 *)(0x4000102) = value + 1;
			break;
		}
		if((value & 0x3) == 1)
		{
			*(u16 *)(0x4000100) = timer0Reload >> 1;
			*(u16 *)(0x4000102) = value + 1;
			break;
		}
		if((value & 3) == 2)
		{
			*(u16 *)(0x4000100) = timer0Reload >> 1;
			*(u16 *)(0x4000102) = value + 1;
			break;
		}
		*(u16 *)(0x4000102) = value;
		printf("big reload0\r\n");//todo 
	}
	else*/
	{	
		*(u16 *)(0x4000100) = timer1Reload << 1;
		*(u16 *)(0x4000102) = value;
	}
    break;
  case 0x104:
    timer1Reload = value;
#ifdef printsoundtimer
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);

	UPDATE_REG(0x104, value);
	break;
  case 0x106:
#ifdef printsoundtimer
	printf("ur %04x to %08x\r\n",value,address);
#endif

    //sound command writes
    //execute_arm7_command(0xc0700100,address,value);
    
    timer1Value = value;
    //timerOnOffDelay|=2;
    //cpuNextEvent = cpuTotalTicks;
	UPDATE_REG(0x106, value);

	/*if(timer1Reload & 0x8000)
	{
		if((value & 0x3) == 0)
		{
			*(u16 *)(0x4000104) = timer1Reload >> 5;
			*(u16 *)(0x4000106) = value + 1;
			break;
		}
		if((value & 0x3) == 1)
		{
			*(u16 *)(0x4000104) = timer1Reload >> 1;
			*(u16 *)(0x4000106) = value + 1;
			break;
		}
		if((value & 3) == 2)
		{
			*(u16 *)(0x4000104) = timer1Reload >> 1;
			*(u16 *)(0x4000106) = value + 1;
			break;
		}
		*(u16 *)(0x4000106) = value;
		printf("big reload1\r\n");//todo 
	}
	else*/
	{	
		*(u16 *)(0x4000104) = timer1Reload << 1;
		*(u16 *)(0x4000106) = value;
	}
	  break;
  case 0x108:
    timer2Reload = value;
	UPDATE_REG(0x108, value);
	*(u16 *)(0x4000108) = value;
    break;
  case 0x10A:
    timer2Value = value;
    //timerOnOffDelay|=4;
    //cpuNextEvent = cpuTotalTicks;
	UPDATE_REG(0x10A, value);

	/*if(timer2Reload & 0x8000)
	{
		if((value & 0x3) == 0)
		{
			*(u16 *)(0x4000108) = timer2Reload >> 5;
			*(u16 *)(0x400010A) = value + 1;
			break;
		}
		if((value & 0x3) == 1)
		{
			*(u16 *)(0x4000108) = timer2Reload >> 1;
			*(u16 *)(0x400010A) = value + 1;
			break;
		}
		if((value & 3) == 2)
		{
			*(u16 *)(0x4000108) = timer2Reload >> 1;
			*(u16 *)(0x400010A) = value + 1;
			break;
		}
		printf("big reload2\r\n");//todo 
		*(u16 *)(0x400010A) = value;
	}
	else*/
	{	
		*(u16 *)(0x4000108) = timer2Reload << 1;
		*(u16 *)(0x400010A) = value;
	}
	  break;
  case 0x10C:
    timer3Reload = value;
	UPDATE_REG(0x10C, value);
	  break;
  case 0x10E:
    timer3Value = value;
    //timerOnOffDelay|=8;
    //cpuNextEvent = cpuTotalTicks;
	UPDATE_REG(0x10E, value);

	/*if(timer3Reload & 0x8000)
	{
		if((value & 0x3) == 0)
		{
			*(u16 *)(0x400010C) = timer3Reload >> 5;
			*(u16 *)(0x400010E) = value + 1;
			break;
		}
		if((value & 0x3) == 1)
		{
			*(u16 *)(0x400010C) = timer3Reload >> 1;
			*(u16 *)(0x400010E) = value + 1;
			break;
		}
		if((value & 3) == 2)
		{
			*(u16 *)(0x400010C) = timer3Reload >> 1;
			*(u16 *)(0x400010E) = value + 1;
			break;
		}
		printf("big reload3\r\n");//todo 
		*(u16 *)(0x400010E) = value;
	}
	else*/
    
    /* //coto: timer3 is used by wifi
	{	
		*(u16 *)(0x400010C) = timer3Reload << 1;
		*(u16 *)(0x400010E) = value;
	}
    */
  break;
  case 0x128:
    if(value & 0x80) {
      value &= 0xff7f;
      if(value & 1 && (value & 0x4000)) {
        UPDATE_REG(0x12a, 0xFF);
        GBAIF |= 0x80;
        UPDATE_REG(0x202, GBAIF);
        value &= 0x7f7f;
      }
    }
    UPDATE_REG(0x128, value);
    break;
  case 0x130:
    //gbaSystemGlobal.GBAP1 |= (value & 0x3FF); //ichfly readonly
    //UPDATE_REG(0x130, gbaSystemGlobal.GBAP1);
    break;
  case 0x132:
    UPDATE_REG(0x132, value & 0xC3FF);
	break;
  case 0x200:
    GBAIE = value & 0x3FFF;
    UPDATE_REG(0x200, GBAIE);
    
    /*if ((GBAIME & 1) && (GBAIF & GBAIE) && armIrqEnable)
      cpuNextEvent = cpuTotalTicks;*/
	
    break;
  case 0x202:
    //GBAIF = value & 0x3FFF;
    //UPDATE_REG(0x204, GBAIF);
    
    break;
  case 0x204:
    { //ichfly can't emulate that
      /*memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakRamWaitState[value & 3];
      
      if(!speedHack) {
        memoryWait[0x08] = memoryWait[0x09] = gamepakWaitState[(value >> 2) & 3];
        memoryWaitSeq[0x08] = memoryWaitSeq[0x09] =
          gamepakWaitState0[(value >> 4) & 1];
        
        memoryWait[0x0a] = memoryWait[0x0b] = gamepakWaitState[(value >> 5) & 3];
        memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] =
          gamepakWaitState1[(value >> 7) & 1];
        
        memoryWait[0x0c] = memoryWait[0x0d] = gamepakWaitState[(value >> 8) & 3];
        memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] =
          gamepakWaitState2[(value >> 10) & 1];
      } else {
        memoryWait[0x08] = memoryWait[0x09] = 3;
        memoryWaitSeq[0x08] = memoryWaitSeq[0x09] = 1;
        
        memoryWait[0x0a] = memoryWait[0x0b] = 3;
        memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] = 1;
        
        memoryWait[0x0c] = memoryWait[0x0d] = 3;
        memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] = 1;
      }
         
      for(int i = 8; i < 15; i++) {
        memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
        memoryWaitSeq32[i] = memoryWaitSeq[i]*2 + 1;
      }

      if((value & 0x4000) == 0x4000) {
        busPrefetchEnable = true;
        busPrefetch = false;
        busPrefetchCount = 0;
      } else {
        busPrefetchEnable = false;
        busPrefetch = false;
        busPrefetchCount = 0;
      }*/
      UPDATE_REG(0x204, value & 0x7FFF);

    }
    break;
  case 0x208:
    GBAIME = value & 1;
    UPDATE_REG(0x208, GBAIME);
	/*if ((GBAIME & 1) && (GBAIF & GBAIE) && armIrqEnable)
      cpuNextEvent = cpuTotalTicks;*/
    break;
  case 0x300:
    if(value != 0) //ichfly this is todo
      value &= 0xFFFE;
    UPDATE_REG(0x300, value);
    break;
  default:
    UPDATE_REG(address&0x3FE, value);
    break;
  }
}


//debug!
u32 debuggeroutput(){
u32 lnk_ptr;
		__asm__ volatile (
		"mov %[lnk_ptr],#0;" "\n\t"
		"add %[lnk_ptr],%[lnk_ptr], lr" "\n\t"//"sub lr, lr, #8;" "\n\t"
		"sub %[lnk_ptr],%[lnk_ptr],#0x8" 
		:[lnk_ptr] "=r" (lnk_ptr)
		);
printf("\n LR callback trace at %x \n", (unsigned int)lnk_ptr);
return 0;
}


//fetch current address
u32 armfetchpc_arm(u32 address){
return CPUReadMemory(address);
}

u16 armfetchpc_thumb(u32 address){
return CPUReadHalfWord(address);
}

//prefetch next opcode
u32 armnextpc(u32 address){
//0:ARM | 1:THUMB
if(armState)
	return CPUReadMemory(address+0x4);
//else
return CPUReadHalfWord(address+0x2);
}

//readjoypad
u32 systemreadjoypad(int which){
	u32 res = keysPressed();
		// disallow L+R or U+D of being pressed at the same time
		if((res & 48) == 48)
		res &= ~16;
		if((res & 192) == 192)
		res &= ~128;
	return res;
}


//



//stacked CPU Read/Writes

u32 CPUReadMemory_stack(u32 address)
{
	return CPUReadMemory(address);
}

u16 CPUReadHalfWord_stack(u32 address)
{
	return CPUReadHalfWord(address);
}

u8 CPUReadByte_stack(u32 address)
{
	return CPUReadByte(address);
}

s16 CPUReadHalfWordSigned_stack(u32 address)
{
	return (s16)CPUReadHalfWordSigned(address);
}

s8 CPUReadByteSigned_stack(u32 address)
{
	return (s8)CPUReadByteSigned(address);
}



void CPUWriteMemory_stack(u32 address, u32 value)
{
	CPUWriteMemory(address,value);
}

void CPUWriteHalfWord_stack(u32 address, u16 value)
{
	CPUWriteHalfWord(address,value);
}

void CPUWriteByte_stack(u32 address, u8 b)
{
	CPUWriteByte(address,b);
}


//normal cpu read/write opcodes
/* //ori: removed because we use vcounter thread
__attribute__((section(".itcm")))
void updateVC()
{
		u32 temp = (REG_VCOUNT&0xff);
		u32 temp2 = REG_DISPSTAT;
		//printf("Vcountreal: %08x\n",temp);
		//u16 help3;
		gbaSystemGlobal.GBAVCOUNT = temp;
		gbaSystemGlobal.GBADISPSTAT &= 0xFFF8; //reset h-blanc and V-Blanc and V-Count Setting
		//if(help3 == gbaSystemGlobal.GBAVCOUNT) //else it is a extra long V-Line // ichfly todo it is to slow
		//{
			gbaSystemGlobal.GBADISPSTAT |= (temp2 & 0x3); //temporary patch get original settings
		//}
		//if(gbaSystemGlobal.GBAVCOUNT > 160 && gbaSystemGlobal.GBAVCOUNT != 227)gbaSystemGlobal.GBADISPSTAT |= 1;//V-Blanc
		UPDATE_REG(0x06, gbaSystemGlobal.GBAVCOUNT);
		if(gbaSystemGlobal.GBAVCOUNT == (gbaSystemGlobal.GBADISPSTAT >> 8)) //update by V-Count Setting
		{
			gbaSystemGlobal.GBADISPSTAT |= 0x4;
			//if(gbaSystemGlobal.GBADISPSTAT & 0x20) {
			//  GBAIF |= 4;
			//  UPDATE_REG(0x202, GBAIF);
			//}
		}
		UPDATE_REG(0x04, gbaSystemGlobal.GBADISPSTAT);
		//printf("Vcountreal: %08x\n",temp);
		//printf("gbaSystemGlobal.GBADISPSTAT: %08x\n",temp2);
}
*/


//CPU Read/Writes

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
u32 CPUReadMemory(u32 address)
{
  
#ifdef printreads
  printf("r32 %08x\n",address);
#endif
  u32 value=0;
  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
				//coto:
				#ifdef require_gbabios
					value = READ32LE(((u8 *)&bios[address & 0x3FFC]));
				#else
					value = READ32LE(((u8 *)&biosProtected));
				#endif
			}
			else 
				goto unreadable;
	break;
  case 2:
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unreadable;
#endif
    value = READ32LE(((u8 *)&workRAM[address & 0x3FFFC]));
    break;
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    value = READ32LE(((u8 *)&internalRAM[address & 0x7ffC]));
    break;
  case 4:
	//ori:
	if(address > 0x40000FF && address < 0x4000111)
	{
		//todo timer shift
		value = *(u32 *)(address);
		break;
	}
    
	if(address == 0x4000202 || address == 0x4000200)//ichfly update
	{
		GBAIF = *(vuint16*)0x04000214; //VBlanc
		UPDATE_REG(0x202, GBAIF);
	}
	
	
	if(address > 0x4000003 && address < 0x4000008)//ichfly update
	{
		//oriupdateVC();
        ////vcounthandler(); //shouldnt do this but helps the vcounter
	}
	
	if(address==0x04000006){
		value = gbaSystemGlobal.GBAVCOUNT;
		break;
	}
	
    if((address < 0x4000400) && ioReadable[address & 0x3fc]) {
      if(ioReadable[(address & 0x3fc) + 2])
        value = READ32LE(((u8 *)&ioMem[address & 0x3fC]));
      else
        value = READ16LE(((u8 *)&ioMem[address & 0x3fc]));
    } else goto unreadable;
    
    
	/*
	switch(address){
		case(0x04000100):
		case(0x04000102):
		case(0x04000104):
		case(0x04000106):
		case(0x04000108):
		case(0x0400010a):
		case(0x0400010c):
		case(0x0400010e):
		case(0x04000110):
			//todo timer shift
			value = *(u32 *)(address);
		break;
		break;
		
		case(0x04000200):
		case(0x04000202):{
			GBAIF = *(vuint16*)0x04000214; //VBlanc
			UPDATE_REG(0x202, GBAIF);
		}
		break;
		
		case(0x04000004):
		case(0x04000005):
		case(0x04000007):
			//vcounthandler();
		break;
		
		case(0x04000006):
			//vcounthandler();
			value = gbaSystemGlobal.GBAVCOUNT;
		break;	
	}
	
	if((address < 0x4000400) && ioReadable[address & 0x3fc]) {
		if(ioReadable[(address & 0x3fc) + 2])
			value = READ32LE(((u8 *)&ioMem[address & 0x3fC]));
		else
			value = READ16LE(((u8 *)&ioMem[address & 0x3fc]));
    }
	else
		goto unreadable;
    
    */
    
	break;
  case 5:
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unreadable;
#endif
    value = READ32LE(((u8 *)&paletteRAM[address & 0x3fC]));
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffc);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    
    //ori: 
    value = READ32LE(((u8 *)&vram[address]));
    
    /*
    //new: detect source gba addresses here and redirect to nds ppu engine
    if(gba_bg_mode == 0){
        //u8 bg0mapblock = ((gbaSystemGlobal.GBABG0CNT >> 8) & 0x1f); // 2 KBytes each unit
        //u8 bg0charblock = ((gbaSystemGlobal.GBABG0CNT >> 2) & 0x3); // 16 KBytes each unit
        
        //u8 * dest_addr = (u8*)bgGetGfxPtr(bg_0_setting) + (u8*)(bg0charblock * (16*1024));
        
        if( ((u32)bgGetGfxPtr(bg_0_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_0_setting) + address) );
        else if( ((u32)bgGetGfxPtr(bg_1_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_1_setting) + address) );
        else if( ((u32)bgGetGfxPtr(bg_2_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_2_setting) + address) );
        else if( ((u32)bgGetGfxPtr(bg_3_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_3_setting) + address) );
        
        
    }
    else if(gba_bg_mode == 1){
        if( ((u32)bgGetGfxPtr(bg_0_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_0_setting) + address) );
        else if( ((u32)bgGetGfxPtr(bg_1_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)(bgGetGfxPtr(bg_1_setting) + address) );
        else if( ((u32)bgGetMapPtr(bg_2_setting) & 0x1fffc) > address)
            value = READ32LE( (u8*)( bgGetMapPtr(bg_2_setting) + address) );
        
    }
    */
    break;
  case 7:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
    value = READ32LE(((u8 *)&oam[address & 0x3FC]));
    break;
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
    /*
	if( (int)(address&0x1FFFFFC) < romSize){						//our offset is inside the GBA Image (2MB)
		value = READ32LE(((u8 *)&rom[address & 0x1FFFFFC]));
	}
	else		//it's a valid GBA read
	{
		value = ichfly_readu32(address&0x1FFFFFC);
	}
	*/
    value = ichfly_readu32((int)address & 0x1FFFFFC);
    break;
  case 13:
    if(cpuEEPROMEnabled)
      // no need to swap this
      return eepromRead(address);
    goto unreadable;
  case 14:
    if(cpuFlashEnabled | cpuSramEnabled)
      // no need to swap this
      return flashRead(address);
    // default
  default:
  unreadable:
  //while(1);
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal word read: %08x\n", address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif

    //if(cpuDmaHack) { //only this is possible here
      value = cpuDmaLast;
  }

  if(address & 3) {
	
    int shift = (address & 3) << 3;
    value = (value >> shift) | (value << (32 - shift));
  }
  return value;
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
u16 CPUReadHalfWord(u32 address) //ichfly not inline is faster because it is smaler
{
#ifdef printreads
	printf("r16 %08x\n",address);
#endif
#ifdef DEV_VERSION      
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned halfword read: %08x at %08x\n", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  u32 value=0;
  
  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
			//coto:
			#ifdef require_gbabios
				value = READ16LE(((u8 *)&bios[address & 0x3FFE]));
			#else
				value = READ16LE(((u8 *)&biosProtected[address&2]));
			#endif	
			}
			else 
				goto unreadable;
		
    break;
  case 2:
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unreadable;
#endif
    value = READ16LE(((u8 *)&workRAM[address & 0x3FFFE]));
    break;
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    value = READ16LE(((u8 *)&internalRAM[address & 0x7ffe]));
    break;
  case 4:
	
    if(address > 0x40000FF && address < 0x4000111)
	{
		
		if((address&0x2) == 0)
		{
			if(ioMem[address & 0x3fe] & 0x8000)
			{
				value = ((*(u16 *)(address)) >> 1) | 0x8000;
			}
			else
			{
				value = (*(u16 *)(address)) >> 1;
			}
			return value;
		}
	}
	
	
	if(address > 0x4000003 && address < 0x4000008)//ichfly update
	{
		//ori:updateVC();
        //vcounthandler();
    }
	
	if(address==0x04000006){
		value = gbaSystemGlobal.GBAVCOUNT;
		break;
	}
	
	
    
	if(address == 0x4000202)//ichfly update
	{
		GBAIF = *(vuint16*)0x04000214;
		UPDATE_REG(0x202, GBAIF);
	}

	
    if((address < 0x4000400) && ioReadable[address & 0x3fe])
    {
		value =  READ16LE(((u8 *)&ioMem[address & 0x3fe]));
    }
    else 
        goto unreadable;
    
	
    /*
	switch(address){
		case(0x04000100):
		case(0x04000102):
		case(0x04000104):
		case(0x04000106):
		case(0x04000108):
		case(0x0400010a):
		case(0x0400010c):
		case(0x0400010e):
		case(0x04000110):
			if((address&0x2) == 0)
			{
				if(ioMem[address & 0x3fe] & 0x8000)
				{
					value = ((*(u16 *)(address)) >> 1) | 0x8000;
				}
				else
				{
					value = (*(u16 *)(address)) >> 1;
				}
				return value;
			}
		break;
		
		case(0x04000200):
		case(0x04000202):{
			GBAIF = *(vuint16*)0x04000214; //VBlanc
			UPDATE_REG(0x202, GBAIF);
		}
		break;
		
		case(0x04000004):
		case(0x04000005):
		case(0x04000007):
			//vcounthandler();
		break;
		
		case(0x04000006):
			//vcounthandler();
			value = gbaSystemGlobal.GBAVCOUNT;
		break;	
	}
	
	if((address < 0x4000400) && ioReadable[address & 0x3fe])
    {
		value =  READ16LE(((u8 *)&ioMem[address & 0x3fe]));
    }
    else 
		goto unreadable;
	
    */
    
	break;
  case 5:
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unreadable;
#endif
    value = READ16LE(((u8 *)&paletteRAM[address & 0x3fe]));
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffe);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    value = READ16LE(((u8 *)&vram[address]));
    break;
  case 7:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
    value = READ16LE(((u8 *)&oam[address & 0x3fe]));
    break;
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
	switch(address){
		case 0x80000c4:case 0x80000c6:case 0x80000c8:
			value = rtcRead(address);
		break;
		
		default:
            /*
			if( (int)(address&0x1FFFFFE) < romSize){						//our offset is inside the GBA Image (2MB)
				value = READ16LE(((u8 *)&rom[address & 0x1FFFFFE]));
			}
			else//it's a valid GBA read
			{
				value = ichfly_readu16(address&0x1FFFFFE);
			}
			*/
            value = ichfly_readu16((int)address & 0x1FFFFFE);
		break;
	}
	break;
  case 13:
#ifdef printsaveread
	  printf("%X\n\r",address);
#endif
    if(cpuEEPROMEnabled)
      // no need to swap this
      return  eepromRead(address);
    goto unreadable;
  case 14:
#ifdef printsaveread
	  printf("%X\n\r",address);
#endif
    if(cpuFlashEnabled | cpuSramEnabled)
      // no need to swap this
      return flashRead(address);
    // default
  default:
  unreadable:

#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal hword read: %08x\n", address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif

	value = cpuDmaLast & 0xFFFF;
    break;
  }

  if(address & 1) {
    value = (value >> 8) | (value << 24);
  }
  
  return value;
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
s16 CPUReadHalfWordSigned(u32 address)
{
  u16 value = CPUReadHalfWord(address);
  if((address & 1))
    value = (s8)value;
  return value;
}


s8 CPUReadByteSigned(u32 address)
{
	return (s8)CPUReadByte(address);
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
u8 CPUReadByte(u32 address)
{
#ifdef printreads
printf("r8 %02x\n",address);
#endif

  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
				//coto: 
				#ifdef require_gbabios
					return bios[address & 0x3FFF];
				#else
					return biosProtected[address & 3];
				#endif
			} 
			else 
				goto unreadable;
		
  break;
  case 2:
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unreadable;
#endif
    return workRAM[address & 0x3FFFF];
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    return internalRAM[address & 0x7fff];
  case 4:
  
	if(address > 0x40000FF && address < 0x4000111)
	{
		//todo timer shift
		return *(u8 *)(address);
	}
	
	if(address > 0x4000003 && address < 0x4000008)//ichfly update
	{
		//ori: updateVC();
        //vcounthandler();
    }
	
	
	if(address==0x04000006){
		return (u8)gbaSystemGlobal.GBAVCOUNT;
		break;
	}
	
    
	if(address == 0x4000202 || address == 0x4000203)//ichfly update
	{
		GBAIF = *(vuint16*)0x04000214; //VBlanc
		UPDATE_REG(0x202, GBAIF);
	}
    
    if((address < 0x4000400) && ioReadable[address & 0x3ff])
      return ioMem[address & 0x3ff];
    else goto unreadable;
  
  /*
  switch(address){
		case(0x04000100):
		case(0x04000102):
		case(0x04000104):
		case(0x04000106):
		case(0x04000108):
		case(0x0400010a):
		case(0x0400010c):
		case(0x0400010e):
		case(0x04000110):
			//todo timer shift
			return *(u8 *)(address);
		
		break;
		
		case(0x04000202):
		case(0x04000203):{
			GBAIF = *(vuint16*)0x04000214; //VBlanc
			UPDATE_REG(0x202, GBAIF);
		}
		break;
		
		case(0x04000004):
		case(0x04000005):
		case(0x04000007):
			//vcounthandler();
		break;
		
		case(0x04000006):
			//vcounthandler();
			return (u8)gbaSystemGlobal.GBAVCOUNT;
		break;	
	}
	if((address < 0x4000400) && ioReadable[address & 0x3ff])
		return ioMem[address & 0x3ff];
	else
		goto unreadable;
	*/
    
	break;
  case 5:
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unreadable;
#endif
    return paletteRAM[address & 0x3ff];
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1ffff);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return 0;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    return vram[address];
  case 7:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
    return oam[address & 0x3ff];
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
    /*
	if( (int)(address&0x1FFFFFF) < romSize){						//our offset is inside the GBA Image (2MB)
		return rom[address & 0x1FFFFFF];
	}
	else        //it's a valid GBA read
	{
		return ichfly_readu8(address&0x1FFFFFF);
	}
	*/
    
    return ichfly_readu8((int)address & 0x1FFFFFF);
    
	break;

  case 13:
#ifdef printsaveread
	  printf("%X\n\r",address);
#endif
    if(cpuEEPROMEnabled)
      return eepromRead(address);
    goto unreadable;
  case 14:
#ifdef printsaveread
	  printf("%X\n\r",address);
#endif
    if(cpuSramEnabled | cpuFlashEnabled)
      return flashRead(address);
    if(cpuEEPROMSensorEnabled) {
      switch(address & 0x00008f00) {
      case 0x8200:
        return systemGetSensorX() & 255;
      case 0x8300:
        return (systemGetSensorX() >> 8)|0x80;
      case 0x8400:
        return systemGetSensorY() & 255;
      case 0x8500:
        return systemGetSensorY() >> 8;
      }
    }
    // default
  default:
  unreadable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal byte read: %08x\n", address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif

      return cpuDmaLast & 0xFF;
    break;
  }
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
void CPUWriteMemory(u32 address, u32 value) //ichfly not inline is faster because it is smaler
{
#ifdef printreads
    printf("w32 %08x to %08x\n",value,address);
#endif		  
#ifdef DEV_VERSION
  if(address & 3) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned word write: %08x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  switch(address >> 24) {
  case 0x02:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeWorkRAM[address & 0x3FFFC]))
      cheatsWriteMemory(address & 0x203FFFC,
                        value);
    else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unreadable;
#endif
      WRITE32LE(((u8 *)&workRAM[address & 0x3FFFC]), value);
    break;
  case 0x03:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeInternalRAM[address & 0x7ffc]))
      cheatsWriteMemory(address & 0x3007FFC,
                        value);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)) //upern mirrow
		goto unreadable;
#endif
      WRITE32LE(((u8 *)&internalRAM[address & 0x7ffC]), value);
    break;
  case 0x04:
    if(address < 0x4000400) {
		CPUUpdateRegister((address & 0x3FC), value & 0xFFFF);
		CPUUpdateRegister((address & 0x3FC) + 2, (value >> 16));
	} 
	else 
		goto unwritable;
    break;
  case 0x05:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezePRAM[address & 0x3fc]))
      cheatsWriteMemory(address & 0x70003FC,
                        value);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unreadable;
#endif
    WRITE32LE(((u8 *)&paletteRAM[address & 0x3FC]), value);
    break;
  case 0x06:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffc);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeVRAM[address]))
      cheatsWriteMemory(address + 0x06000000, value);
    else
#endif
    
    WRITE32LE(((u8 *)&vram[address]), value);
    break;
  case 0x07:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeOAM[address & 0x3fc]))
      cheatsWriteMemory(address & 0x70003FC,
                        value);
    else
#endif
    WRITE32LE(((u8 *)&oam[address & 0x3fc]), value);
    break;
  case 0x0D:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,value);
#endif
    if(cpuEEPROMEnabled) {
      eepromWrite(address, value);
      break;
    }
    goto unwritable;
  case 0x0E:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,value);
#endif
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    // default
  default:
  unwritable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal word write: %08x to %08x\n",value, address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif
    break;
  }
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
void CPUWriteHalfWord(u32 address, u16 value)
{
#ifdef printreads
printf("w16 %04x to %08x\r\n",value,address);
#endif

#ifdef DEV_VERSION
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned halfword write: %04x to %08x from %08x\n",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeWorkRAM[address & 0x3FFFE]))
      cheatsWriteHalfWord(address & 0x203FFFE,
                          value);
    else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unwritable;
#endif
      WRITE16LE(((u8 *)&workRAM[address & 0x3FFFE]),value);
    break;
  case 3:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeInternalRAM[address & 0x7ffe]))
      cheatsWriteHalfWord(address & 0x3007ffe,
                          value);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)) //upern mirrow
		goto unwritable;
#endif
      WRITE16LE(((u8 *)&internalRAM[address & 0x7ffe]), value);
    break;    
  case 4:
	if(address < 0x4000400)
		CPUUpdateRegister(address & 0x3fe, value);
    else 
		goto unwritable;
  break;
  case 5:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezePRAM[address & 0x03fe]))
      cheatsWriteHalfWord(address & 0x70003fe,
                          value);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unwritable;
#endif
    WRITE16LE(((u8 *)&paletteRAM[address & 0x3fe]), value);
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unwritable;
#endif
    address = (address & 0x1fffe);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeVRAM[address]))
      cheatsWriteHalfWord(address + 0x06000000,
                          value);
    else
#endif
    WRITE16LE(((u8 *)&vram[address]), value); 
    break;
  case 7:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeOAM[address & 0x03fe]))
      cheatsWriteHalfWord(address & 0x70003fe,
                          value);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unwritable;
#endif
    WRITE16LE(((u8 *)&oam[address & 0x3fe]), value);
    break;
  case 8:
  case 9:
    switch(address){
		case 0x080000c4: case 0x080000c6: case 0x080000c8:
			if(!rtcWrite(address, value)){
				//printf("RTC write failed \n");
				goto unwritable;
			}
		break;
	}
	break;
  case 13:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,value);
#endif
    if(cpuEEPROMEnabled) {
      eepromWrite(address, (u8)value);
      break;
    }
    goto unwritable;
  case 14:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,value);
#endif
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    goto unwritable;
  default:
  unwritable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal hword write: %04x to %08x\n",value, address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif
    break;
  }
}

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
void CPUWriteByte(u32 address, u8 b)
{
#ifdef printreads
	printf("w8 %02x to %08x\r\n",b,address);
#endif
  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
      if(freezeWorkRAM[address & 0x3FFFF])
        cheatsWriteByte(address & 0x203FFFF, b);
      else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unwritable;
#endif
        workRAM[address & 0x3FFFF] = b;
    break;
  case 3:
#ifdef BKPT_SUPPORT
    if(freezeInternalRAM[address & 0x7fff])
      cheatsWriteByte(address & 0x3007fff, b);
    else
#endif
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unwritable;
#endif
      internalRAM[address & 0x7fff] = b;
    break;
  case 4:
	
	if(address < 0x4000400) {
		switch(address & 0x3FF) {
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x68:
			case 0x69:
			case 0x6c:
			case 0x6d:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x78:
			case 0x79:
			case 0x7c:
			case 0x7d:
			case 0x80:
			case 0x81:
			case 0x84:
			case 0x85:
			case 0x90:
			case 0x91:
			case 0x92:
			case 0x93:
			case 0x94:
			case 0x95:
			case 0x96:
			case 0x97:
			case 0x98:
			case 0x99:
			case 0x9a:
			case 0x9b:
			case 0x9c:
			case 0x9d:
			case 0x9e:
			case 0x9f:
				//ori: soundEvent(gba, address&0xFF, b); //coto: sound writes are cpu write bytes (strb)
				//execute_arm7_command(0xc0700100,(u32)address&0xFF, (u32)b);
			break;
			case 0x301: // HALTCNT, undocumented
				//should we use NDS HALTCNT?
			break;
			default: // every other register
				u32 lowerBits = address & 0x3fe;
				if(address & 1) {
					CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0x00FF) | (b << 8));
				} 
				else 
				{
					CPUUpdateRegister(lowerBits, (READ16LE(&ioMem[lowerBits]) & 0xFF00) | b);
				}
		}
	}
	else 
		goto unwritable;

    break;
	
  case 5:
#ifdef checkclearaddrrw
	if(address > 0x05000400)goto unwritable;
#endif
    // no need to switch
    *((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unwritable;
#endif
    address = (address & 0x1fffe);
    if (((gbaSystemGlobal.GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

    // no need to switch 
    // byte writes to OBJ VRAM are ignored
    if ((address) < objTilesAddress[((gbaSystemGlobal.GBADISPCNT&7)+1)>>2])
    {
#ifdef BKPT_SUPPORT
      if(freezeVRAM[address])
        cheatsWriteByte(address + 0x06000000, b);
      else
#endif  
            *((u16 *)&vram[address]) = (b << 8) | b;
    }
    break;
  case 7:
#ifdef checkclearaddrrw
	goto unwritable;
#endif
    // no need to switch
    // byte writes to OAM are ignored
    //    *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
    break;    
  case 13:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,b);
#endif

    if(cpuEEPROMEnabled) {
      eepromWrite(address, b);
      break;
    }
    goto unwritable;
  case 14:
#ifdef printsavewrite
	  	  printf("%X %X\n\r",address,b);
#endif
      if (!(saveType == 5) && (!eepromInUse | cpuSramEnabled | cpuFlashEnabled)) {

    //if(!cpuEEPROMEnabled && (cpuSramEnabled | cpuFlashEnabled)) { 

        (*cpuSaveGameFunc)(address, b);
      break;
    }
    // default
  default:
  unwritable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x\n", address,reg[15].I);
	  printf("Illegal byte write: %02x to %08x\n",b, address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif
    break;
  }
}