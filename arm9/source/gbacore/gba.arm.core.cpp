#include "gba.arm.core.h"
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <time.h> 
#include "RTC.h"
#include "Util.h"
#include "translator.h"
#include "bios.h"
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
#include "main.h"
#include "savechip.h"
#include "EEprom.h"
#include "Flash.h"
#include "Sram.h"
#include "System.h"
#include "keypadTGDS.h"

u16 GBADISPCNT = 0;
u16 GBAIE       = 0x0000;
u16 GBAIF       = 0x0000;
u16 GBAIME      = 0x0000;
u16 GBADISPSTAT = 0;
u16 GBAVCOUNT = 0;
u16 GBABG0CNT = 0;
u16 GBABG1CNT = 0;
u16 GBABG2CNT = 0;
u16 GBABG3CNT = 0;
u16 GBABG0HOFS = 0;
u16 GBABG0VOFS = 0;
u16 GBABG1HOFS = 0;
u16 GBABG1VOFS = 0;
u16 GBABG2HOFS = 0;
u16 GBABG2VOFS = 0;
u16 GBABG3HOFS = 0;
u16 GBABG3VOFS = 0;
u16 GBABG2PA = 0;
u16 GBABG2PB = 0;
u16 GBABG2PC = 0;
u16 GBABG2PD = 0;
u16 GBABG2X_L = 0;
u16 GBABG2X_H = 0;
u16 GBABG2Y_L = 0;
u16 GBABG2Y_H = 0;
u16 GBABG3PA = 0;
u16 GBABG3PB = 0;
u16 GBABG3PC = 0;
u16 GBABG3PD = 0;
u16 GBABG3X_L = 0;
u16 GBABG3X_H = 0;
u16 GBABG3Y_L = 0;
u16 GBABG3Y_H = 0;
u16 GBAWIN0H = 0;
u16 GBAWIN1H = 0;
u16 GBAWIN0V = 0;
u16 GBAWIN1V = 0;
u16 GBAWININ = 0;
u16 GBAWINOUT = 0;
u16 GBAMOSAIC = 0;
u16 GBABLDMOD = 0;
u16 GBACOLEV = 0;
u16 GBACOLY = 0;
u16 GBADM0SAD_L = 0;
u16 GBADM0SAD_H = 0;
u16 GBADM0DAD_L = 0;
u16 GBADM0DAD_H = 0;
u16 GBADM0CNT_L = 0;
u16 GBADM0CNT_H = 0;
u16 GBADM1SAD_L = 0;
u16 GBADM1SAD_H = 0;
u16 GBADM1DAD_L = 0;
u16 GBADM1DAD_H = 0;
u16 GBADM1CNT_L = 0;
u16 GBADM1CNT_H = 0;
u16 GBADM2SAD_L = 0;
u16 GBADM2SAD_H = 0;
u16 GBADM2DAD_L = 0;
u16 GBADM2DAD_H = 0;
u16 GBADM2CNT_L = 0;
u16 GBADM2CNT_H = 0;
u16 GBADM3SAD_L = 0;
u16 GBADM3SAD_H = 0;
u16 GBADM3DAD_L = 0;
u16 GBADM3DAD_H = 0;
u16 GBADM3CNT_L = 0;
u16 GBADM3CNT_H = 0;
u16 GBATM0D = 0;
u16 GBATM0CNT = 0;
u16 GBATM1D = 0;
u16 GBATM1CNT = 0;
u16 GBATM2D = 0;
u16 GBATM2CNT = 0;
u16 GBATM3D = 0;
u16 GBATM3CNT = 0;
u16 GBAP1 = 0;

patch_t patchheader;
int   sound_clock_ticks = 0;
memoryMap map[256];
bool ioReadable[0x400];
bool cpuStart = false;				//1 CPU Running / 0 CPU Halt
bool armIrqEnable = true;
u32 armNextPC = 0x00000000;
int armMode = 0x1f;
u32 stop = 0x08000568;              //stop address
int saveType = 0;
bool useBios = false;
bool skipBios = false;
int frameSkip = 1;
bool speedup = false;
bool synchronize = true;
bool cpuDisableSfx = false;
bool cpuIsMultiBoot = false;
bool parseDebug = true;
int layerSettings = 0xff00;
int layerEnable = 0xff00;
bool speedHack = false;
int cpuSaveType = 0;
bool cheatsEnabled = true;
bool mirroringEnable = false;
int   soundtTicks = 0;
u8 ioMem[0x400];

//gba arm core variables
int SWITicks = 0;
int IRQTicks = 0;

int layerEnableDelay = 0;
bool busPrefetch = false;
bool busPrefetchEnable = false;
u32 busPrefetchCount = 0;
int cpuDmaTicksToUpdate = 0;
int cpuDmaCount = 0;
bool cpuDmaHack = false;
u32 cpuDmaLast = 0;
int dummyAddress = 0;
bool cpuBreakLoop = false;
int cpuNextEvent = 0;

int gbaSaveType = 0; // used to remember the save type on reset
bool intState = false;
bool stopState = false;
bool holdState = false;
int holdType = 0;
bool cpuSramEnabled = true;
bool cpuFlashEnabled = true;
bool cpuEEPROMEnabled = true;
bool cpuEEPROMSensorEnabled = false;

u32 cpuPrefetch[2];

int cpuTotalTicks = 0;                  //cycle count for each opcode processed
#ifdef PROFILING
int profilingTicks = 0;
int profilingTicksReload = 0;
static profile_segment *profilSegment = NULL;
#endif

int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
u8 timerOnOffDelay = 0;
u16 timer0Value = 0;
bool timer0On = false;
int timer0Ticks = 0;
int timer0Reload = 0;
int timer0ClockReload  = 0;
u16 timer1Value = 0;
bool timer1On = false;
int timer1Ticks = 0;
int timer1Reload = 0;
int timer1ClockReload  = 0;
u16 timer2Value = 0;
bool timer2On = false;
int timer2Ticks = 0;
int timer2Reload = 0;
int timer2ClockReload  = 0;
u16 timer3Value = 0;
bool timer3On = false;
int timer3Ticks = 0;
int timer3Reload = 0;
int timer3ClockReload  = 0;
u32 dma0Source = 0;
u32 dma0Dest = 0;
u32 dma1Source = 0;
u32 dma1Dest = 0;
u32 dma2Source = 0;
u32 dma2Dest = 0;
u32 dma3Source = 0;
u32 dma3Dest = 0;

void (*cpuSaveGameFunc)(u32,u8) = flashSaveDecide;
//void (*renderLine)() = mode0RenderLine;
bool windowOn = false;
int frameCount = 0;
char buffer[1024];

FILE *out = NULL;

//u32 lastTime = 0;
int count = 0;

int capture = 0;
int capturePrevious = 0;
int captureNumber = 0;

int TIMER_TICKS[4] = {
  0,
  6,
  8,
  10
};

u32  objTilesAddress [3] = {0x010000, 0x014000, 0x014000};
u8 gamepakRamWaitState[4] = { 4, 3, 2, 8 };
u8 gamepakWaitState[4] =  { 4, 3, 2, 8 };
u8 gamepakWaitState0[2] = { 2, 1 };
u8 gamepakWaitState1[2] = { 4, 1 };
u8 gamepakWaitState2[2] = { 8, 1 };

u8 memoryWait[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 };

u8 memoryWait32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };

u8 memoryWaitSeq[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 };

u8 memoryWaitSeq32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const u8 videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

u8 biosProtected[4];

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

u8 cpuBitsSet[256];

u8 cpuLowestBitSet[256];

int romSize = 0x200000; //test normal 0x2000000 current 1/10 oh no only 2.4 MB

int hblank_ticks = 0;

//gba vars end

//GBA CPU opcodes
u32 __attribute__ ((hot)) ndsvcounter(){
	return (u32read(0x04000006)&0x1ff);
}


//CPULoop-> processes registers from various mem and generate threads for each one
__attribute__ ((hot)) u32 cpu_calculate(){	//todo
	int clockticks=0; //actual CPU ticking (heartbeat)
	
	//***********************(CPU prefetch cycle lapsus)********************************/
	// variable used by the CPU core
	cpuBreakLoop = false;
	
	//cause spammed cpu_xxxreads: any broken PC address, will abuse cpureads.
	#ifdef PREFETCH_ENABLED
		//prefetch part
		//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
		u32 fetchnextpc=armnextpc(exRegs[0xf]&0xfffffffe);
	
		//idles in this state because swi idle and CPU enabled
		if(!holdState && !SWITicks) {
			if ((fetchnextpc & 0x0803FFFF) == 0x08020000) //armNextPC
				busPrefetchCount=0x100;
		}
	#endif
	
	/////////////////////////(CPU prefetch cycle lapsus end)///////////////////////////
	
	/* nds has that on lcd hardware
	if(soundtTicks < lcdTicks){
		soundtTicks+=lcdTicks; //element thread is set to zero when thread is executed.
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
	
	if(GBADISPSTAT & 2) { //HBLANK period
		//draw happens directly through CPU decoding into target VRAM memory
	
		// if in H-Blank, leave it and move to drawing mode (deadzone ticks even while hblank is 0)
		lcdTicks += 1008;
		
		//last (visible) vertical line
		if(GBAVCOUNT == 160) {
			frameCount++;	//framecount
			u32 joy = 0;	// update joystick information
		
			joy = systemreadjoypad(-1);
			GBAP1 = 0x03FF ^ (joy & 0x3FF);
			if(cpuEEPROMSensorEnabled == true){
				//systemUpdateMotionSensor(); //
			}
			UPDATE_REG(0x130 , GBAP1);	//UPDATE_REG(0x130, GBAP1);
			u16 P1CNT =	CPUReadHalfWord(((u32)ioMem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
			// this seems wrong, but there are cases where the game
			// can enter the stop state without requesting an IRQ from
			// the joypad.
			if((P1CNT & 0x4000) || stopState) {
				u16 p1 = (0x3FF ^ GBAP1) & 0x3FF;
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
			
			GBADISPSTAT |= 1; 			//V-BLANK flag set (period 160..226)
			GBADISPSTAT &= 0xFFFD;		//UNSET V-COUNTER FLAG
			//UPDATE_REG(0x04 , GBADISPSTAT);
			
			if(GBADISPSTAT & 0x0008) {
				GBAIF |= 1;
				UPDATE_REG(0x202 , GBAIF);
			}
			
			//CPUCheckDMA(1, 0x0f); 	//DMA is HBLANK period now
		}
		
		//Reaching last (non-visible) line
		else if(GBAVCOUNT >= 228){ 	
			GBADISPSTAT &= 0xFFFC;
			GBAVCOUNT = 0;
		}
		
		
		if(hblank_ticks>=240){
			GBADISPSTAT &= 0xFFFD; //(unset) toggle HBLANK flag only when entering/exiting VCOUNTer
			hblank_ticks=0;
		}
	}
	
	//Entering H-Blank (GBAVCOUNT time) 
	//note: (can't set GBAVCOUNT ticking on HBLANK because this is always HBLANK time on NDS interrupts (stuck GBAVCOUNT from 0x04000006)
	else{
		
		GBADISPSTAT |= 2;					//(set) toggle HBLANK flag only when entering/exiting VCOUNTer
		lcdTicks += 224;
		GBAVCOUNT=ndsvcounter();
		//GBAVCOUNT++; 					//vcount method read from hardware from now on
		
		//CPUCheckDMA(2, 0x0f); 	//DMA is HBLANK period now
		
		if(GBADISPSTAT & 16) {
			GBAIF |= 2;
			UPDATE_REG(0x202 , GBAIF);
		}
		
		//////////////////////////////////////VCOUNTER Thread ////////////////////////////////////////////////////////////
		if( (GBAVCOUNT) == (GBADISPSTAT >> 8) ) { //V-Count Setting (LYC) == GBAVCOUNT = IRQ VBLANK
			GBADISPSTAT |= 4;
			
			if(GBADISPSTAT & 0x20) {
				GBAIF |= 4;
				UPDATE_REG(0x202, GBAIF);
			}
		}
		else {
			GBADISPSTAT &= 0xFFFB;
		}		
	}
	
	/*
	if((layerEnableDelay) >0){
		layerEnableDelay--;
		if (layerEnableDelay==1)
			layerEnableDelay = layerEnableDelay & GBADISPCNT;
	}
	*/
	
	UPDATE_REG(0x04, GBADISPSTAT);	
	UPDATE_REG(0x06, GBAVCOUNT);
	
	//**********************************DMA THREAD runs on blank irq*************************************/
	
	return 0;
}

__attribute__ ((hot)) void cpu_fdexecute(){

	//1) read GBAROM entrypoint
	//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
	//3) get pointers from all reserved memory areas (allocated)
	//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
	//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]
	
	if(armstate == CPUSTATE_ARM){
		//Fetch Decode & Execute ARM
		//Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever Word is)
		u32 new_instr=armfetchpc_arm(exRegs[0xf]&0xfffffffe); 
		#ifdef DEBUGEMU
			printf("/******ARM******/");
			printf(" rom:%x [%x]",(unsigned int)exRegs[0xf]&0xfffffffe,(unsigned int)new_instr);
		#endif
		disarmcode(new_instr);
	}
	else if(armstate == CPUSTATE_THUMB){
		//Fetch Decode & Execute THUMB
		//Half Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever HalfWord is)
		u16 new_instr=armfetchpc_thumb((exRegs[0xf])&0xfffffffe);
		#ifdef DEBUGEMU
			printf("/******THUMB******/");
			printf(" rom:%x [%x]",(unsigned int)exRegs[0xf]&0xfffffffe,(unsigned int)new_instr);
		#endif
		disthumbcode(new_instr);
		
	}
	//HBLANK Handles CPU_EMULATE(); //checks and interrupts all the emulator kind of requests (IRQ,DMA)

	//increase PC depending on CPUmode
	if (armstate == CPUSTATE_ARM){
		exRegs[0xf]+=4;
	}
	else if(armstate == CPUSTATE_THUMB){
		exRegs[0xf]+=2;
	}
}

//cpu interrupt
u32 cpuirq(u32 cpumode){
	//Enter cpu<mode>
	updatecpuflags(CPUFLAG_UPDATE_CPSR,exRegs[0x10],cpumode);
	exRegs[0xe]=exRegs[0xf];
	
	if(armstate == CPUSTATE_THUMB){	//thumb
		exRegs[0xe]+=2;
	}
	armstate = CPUSTATE_ARM;
	armIrqEnable = false;
	
	//refresh jump opcode in biosprotected vector
	biosProtected[0] = 0x02;
	biosProtected[1] = 0xc0;
	biosProtected[2] = 0x5e;
	biosProtected[3] = 0xe5;

	//set PC
	exRegs[0xf]=(u32)0x18;
	
	//Restore CPU<mode>
	updatecpuflags(CPUFLAG_UPDATE_CPSR, exRegs[0x10], getSPSRFromCPSR(exRegs[0x10])&0x1F);

	return 0;
}

//GBA CPU mode registers:
//r0 - r15 	-- 	0x0 - 0xf
//CPSR 		--	0x10
u32	exRegs[16 + 1];

//r13, r14 for each mode (sp and lr)
u32 exRegs_r13usr[0x1];
u32 exRegs_r14usr[0x1];
u32 exRegs_r13fiq[0x1];
u32 exRegs_r14fiq[0x1];
u32 exRegs_r13irq[0x1];
u32 exRegs_r14irq[0x1];
u32 exRegs_r13svc[0x1];
u32 exRegs_r14svc[0x1];
u32 exRegs_r14abt[0x1];
u32 exRegs_r13abt[0x1];
u32 exRegs_r14und[0x1];
u32 exRegs_r13und[0x1];
//u32 exRegs_r14sys[0x1]; //stack shared with usr
//u32 exRegs_r14sys[0x1];

//original registers used by any PSR_MODE that do belong to FIQ r8-r12
u32  exRegs_fiq[0x5];

//"banked" SPSR register for every PSR mode, except the USR/SYS psr, which has NOT a SPSR and read/writes to it everywhere (from/to CPSR directly or through opcodes) are ignored
u32  SPSR_fiq[0x1];
u32  SPSR_svc[0x1];
u32  SPSR_abt[0x1];
u32  SPSR_irq[0x1];
u32  SPSR_und[0x1];

void doDMA(u32 &s, u32 &d, u32 si, u32 di, u32 c, int transfer32){

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
__attribute__ ((hot))
void  __attribute__ ((hot)) CPUCheckDMA(int reason, int dmamask)
{
  // DMA 0
  if((GBADM0CNT_H & 0x8000) && (dmamask & 1)) {
    if(((GBADM0CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((GBADM0CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((GBADM0CNT_H >> 5) & 3) {
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
        int count = (GBADM0CNT_L ? GBADM0CNT_L : 0x4000) << 1;
        if(GBADM0CNT_H & 0x0400)
          count <<= 1;
        printf("DMA0: s=%08x d=%08x c=%04x count=%08x", dma0Source, dma0Dest, 
            GBADM0CNT_H,
            count);
      }
#endif
      doDMA(dma0Source, dma0Dest, sourceIncrement, destIncrement,
            GBADM0CNT_L ? GBADM0CNT_L : 0x4000,
            GBADM0CNT_H & 0x0400);
      cpuDmaHack = true;

      /*if(GBADM0CNT_H & 0x4000) {
        GBAIF |= 0x0100;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo
      
      if(((GBADM0CNT_H >> 5) & 3) == 3) {
        dma0Dest = GBADM0DAD_L | (GBADM0DAD_H << 16);
      }
      
      if(!(GBADM0CNT_H & 0x0200) || (reason == 0)) {
        GBADM0CNT_H &= 0x7FFF;
        UPDATE_REG(0xBA, GBADM0CNT_H);
      }
    }
  }
  
  // DMA 1
  if((GBADM1CNT_H & 0x8000) && (dmamask & 2)) {
    if(((GBADM1CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((GBADM1CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((GBADM1CNT_H >> 5) & 3) {
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
          printf("DMA1: s=%08x d=%08x c=%04x count=%08x", dma1Source, dma1Dest,
              GBADM1CNT_H,
              16);
        }
#endif  
        doDMA(dma1Source, dma1Dest, sourceIncrement, 0, 4,
              0x0400);
      } else {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA1) {
          int count = (GBADM1CNT_L ? GBADM1CNT_L : 0x4000) << 1;
          if(GBADM1CNT_H & 0x0400)
            count <<= 1;
          printf("DMA1: s=%08x d=%08x c=%04x count=%08x", dma1Source, dma1Dest,
              GBADM1CNT_H,
              count);
        }
#endif          
        doDMA(dma1Source, dma1Dest, sourceIncrement, destIncrement,
              GBADM1CNT_L ? GBADM1CNT_L : 0x4000,
              GBADM1CNT_H & 0x0400);
      }
      cpuDmaHack = true;

      /*if(GBADM1CNT_H & 0x4000) {
        GBAIF |= 0x0200;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo
      
      if(((GBADM1CNT_H >> 5) & 3) == 3) {
        dma1Dest = GBADM1DAD_L | (GBADM1DAD_H << 16);
      }
      
      if(!(GBADM1CNT_H & 0x0200) || (reason == 0)) {
        GBADM1CNT_H &= 0x7FFF;
        UPDATE_REG(0xC6, GBADM1CNT_H);
      }
    }
  }
  
  // DMA 2
  if((GBADM2CNT_H & 0x8000) && (dmamask & 4)) {
    if(((GBADM2CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((GBADM2CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((GBADM2CNT_H >> 5) & 3) {
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
          printf("DMA2: s=%08x d=%08x c=%04x count=%08x", dma2Source, dma2Dest,
              GBADM2CNT_H,
              count);
        }
#endif                  
        doDMA(dma2Source, dma2Dest, sourceIncrement, 0, 4,
              0x0400);
      } else {
#ifdef DEV_VERSION
        if(systemVerbose & VERBOSE_DMA2) {
          int count = (GBADM2CNT_L ? GBADM2CNT_L : 0x4000) << 1;
          if(GBADM2CNT_H & 0x0400)
            count <<= 1;
          printf("DMA2: s=%08x d=%08x c=%04x count=%08x", dma2Source, dma2Dest,
              GBADM2CNT_H,
              count);
        }
#endif                  
        doDMA(dma2Source, dma2Dest, sourceIncrement, destIncrement,
              GBADM2CNT_L ? GBADM2CNT_L : 0x4000,
              GBADM2CNT_H & 0x0400);
      }
      cpuDmaHack = true;

      /*if(GBADM2CNT_H & 0x4000) {
        GBAIF |= 0x0400;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo

      if(((GBADM2CNT_H >> 5) & 3) == 3) {
        dma2Dest = GBADM2DAD_L | (GBADM2DAD_H << 16);
      }
      
      if(!(GBADM2CNT_H & 0x0200) || (reason == 0)) {
        GBADM2CNT_H &= 0x7FFF;
        UPDATE_REG(0xD2, GBADM2CNT_H);
      }
    }
  }

  // DMA 3
  if((GBADM3CNT_H & 0x8000) && (dmamask & 8)) {
    if(((GBADM3CNT_H >> 12) & 3) == reason) {
      u32 sourceIncrement = 4;
      u32 destIncrement = 4;
      switch((GBADM3CNT_H >> 7) & 3) {
      case 0:
        break;
      case 1:
        sourceIncrement = (u32)-4;
        break;
      case 2:
        sourceIncrement = 0;
        break;
      }
      switch((GBADM3CNT_H >> 5) & 3) {
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
        int count = (GBADM3CNT_L ? GBADM3CNT_L : 0x10000) << 1;
        if(GBADM3CNT_H & 0x0400)
          count <<= 1;
        printf("DMA3: s=%08x d=%08x c=%04x count=%08x", dma3Source, dma3Dest,
            GBADM3CNT_H,
            count);
      }
#endif                
      doDMA(dma3Source, dma3Dest, sourceIncrement, destIncrement,
            GBADM3CNT_L ? GBADM3CNT_L : 0x10000,
            GBADM3CNT_H & 0x0400);
      /*if(GBADM3CNT_H & 0x4000) {
        GBAIF |= 0x0800;
        UPDATE_REG(0x202, GBAIF);
        cpuNextEvent = cpuTotalTicks;
      }*/ //ichfly todo

      if(((GBADM3CNT_H >> 5) & 3) == 3) {
        dma3Dest = GBADM3DAD_L | (GBADM3DAD_H << 16);
      }
      
      if(!(GBADM3CNT_H & 0x0200) || (reason == 0)) {
        GBADM3CNT_H &= 0x7FFF;
        UPDATE_REG(0xDE, GBADM3CNT_H);
      }
    }
  }
}

//IO opcodes
__attribute__ ((hot))
void  CPUUpdateRegister(u32 address, u16 value){
	switch(address){
		case 0x00:{ 
			// we need to place the following code in { } because we declare & initialize variables in a case statement
			if((value & 7) > 5) {
				// display modes above 0-5 are prohibited
				GBADISPCNT = (value & 7);
			}
			bool change = (0 != ((GBADISPCNT ^ value) & 0x80)); //forced vblank? (1 means delta time vram access)
			bool changeBG = (0 != ((GBADISPCNT ^ value) & 0x0F00));
			u16 changeBGon = ((~GBADISPCNT) & value) & 0x0F00; // these layers are being activated
			
			GBADISPCNT = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
			UPDATE_REG(0x00 , GBADISPCNT);

			if(changeBGon) {
				layerEnableDelay = 4;
				layerEnable = layerSettings & value & (~changeBGon);
			}
			else {
				layerEnable = layerSettings & value;
				CPUUpdateTicks();
			}

			windowOn = (layerEnable & 0x6000) ? true : false;
			if(change && !((value & 0x80))) {
				if(!(GBADISPSTAT & 1)) {
					lcdTicks = 1008;
					//      GBAVCOUNT = 0;
					//      UPDATE_REG(0x06, GBAVCOUNT);
					GBADISPSTAT &= 0xFFFC;
					UPDATE_REG(0x04 , GBADISPSTAT);
					//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT(gba);
				}
			//        (*renderLine)();
			}
			// we only care about changes in BG0-BG3
			if(changeBG) {
			}
		break;
		}
		case 0x04:
			GBADISPSTAT = (value & 0xFF38) | (GBADISPSTAT & 7);
			UPDATE_REG(0x04 , GBADISPSTAT);
		break;
		case 0x06:
			// not writable
		break;
		case 0x08:
			GBABG0CNT = (value & 0xDFCF);
			UPDATE_REG(0x08 , GBABG0CNT);
		break;
		case 0x0A:
			GBABG1CNT = (value & 0xDFCF);
			UPDATE_REG(0x0A , GBABG1CNT);
		break;
		case 0x0C:
			GBABG2CNT = (value & 0xFFCF);
			UPDATE_REG(0x0C , GBABG2CNT);
		break;
		case 0x0E:
			GBABG3CNT = (value & 0xFFCF);
			UPDATE_REG(0x0E , GBABG3CNT);
		break;
		case 0x10:
			GBABG0HOFS = value & 511;
			UPDATE_REG(0x10 , GBABG0HOFS);
		break;
		case 0x12:
			GBABG0VOFS = value & 511;	
			UPDATE_REG(0x12 , GBABG0VOFS);
		break;
		case 0x14:
			GBABG1HOFS = value & 511;
			UPDATE_REG(0x14 , GBABG1HOFS);
		break;
		case 0x16:
			GBABG1VOFS = value & 511;
			UPDATE_REG(0x16 , GBABG1VOFS);
		break;
		case 0x18:
			GBABG2HOFS = value & 511;
			UPDATE_REG(0x18 , GBABG2HOFS);
		break;
		case 0x1A:
			GBABG2VOFS = value & 511;
			UPDATE_REG(0x1A , GBABG2VOFS);
		break;
		case 0x1C:
			GBABG3HOFS = value & 511;
			UPDATE_REG(0x1C , GBABG3HOFS);
		break;
		case 0x1E:
			GBABG3VOFS = value & 511;
			UPDATE_REG(0x1E , GBABG3VOFS);
		break;
		case 0x20:
			GBABG2PA = value;
			UPDATE_REG(0x20 , GBABG2PA);
		break;
		case 0x22:
			GBABG2PB = value;
			UPDATE_REG(0x22 , GBABG2PB);
		break;
		case 0x24:
			GBABG2PC = value;
			UPDATE_REG(0x24 , GBABG2PC);
		break;
		case 0x26:
			GBABG2PD = value;
			UPDATE_REG(0x26 , GBABG2PD);
		break;
		case 0x28:
			GBABG2X_L = value;
			UPDATE_REG(0x28 , GBABG2X_L);
			//gfxbg2changed |= 1;
		break;
		case 0x2A:
			GBABG2X_H = (value & 0xFFF);
			UPDATE_REG(0x2A , GBABG2X_H);	
			//gfxbg2changed |= 1;
		break;
		case 0x2C:
			GBABG2Y_L = value;
			UPDATE_REG(0x2C , GBABG2Y_L);
			//gfxbg2changed |= 2;
		break;
		case 0x2E:
			GBABG2Y_H = value & 0xFFF;
			UPDATE_REG(0x2E , GBABG2Y_H);
			//gfxbg2changed |= 2;
		break;
		case 0x30:
			GBABG3PA = value;
			UPDATE_REG(0x30 , GBABG3PA);	
		break;
		case 0x32:
			GBABG3PB = value;
			UPDATE_REG(0x32 , GBABG3PB);
		break;
		case 0x34:
			GBABG3PC = value;
			UPDATE_REG(0x34 , GBABG3PC);	
		break;
		case 0x36:
			GBABG3PD = value;
			UPDATE_REG(0x36 , GBABG3PD);
		break;
		case 0x38:
			GBABG3X_L = value;
			UPDATE_REG(0x38 , GBABG3X_L);
			//gfxbg3changed |= 1;
		break;
		case 0x3A:
			GBABG3X_H = value & 0xFFF;
			UPDATE_REG(0x3A , GBABG3X_H);
			//gfxbg3changed |= 1;
		break;
		case 0x3C:
			GBABG3Y_L = value;
			UPDATE_REG(0x3C , GBABG3Y_L);	
			//gfxbg3changed |= 2;
		break;
		case 0x3E:
			GBABG3Y_H = value & 0xFFF;
			UPDATE_REG(0x3E , GBABG3Y_H);	
			//gfxbg3changed |= 2;
		break;
		case 0x40:
			GBAWIN0H = value;
			UPDATE_REG(0x40 , GBAWIN0H);	
		break;
		case 0x42:
			GBAWIN1H = value;
			UPDATE_REG(0x42 , GBAWIN1H);	
		break;
		case 0x44:
			GBAWIN0V = value;
			UPDATE_REG(0x44 , GBAWIN0V);
		break;
		case 0x46:
			GBAWIN1V = value;
			UPDATE_REG(0x46 , GBAWIN1V);
		break;
		case 0x48:
			GBAWININ = value & 0x3F3F;
			UPDATE_REG(0x48 , GBAWININ);
		break;
		case 0x4A:
			GBAWINOUT = value & 0x3F3F;
			UPDATE_REG(0x4A , GBAWINOUT);
		break;
		case 0x4C:
			GBAMOSAIC = value;
			UPDATE_REG(0x4C , GBAMOSAIC);
		break;
		case 0x50:
			GBABLDMOD = value & 0x3FFF;
			UPDATE_REG(0x50 , GBABLDMOD);
		break;
		case 0x52:
			GBACOLEV = value & 0x1F1F;
			UPDATE_REG(0x52 , GBACOLEV);
		break;
		case 0x54:
			GBACOLY = value & 0x1F;
			UPDATE_REG(0x54 , GBACOLY);	
		break;
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
			//soundEvent(gba, address&0xFF, (u8)(value & 0xFF));	//sound not yet!
			//soundEvent(gba, (address&0xFF)+1, (u8)(value>>8));
		break;
		case 0x82:
		case 0x88:
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
		case 0x9e:
			//soundEvent(gba, address&0xFF, value);	//sound not yet!
		break;
		case 0xB0:
			GBADM0SAD_L = value;
			UPDATE_REG(0xB0 , GBADM0SAD_L);
		break;
		case 0xB2:
			GBADM0SAD_H = value & 0x07FF;
			UPDATE_REG(0xB2, GBADM0SAD_H);
		break;
		case 0xB4:
			GBADM0DAD_L = value;
			UPDATE_REG(0xB4, GBADM0DAD_L);
		break;
		case 0xB6:
			GBADM0DAD_H = value & 0x07FF;
			UPDATE_REG(0xB6, GBADM0DAD_H);
		break;
		case 0xB8:
			GBADM0CNT_L = value & 0x3FFF;
			UPDATE_REG(0xB8 , 0);
		break;
		case 0xBA:{
			bool start = ((GBADM0CNT_H ^ value) & 0x8000) ? true : false;
			value &= 0xF7E0;

			GBADM0CNT_H = value;
			UPDATE_REG(0xBA, GBADM0CNT_H);

			if(start && (value & 0x8000)) {
				dma0Source = GBADM0SAD_L | (GBADM0SAD_H << 16);
				dma0Dest = GBADM0DAD_L | (GBADM0DAD_H << 16);
				//CPUCheckDMA(gba, 0, 1); //launch DMA hardware , user dma args , serve them and unset dma used bits
			}
		}
		break;
		case 0xBC:
			GBADM1SAD_L = value;
			UPDATE_REG(0xBC, GBADM1SAD_L);
		break;
		case 0xBE:
			GBADM1SAD_H = value & 0x0FFF;
			UPDATE_REG(0xBE, GBADM1SAD_H);
		break;
		case 0xC0:
			GBADM1DAD_L = value;
			UPDATE_REG(0xC0, GBADM1DAD_L);
		break;
		case 0xC2:
			GBADM1DAD_H = value & 0x07FF;
			UPDATE_REG(0xC2, GBADM1DAD_H);
		break;
		case 0xC4:
			GBADM1CNT_L = value & 0x3FFF;
			UPDATE_REG(0xC4, GBADM1CNT_L);
		break;
		case 0xC6:{
			bool start = ((GBADM1CNT_H ^ value) & 0x8000) ? true : false;
			value &= 0xF7E0;

			GBADM1CNT_H = value;
			UPDATE_REG(0xC6, GBADM1CNT_H);

			if(start && (value & 0x8000)) {
				dma1Source = GBADM1SAD_L | (GBADM1SAD_H << 16);
				dma1Dest = GBADM1DAD_L | (GBADM1DAD_H << 16);
				//CPUCheckDMA(gba, 0, 2); //launch DMA hardware , user dma args , serve them and unset dma used bits
			}
		}
		break;
		case 0xC8:
			GBADM2SAD_L = value;
			UPDATE_REG(0xC8, GBADM2SAD_L);
		break;
		case 0xCA:
			GBADM2SAD_H = value & 0x0FFF;
			UPDATE_REG(0xCA, GBADM2SAD_H);
		break;
		case 0xCC:
			GBADM2DAD_L = value;
			UPDATE_REG(0xCC, GBADM2DAD_L);
		break;
		case 0xCE:
			GBADM2DAD_H = value & 0x07FF;
			UPDATE_REG(0xCE, GBADM2DAD_H);
		break;
		case 0xD0:
			GBADM2CNT_L = value & 0x3FFF;
			UPDATE_REG(0xD0 , 0);
		break;
		case 0xD2:{
			bool start = ((GBADM2CNT_H ^ value) & 0x8000) ? true : false;
			
			value &= 0xF7E0;
			
			GBADM2CNT_H = value;
			UPDATE_REG(0xD2, GBADM2CNT_H);

			if(start && (value & 0x8000)) {
				dma2Source = GBADM2SAD_L | (GBADM2SAD_H << 16);
				dma2Dest = GBADM2DAD_L | (GBADM2DAD_H << 16);

				//CPUCheckDMA(gba, 0, 4); //launch DMA hardware , user dma args , serve them and unset dma used bits
			}
		}
		break;
		case 0xD4:
			GBADM3SAD_L = value;
			UPDATE_REG(0xD4, GBADM3SAD_L);
		break;
		case 0xD6:
			GBADM3SAD_H = value & 0x0FFF;
			UPDATE_REG(0xD6, GBADM3SAD_H);
		break;
		case 0xD8:
			GBADM3DAD_L = value;
			UPDATE_REG(0xD8, GBADM3DAD_L);
		break;
		case 0xDA:
			GBADM3DAD_H = value & 0x0FFF;
			UPDATE_REG(0xDA, GBADM3DAD_H);
		break;
		case 0xDC:
			GBADM3CNT_L = value;
			UPDATE_REG(0xDC , 0);
		break;
		case 0xDE:{
			bool start = ((GBADM3CNT_H ^ value) & 0x8000) ? true : false;
			
			value &= 0xFFE0;
			
			GBADM3CNT_H = value;
			UPDATE_REG(0xDE, GBADM3CNT_H);

			if(start && (value & 0x8000)) {
				dma3Source = GBADM3SAD_L | (GBADM3SAD_H << 16);
				dma3Dest = GBADM3DAD_L | (GBADM3DAD_H << 16);
				//CPUCheckDMA(gba, 0, 8); //launch DMA hardware , user dma args , serve them and unset dma used bits
			}
		}
		break;
		case 0x100:
			timer0Reload = value;
		break;
		case 0x102:
			timer0Value = value;
			//timerOnOffDelay|=1; //added delta before activating timer?
			cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks;
		break;
		case 0x104:
			timer1Reload = value;
		break;
		case 0x106:
			timer1Value = value;
			//timerOnOffDelay|=2;
			cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks;
		break;
		case 0x108:
			timer2Reload = value;
		break;
		case 0x10A:
			timer2Value = value;
			//timerOnOffDelay|=4;
			cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks;
		break;
		case 0x10C:
			timer3Reload = value;
		break;
		case 0x10E:
			timer3Value = value;
			//timerOnOffDelay|=8;
			cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks;
		break;

		case 0x130:
			GBAP1 |= (value & 0x3FF);
			UPDATE_REG(0x130, GBAP1);
		break;

		case 0x132:
			UPDATE_REG(0x132 , value & 0xC3FF);	//UPDATE_REG(0x132, value & 0xC3FF);
		break;

		case 0x200:
			GBAIE = (value & 0x3FFF);
			UPDATE_REG(0x200 , GBAIE);
			if ((GBAIME & 1) && (GBAIF & GBAIE) && (i_flag==true))
				cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks; //acknowledge cycle & program flow
		break;
		case 0x202:
			GBAIF ^= (value & GBAIF);
			UPDATE_REG(0x202 , GBAIF);
		break;
		case 0x204:{
			memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakramwaitstate[value & 3];

			memoryWait[0x08] = memoryWait[0x09] = gamepakwaitstate[(value >> 2) & 3];
			memoryWaitSeq[0x08] = memoryWaitSeq[0x09] =
			gamepakwaitstate0[(value >> 4) & 1];

			memoryWait[0x0a] = memoryWait[0x0b] = gamepakwaitstate[(value >> 5) & 3];
			memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] =
			gamepakwaitstate1[(value >> 7) & 1];

			memoryWait[0x0c] = memoryWait[0x0d] = gamepakwaitstate[(value >> 8) & 3];
			memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] =
			gamepakwaitstate2[(value >> 10) & 1];
			
			/* //not now
			for(i = 8; i < 15; i++) {
				memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
				memoryWaitSeq32[i] = memoryWaitSeq[i]*2 + 1;
			}*/

			if((value & 0x4000) == 0x4000) {
				busPrefetchEnable= true;
				busPrefetch = false;
				busPrefetchCount = 0;
			} 
			else {
				busPrefetchEnable= false;
				busPrefetch = false;
				busPrefetchCount = 0;
			}
			UPDATE_REG(0x204 , value & 0x7FFF); //UPDATE_REG(0x204, value & 0x7FFF);

		}
		break;
		case 0x208:
			GBAIME = value & 1;
			UPDATE_REG(0x208 , GBAIME);
			if ((GBAIME & 1) && (GBAIF & GBAIE) && (i_flag==true))
				cpuNextEvent = cpuTotalTicks;	//ori: cpuNextEvent = cpuTotalTicks;
		break;
		case 0x300:
		if(value != 0)
			value &= 0xFFFE;
			UPDATE_REG(0x300 , value); 	//UPDATE_REG(0x300, value);
		break;
		default:
			UPDATE_REG((address&0x3FE) , value);		//UPDATE_REG(address&0x3FE, value);
		break;
	  }

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
	if(armstate == CPUSTATE_ARM){
		return CPUReadMemory(address+0x4);
	}
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
		//printf("Vcountreal: %08x",temp);
		//u16 help3;
		GBAVCOUNT = temp;
		GBADISPSTAT &= 0xFFF8; //reset h-blanc and V-Blanc and V-Count Setting
		//if(help3 == GBAVCOUNT) //else it is a extra long V-Line // ichfly todo it is to slow
		//{
			GBADISPSTAT |= (temp2 & 0x3); //temporary patch get original settings
		//}
		//if(GBAVCOUNT > 160 && GBAVCOUNT != 227)GBADISPSTAT |= 1;//V-Blanc
		UPDATE_REG(0x06, GBAVCOUNT);
		if(GBAVCOUNT == (GBADISPSTAT >> 8)) //update by V-Count Setting
		{
			GBADISPSTAT |= 0x4;
			//if(GBADISPSTAT & 0x20) {
			//  GBAIF |= 4;
			//  UPDATE_REG(0x202, GBAIF);
			//}
		}
		UPDATE_REG(0x04, GBADISPSTAT);
		//printf("Vcountreal: %08x",temp);
		//printf("GBADISPSTAT: %08x",temp2);
}
*/


//CPU Read/Writes

__attribute__ ((aligned (4)))
__attribute__((section(".itcm")))
u32 CPUReadMemory(u32 address)
{
  
#ifdef printreads
  printf("r32 %08x",address);
#endif
  u32 value=0;
  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
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
    value = *(u32*)((u8*)workRAM+(address&0x3FFFC));
    break;
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    value = *(u32*)((u8*)internalRAM+(address & 0x7ffC));
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
		value = GBAVCOUNT;
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
			value = GBAVCOUNT;
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
    value = *(u32*)((u8*)paletteRAM+(address & 0x3fC));
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffc);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    
    //ori: 
    value = *(u32*)((u8*)vram+address);
    
    /*
    //new: detect source gba addresses here and redirect to nds ppu engine
    if(gba_bg_mode == 0){
        //u8 bg0mapblock = ((GBABG0CNT >> 8) & 0x1f); // 2 KBytes each unit
        //u8 bg0charblock = ((GBABG0CNT >> 2) & 0x3); // 16 KBytes each unit
        
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
    value = *(u32*)((u8*)oam+(address & 0x3FC));
    break;
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
    value = (u32)(stream_readu32(address & 0x1FFFFFC));
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
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal word read: %08x", address);
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
	printf("r16 %08x",address);
#endif
#ifdef DEV_VERSION      
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned halfword read: %08x at %08x", address, armMode ?
          armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  u32 value=0;
  
  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
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
    value = *(u16*)((u8*)workRAM+(address & 0x3FFFE));
    break;
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    value = *(u16*)((u8*)internalRAM+(address & 0x7ffe));
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
		value = GBAVCOUNT;
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
			value = GBAVCOUNT;
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
    value = *(u16*)((u8*)paletteRAM+(address & 0x3fe));
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffe);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
    {
        value = 0;
        break;
    }
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    value = *(u16*)((u8*)vram+address);
    break;
  case 7:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
    value = *(u16*)((u8*)oam+(address & 0x3fe));
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
            value = stream_readu16((int)address & 0x1FFFFFE);
		break;
	}
	break;
  case 13:
#ifdef printsaveread
	  printf("%X\r",address);
#endif
    if(cpuEEPROMEnabled)
      // no need to swap this
      return  eepromRead(address);
    goto unreadable;
  case 14:
#ifdef printsaveread
	  printf("%X\r",address);
#endif
    if(cpuFlashEnabled | cpuSramEnabled)
      // no need to swap this
      return flashRead(address);
    // default
  default:
  unreadable:

#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal hword read: %08x", address);
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
printf("r8 %02x",address);
#endif

  switch(address >> 24) {
  case 0:
			if(address < 0x4000) {
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
    return (u8)*workRAM+(address & 0x3FFFF);
  case 3:
#ifdef checkclearaddrrw
	if(address > 0x03008000 && !(address > 0x03FF8000)/*upern mirrow*/)goto unreadable;
#endif
    return (u8)*internalRAM+(address & 0x7fff);
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
		return (u8)GBAVCOUNT;
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
			return (u8)GBAVCOUNT;
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
    return (u8)*paletteRAM+(address & 0x3ff);
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1ffff);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return 0;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
    return (u8)*(vram+address);
  case 7:
#ifdef checkclearaddrrw
	if(address > 0x07000400)goto unreadable;
#endif
    return (u8)*oam+(address & 0x3ff);
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
    return stream_readu8((int)address & 0x1FFFFFF);
	break;

  case 13:
#ifdef printsaveread
	  printf("%X\r",address);
#endif
    if(cpuEEPROMEnabled)
      return eepromRead(address);
    goto unreadable;
  case 14:
#ifdef printsaveread
	  printf("%X\r",address);
#endif
    if(cpuSramEnabled | cpuFlashEnabled)
      return flashRead(address);
    if(cpuEEPROMSensorEnabled == true) {
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
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal byte read: %08x", address);
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
    printf("w32 %08x to %08x",value,address);
#endif		  
#ifdef DEV_VERSION
  if(address & 3) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned word write: %08x to %08x from %08x",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  switch(address >> 24) {
  case 0x02:
#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeworkRAM[address & 0x3FFFC]))
      cheatsWriteMemory(address & 0x203FFFC,
                        value);
    else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unreadable;
#endif
      *(u32*)((u8*)workRAM+(address & 0x3FFFC)) = value;
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
      *(u32*)((u8*)internalRAM+(address & 0x7ffC)) = value;	
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
    *(u32*)((u8*)paletteRAM+(address & 0x3FC)) = value;	
    break;
  case 0x06:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unreadable;
#endif
    address = (address & 0x1fffc);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

#ifdef BKPT_SUPPORT
    if(*((u32 *)&freezeVRAM[address]))
      cheatsWriteMemory(address + 0x06000000, value);
    else
#endif
    
    *(u32*)((u8*)vram+address) = value;	
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
    *(u32*)((u8*)oam+(address & 0x3fc)) = value;
    break;
  case 0x0D:
#ifdef printsavewrite
	  	  printf("%X %X\r",address,value);
#endif
    if(cpuEEPROMEnabled) {
      eepromWrite(address, value);
      break;
    }
    goto unwritable;
  case 0x0E:
#ifdef printsavewrite
	  	  printf("%X %X\r",address,value);
#endif
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    // default
  default:
  unwritable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal word write: %08x to %08x",value, address);
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
printf("w16 %04x to %08x\r",value,address);
#endif

#ifdef DEV_VERSION
  if(address & 1) {
    if(systemVerbose & VERBOSE_UNALIGNED_MEMORY) {
      printf("Unaligned halfword write: %04x to %08x from %08x",
          value,
          address,
          armMode ? armNextPC - 4 : armNextPC - 2);
    }
  }
#endif
  
  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeworkRAM[address & 0x3FFFE]))
      cheatsWriteHalfWord(address & 0x203FFFE,
                          value);
    else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unwritable;
#endif
	*(u16*)((u8*)workRAM+(address & 0x3FFFE)) = value;
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
      *(u16*)((u8*)internalRAM+(address & 0x7ffe)) = value;
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
    *(u16*)((u8*)paletteRAM+(address & 0x3fe)) = value;
    break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unwritable;
#endif
    address = (address & 0x1fffe);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;
#ifdef BKPT_SUPPORT
    if(*((u16 *)&freezeVRAM[address]))
      cheatsWriteHalfWord(address + 0x06000000,
                          value);
    else
#endif
    *(u16*)((u8*)vram+address) = value; 
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
    *(u16*)((u8*)oam+(address & 0x3fe)) = value;
    break;
  case 8:
  case 9:
    switch(address){
		case 0x080000c4: case 0x080000c6: case 0x080000c8:
			if(!rtcWrite(address, value)){
				//printf("RTC write failed ");
				goto unwritable;
			}
		break;
	}
	break;
  case 13:
#ifdef printsavewrite
	  	  printf("%X %X\r",address,value);
#endif
    if(cpuEEPROMEnabled) {
      eepromWrite(address, (u8)value);
      break;
    }
    goto unwritable;
  case 14:
#ifdef printsavewrite
	  	  printf("%X %X\r",address,value);
#endif
    if(!eepromInUse | cpuSramEnabled | cpuFlashEnabled) {
      (*cpuSaveGameFunc)(address, (u8)value);
      break;
    }
    goto unwritable;
  default:
  unwritable:
#ifdef checkclearaddrrw
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal hword write: %04x to %08x",value, address);
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
	printf("w8 %02x to %08x\r",b,address);
#endif
  switch(address >> 24) {
  case 2:
#ifdef BKPT_SUPPORT
      if(freezeworkRAM[address & 0x3FFFF])
        cheatsWriteByte(address & 0x203FFFF, b);
      else
#endif
#ifdef checkclearaddrrw
	if(address >0x023FFFFF)goto unwritable;
#endif
        *((u8*)workRAM+(address & 0x3FFFF)) = b;
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
      *((u8*)internalRAM+(address & 0x7fff)) = b;
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
				//ori: soundEvent(gba, address&0xFF, b); //sound writes are cpu write bytes (strb)
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
    *(u16 *)((u8*)paletteRAM+(address & 0x3FE)) = (b << 8) | b;
	break;
  case 6:
#ifdef checkclearaddrrw
	if(address > 0x06020000)goto unwritable;
#endif
    address = (address & 0x1fffe);
    if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
        return;
    if ((address & 0x18000) == 0x18000)
      address &= 0x17fff;

    // no need to switch 
    // byte writes to OBJ VRAM are ignored
    if ((address) < objTilesAddress[((GBADISPCNT&7)+1)>>2])
    {
#ifdef BKPT_SUPPORT
      if(freezeVRAM[address])
        cheatsWriteByte(address + 0x06000000, b);
      else
#endif  
        *(u16*)((u8*)vram+address) = (b << 8) | b;
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
	  	  printf("%X %X\r",address,b);
#endif

    if(cpuEEPROMEnabled) {
      eepromWrite(address, b);
      break;
    }
    goto unwritable;
  case 14:
#ifdef printsavewrite
	  	  printf("%X %X\r",address,b);
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
      //printf("Illegal word read: %08x at %08x", address,reg[15].I);
	  printf("Illegal byte write: %02x to %08x",b, address);
	  REG_IME = IME_DISABLE;
	  debugDump();
	  while(1);
#endif
    break;
  }
}