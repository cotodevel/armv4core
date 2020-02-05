//CPU opcodes (that do not rely on translator part, but GBA opcodes) here!

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
#include <dirent.h>
#include "supervisor.h"

#include "pu.h"
#include "opcode.h"
#include "Util.h"
#include "buffer.h"
#include "translator.h"
#include "bios.h"

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

#include "settings.h"
#include "keypadTGDS.h"
#include "timerTGDS.h"
#include "dmaTGDS.h"
#include "gba.arm.core.h"


//fifo
extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

//Stack for GBA
u8 __attribute__((section(".dtcm"))) gbastck_usr[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_fiq[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_irq[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_svc[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_abt[0x200];
u8 __attribute__((section(".dtcm"))) gbastck_und[0x200];
//u8 __attribute__((section(".dtcm"))) gbastck_sys[0x200]; //stack shared with usr

//Stack Pointer for GBA
u32  __attribute__((section(".dtcm"))) * gbastackptr;

//and cpu<mode> all the other backup registers when restoring from FIQ r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_cpubup[0x5];

//////////////////////////////////////////////opcodes that must be virtualized, go here.

//bx and normal branches use a branching stack
//40 u32 slots for branchcalls
u32  __attribute__((section(".dtcm"))) branch_stack[17*32]; //32 slots for branch calls

//NDS Interrupts
//irq to be set
u32 setirq(u32 irqtoset){
	//enable VBLANK IRQ
	if(irqtoset == (1<<0)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<3));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<3);
		//printf("set vb irq");
	}
	//enable HBLANK IRQ
	else if (irqtoset == (1<<1)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<4));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<4);
		//printf("set hb irq");
	}
	//enable VCOUNTER IRQ
	else if (irqtoset == (1<<2)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<5));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<5);
		//printf("set vcnt irq");
	}
	
return 0;
}


//IO opcodes
u32 __attribute__ ((hot)) ndscpu_refreshvcount(){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif


return 0;
}


u32 __attribute__ ((hot)) gbacpu_refreshvcount(){
	//#ifdef DEBUGEMU
	//debuggeroutput();
	//#endif
	
	if( (GBAVCOUNT) == (GBADISPSTAT >> 8) ) { //V-Count Setting (LYC) == GBAVCOUNT = IRQ VBLANK
		GBADISPSTAT |= 4;
		UPDATE_REG(0x04, GBADISPSTAT);

		if(GBADISPSTAT & 0x20) {
			GBAIF |= 4;
			UPDATE_REG(0x202, GBAIF);
		}
	} 
	else {
		GBADISPSTAT &= 0xFFFB;
		UPDATE_REG(0x4, GBADISPSTAT); 
	}
	if((layerEnableDelay) >0){
		layerEnableDelay--;
			if (layerEnableDelay==1)
				layerEnableDelay = layerSettings & GBADISPCNT;
	}
	return 0;
}

//src_address / addrpatches[] (blacklisted) / addrfixes[] (patched ret)
const u32 __attribute__ ((hot)) addresslookup(u32 srcaddr, u32 * blacklist, u32 * whitelist){

//this must go in pairs otherwise nds ewram ranges might wrongly match gba addresses
int i=0;
for (i=0;i<16;i+=2){

	if( (srcaddr>=(u32)(*(blacklist+((u32)i)))) &&  (srcaddr<=(u32)(*(blacklist+((u32)i+1)))) )
	{
		srcaddr=(u32)( (*(whitelist+(i))));
		break;
		//srcaddr=i; //debug
		//return i; //correct offset
	}
}
return srcaddr;
}

//fetch current address
u32 armfetchpc(u32 address){

	if(armstate==0)
		address=cpuread_word(address);
	else
		address=cpuread_hword(address);
		
return address;
}


//coto
//cpuupdateticks()
int __attribute((hot)) cpuupdateticks(){

	int cpuloopticks = lcdTicks;
	if(soundtTicks < cpuloopticks)
		cpuloopticks = soundtTicks;

	else if(timer0On && (timer0Ticks < cpuloopticks)) {
		cpuloopticks = timer0Ticks;
	}
	else if(timer1On && !(GBATM1CNT & 4) && (timer1Ticks < cpuloopticks)) {
		cpuloopticks = timer1Ticks;
	}
	else if(timer2On && !(GBATM2CNT & 4) && (timer2Ticks < cpuloopticks)) {
		cpuloopticks = timer2Ticks;
	}
	else if(timer3On && !(GBATM3CNT & 4) && (timer3Ticks < cpuloopticks)) {
		cpuloopticks = timer3Ticks;
	}

	else if (SWITicks) {
		if (SWITicks < cpuloopticks)
			cpuloopticks = SWITicks;
	}

	else if (IRQTicks) {
		if (IRQTicks < cpuloopticks)
			cpuloopticks = IRQTicks;
	}

	return cpuloopticks;
}

//CPULoop
u32 cpuloop(int ticks){
	int clockticks=0;
	int timeroverflow=0;
	
	//summary: cpuupdateticks() == refresh thread time per hw slot
	
	// variable used by the CPU core
	cpuBreakLoop = false;
	cpuNextEvent = cpuupdateticks(); //CPUUpdateTicks(); (CPU prefetch cycle lapsus)
	if(cpuNextEvent > ticks)
		cpuNextEvent = ticks;
	
	//coto
	//for(;;) { //our loop is already HBLANK -> hw controlled loop is nice
		
		//prefetch part
		u32 fetchnextpc=0;
		//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
		switch(armstate){
			//ARM code
			case(0):{
				//#ifdef DEBUGEMU
				//	printf("(ARM)PREFETCH ACCESS!! ***");
				//#endif
				fetchnextpc=armnextpc((exRegs[0xf]&0xfffffffe)+4); 
				//#ifdef DEBUGEMU
				//	printf("(ARM)PREFETCH ACCESS END !!*** ");
				//#endif
				
			}
			break;	
			//thumb code
			case(1):{
				//#ifdef DEBUGEMU
				//	printf("(THMB)PREFETCH ACCESS!! ***");
				//#endif
				fetchnextpc=armnextpc((exRegs[0xf]&0xfffffffe)+2);
				//#ifdef DEBUGEMU
				//	printf("(THMB)PREFETCH ACCESS END !!*** ");
				//#endif	
			}
			break;
		}
		//idles in this state because swi idle and CPU enabled
		if(!holdState && !SWITicks) {
			if ((/*armNextPC*/ fetchnextpc & 0x0803FFFF) == 0x08020000)
				busPrefetchCount=0x100;
		}
		//end prefetch part
		
		
		
		clockticks = cpuupdateticks(); //CPUUpdateTicks(); (CPU cycle lapsus)
		cpuTotalTicks += clockticks; //ori: cpuTotalTicks += clockticks;
	
		//if there is a new event: remaining ticks is the new thread time
		if(cpuTotalTicks >= cpuNextEvent){	//ori: if(cpuTotalTicks >= cpuNextEvent){
		
			int remainingticks = cpuTotalTicks - cpuNextEvent; //ori: int remainingticks = cpuTotalTicks - cpuNextEvent;

			if (SWITicks){
				SWITicks-=clockticks;
				if (SWITicks<0)
					SWITicks = 0;
			}
			
			clockticks = cpuNextEvent;
			
			cpuTotalTicks = 0;	//ori: cpuTotalTicks = 0;
			cpuDmaLast = false;
			updateLoop:
			if (IRQTicks){
				IRQTicks -= clockticks;
				if (IRQTicks<0)
					IRQTicks = 0;
			}
			lcdTicks -= clockticks;
			if(lcdTicks <= 0) {
				if(GBADISPSTAT & 1) { // V-BLANK
					// if in V-Blank mode, keep computing... (v-counter flag match = 1)
					if(GBADISPSTAT & 2) {
						lcdTicks += 1008;
							//GBAVCOUNT++; //hardware vcounter
						UPDATE_REG(0x06, GBAVCOUNT);	//UPDATE_REG(0x06, GBAVCOUNT);
						GBADISPSTAT &= 0xFFFD;
						UPDATE_REG(0x04, GBADISPSTAT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					} 
					else{
						lcdTicks += 224;
						GBADISPSTAT |= 2;
						UPDATE_REG(0x04 , GBADISPSTAT);
						if(GBADISPSTAT & 16) {
							GBAIF |= 2;	//GBAIF |= 2;
							UPDATE_REG(0x202, GBAIF);	//UPDATE_REG(0x202, GBAIF);
						}
					}
					if(GBAVCOUNT >= 228){ //Reaching last line
						GBADISPSTAT &= 0xFFFC;
						UPDATE_REG(0x04, GBADISPSTAT);
						GBAVCOUNT = 0;
						UPDATE_REG(0x06, GBAVCOUNT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					}
				}
				else{
					if(GBADISPSTAT & 2) {
						// if in H-Blank, leave it and move to drawing mode
							//GBAVCOUNT++; //hardware vcounter
						UPDATE_REG(0x06 , GBAVCOUNT);
						lcdTicks += 1008;
						GBADISPSTAT &= 0xFFFD;
						if(GBAVCOUNT == 160) { //last hblank line
							frameCount++;
							u32 joy = 0;	// update joystick information
							
							joy = systemreadjoypad(-1);
							GBAP1 = 0x03FF ^ (joy & 0x3FF);
							if(cpuEEPROMSensorEnabled == true){
								//systemUpdateMotionSensor(); 
							}
							UPDATE_REG(0x130 , GBAP1);
							u16 P1CNT =	cpuread_hwordfast(((u32)iomem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
							// this seems wrong, but there are cases where the game
							// can enter the stop state without requesting an IRQ from
							// the joypad.
							if((P1CNT & 0x4000) || stopState) {
								u16 p1 = (0x3FF ^ GBAP1) & 0x3FF;
								if(P1CNT & 0x8000) {
									if(p1 == (P1CNT & 0x3FF)) {
										GBAIF |= 0x1000;
										UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
									}
								} 
								else {
									if(p1 & P1CNT) {
										GBAIF |= 0x1000;
										UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
									}
								}
							}
							
							GBADISPSTAT |= 1; 		//V-BLANK flag set (period 160..226)
							GBADISPSTAT &= 0xFFFD;
							UPDATE_REG(0x04 , GBADISPSTAT);	
							if(GBADISPSTAT & 0x0008) {
								GBAIF |= 1;
								UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
							}
							
							//CPUCheckDMA(1, 0x0f); //add later
							
							//framenummer++; //add later
						}
						UPDATE_REG(0x04, GBADISPSTAT);
						//gbacpu_refreshvcount(); moved to vcount thread	//CPUCompareVCOUNT();
					} 
					else{
						// entering H-Blank
						GBADISPSTAT |= 2;
						UPDATE_REG(0x04, GBADISPSTAT);
						lcdTicks += 224;
							//CPUCheckDMA(2, 0x0f); //add later
						if(GBADISPSTAT & 16) {
							GBAIF |= 2;
							UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
						}
					}
				}
			}//end if there's new delta lcd ticks to process..!

			if(stopState == false) {
				if(timer0On) {
					timer0Ticks -= clockticks;
					if(timer0Ticks <= 0) {
						timer0Ticks += (0x10000 - timer0Reload) << timer0ClockReload;
						timeroverflow |= 1;
						//soundTimerOverflow(0);
						if(GBATM0CNT & 0x40) {
							GBAIF |= 0x08;
							UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
						}
					}
					GBATM0D = 0xFFFF - (timer0Ticks >> timer0ClockReload);
					UPDATE_REG(0x100 , GBATM0D);	//UPDATE_REG(0x100, GBATM0D); 
				}
				if(timer1On) {
					if(GBATM1CNT & 4) {
						if(timeroverflow & 1) {
							GBATM1D++;
							if(GBATM1D == 0) {
								GBATM1D += timer1Reload;
								timeroverflow |= 2;
								//soundTimerOverflow(1);
								if(GBATM1CNT & 0x40) {
									GBAIF |= 0x10;
									UPDATE_REG(0x202 , GBAIF);
								}
							}
							UPDATE_REG(0x104 , GBATM1D);
						}
					} 
					else{
						timer1Ticks -= clockticks;
						if(timer1Ticks <= 0) {
							timer1Ticks += (0x10000 - timer1Reload) << timer1ClockReload;
							timeroverflow |= 2; 
							//soundTimerOverflow(1);
							if(GBATM1CNT & 0x40) {
								GBAIF |= 0x10;
								UPDATE_REG(0x202 , GBAIF);
							}
						}
						GBATM1D = 0xFFFF - (timer1Ticks >> timer1ClockReload);
						UPDATE_REG(0x104 , GBATM1D);
					}
				}
				if(timer2On) {
					if(GBATM2CNT & 4) {
						if(timeroverflow & 2){
							GBATM2D++;
							if(GBATM2D == 0) {
								GBATM2D += timer2Reload;
								timeroverflow |= 4;
								if(GBATM2CNT & 0x40) {
									GBAIF |= 0x20;
									UPDATE_REG(0x202 , GBAIF);
								}
							}
							UPDATE_REG(0x108 , GBATM2D);
						}
					}
					else{
						timer2Ticks -= clockticks;
						if(timer2Ticks <= 0) {
							timer2Ticks += (0x10000 - timer2Reload) << timer2ClockReload;
							timeroverflow |= 4; 
							if(GBATM2CNT & 0x40) {
								GBAIF |= 0x20;
								UPDATE_REG(0x202 , GBAIF);
							}
						}
						GBATM2D = 0xFFFF - (timer2Ticks >> timer2ClockReload);
						UPDATE_REG(0x108 , GBATM2D);
					}
				}
				if(timer3On) {
					if(GBATM3CNT & 4) {
						if(timeroverflow & 4) {
							GBATM3D++;
							if(GBATM3D == 0) {
								GBATM3D += timer3Reload;
								if(GBATM3CNT & 0x40) {
									GBAIF |= 0x40;
									UPDATE_REG(0x202 , GBAIF);
								}
							}
							UPDATE_REG(0x10c , GBATM3D);
						}
					}
					else{
						timer3Ticks -= clockticks;
						if(timer3Ticks <= 0) {
							timer3Ticks += (0x10000 - timer3Reload) << timer3ClockReload; 
							if(GBATM3CNT & 0x40) {
								GBAIF |= 0x40;
								UPDATE_REG(0x202 , GBAIF);	//UPDATE_REG(0x202, GBAIF);
							}
						}
						GBATM3D = 0xFFFF - (timer3Ticks >> timer3ClockReload);
						UPDATE_REG(0x10c , GBATM3D);
					}
				}
			}//end if not idle (CPU toggle StopState)

			timeroverflow = 0;
			
			ticks -= clockticks;
			cpuNextEvent = cpuupdateticks();
			if(cpuDmaTicksToUpdate > 0) {
				if(cpuDmaTicksToUpdate > cpuNextEvent)
					clockticks = cpuNextEvent;
				else
					clockticks = cpuDmaTicksToUpdate;
				cpuDmaTicksToUpdate -= clockticks;
				if(cpuDmaTicksToUpdate < 0)
					cpuDmaTicksToUpdate = 0;
				cpuDmaLast = true;
				goto updateLoop;
			}
			
			if(GBAIF && (GBAIME & 1) && armIrqEnable){
				int res = GBAIF & GBAIE;
				if(stopState == true)
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
			
			if(remainingticks > 0){
				if(remainingticks > cpuNextEvent)
					clockticks = cpuNextEvent;
				else
					clockticks = remainingticks;
				remainingticks -= clockticks;
				if(remainingticks < 0)
					remainingticks = 0;
				goto updateLoop;
			}
			if (timerOnOffDelay)
				//applyTimer(); //add this 
			if(cpuNextEvent > ticks)
				cpuNextEvent = ticks;
			if(ticks <= 0 || cpuBreakLoop)
				//break;
				return 0;
		}//end if there's a new event
	
	//}//endmain looper //coto

return 0;
}


//hardware < - > virtual environment update CPU stats

//IO address, u32 value
u32 __attribute__ ((hot)) cpu_updateregisters(u32 address, u16 value){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

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
			cpuupdateticks();	// CPUUpdateTicks();
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

return 0;
}

//direct GBA CPU reads
u32 virtread_word(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u32)*((u32*)address);
}

u16 virtread_hword(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u16)*((u16*)address);
}

u8 virtread_byte(u32 address){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
return (u8)*((u8*)address);
}

//direct GBA CPU writes
u32 virtwrite_word(u32 address,u32 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u32*)address)=data;
return 0;
}

u16 virtwrite_hword(u32 address,u16 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u16*)address)=data;
return 0;
}

u8 virtwrite_byte(u32 address,u8 data){
	address=addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	*((u8*)address)=data;
return 0;
}

//hamming weight read from memory CPU opcodes

//#define CPUReadByteQuick(gba, addr)
//(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]

u8 __attribute__ ((hot)) cpuread_bytefast(u8 address){
	#ifdef DEBUGEMU
	debuggeroutput();
	#endif
	return CPUReadByte(address);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadHalfWordQuick(gba, addr)
//READ16LE(((u16*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
  
u16 __attribute__ ((hot)) cpuread_hwordfast(u16 address){
	#ifdef DEBUGEMU
	debuggeroutput();
	#endif

	return  CPUReadHalfWord(address);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadMemoryQuick(gba, addr) 
//READ32LE(((u32*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
		
u32 __attribute__ ((hot)) cpuread_wordfast(u32 address){
	#ifdef DEBUGEMU
	debuggeroutput();
	#endif

	CPUReadMemory(address);
}

////////////////////////////////////VBA CPU read mechanism/////////////////////////////////////////////// //coto
//u8 CPUGBA reads
//old: CPUReadByte(GBASystem *gba, u32 address);
u8 __attribute__ ((hot)) cpuread_byte(u32 address){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

switch(address >> 24) {
	case 0:
		fastldr((u8*)&dummyreg, exRegs, (0xf), 32,0);	//PC fetch
		if ((dummyreg) >> 24) {
			if(address < 0x4000) {
				#ifdef DEBUGEMU
					printf("byte: bios protected read! (%x) ",(unsigned int)address);
				#endif
				return u8read((u32)biosProtected+(address & 3));
			} 
			else 
				goto unreadable;
		}
	#ifdef DEBUGEMU
		printf("byte: bios read! (%x) ",(unsigned int)address);
	#endif
	
		//return bios[address & 0x3FFF];
		return u8read((u32)bios+(address & 0x3FFF));
	break;
	case 2:
		#ifdef DEBUGEMU
			printf("byte: workRAM read! (%x) ",(unsigned int)address);
		#endif
		
		//return workRAM[address & 0x3FFFF];
		return u8read((u32)workRAM+(address & 0x3FFFF));
	break;
	case 3:
		#ifdef DEBUGEMU
			printf("byte: internalRAM read! (%x) ",(unsigned int)address);
		#endif
		//return internalRAM[address & 0x7fff];
		return u8read((u32)internalRAM+(address & 0x7fff));
	break;
	case 4:
		if((address < 0x4000400) && ioReadable[address & 0x3ff]){
			#ifdef DEBUGEMU
				printf("byte: IO read! (%x) ",(unsigned int)address);
			#endif
			//return ioMem[address & 0x3ff];
			return u8read((u32)iomem+(address & 0x3ff));
		}
		else 
			goto unreadable;
	break;
	case 5:
		#ifdef DEBUGEMU
			printf("byte: palram read! (%x) ",(unsigned int)address);
		#endif
		
		//return paletteRAM[address & 0x3ff];
		return u8read((u32)palram+(address & 0x3ff));
	break;
	case 6:
		address = (address & 0x1ffff);
		if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		#ifdef DEBUGEMU
			printf("byte: vram read! (%x) ",(unsigned int)address);
		#endif
		//return vram[address];
		return u8read((u32)vram+address);
	break;
	case 7:
		#ifdef DEBUGEMU
			printf("byte: oam read! (%x) ",(unsigned int)address);
		#endif
		//return oam[address & 0x3ff];
		return u8read((u32)oam+(address & 0x3ff));
	break;
	case 8: case 9: case 10: case 11: case 12:
		//coto
		#ifndef ROMTEST
			return (u8)(stream_readu8(address % 0x08000000)); //gbareads SHOULD NOT be aligned
		#endif
			
		#ifdef ROMTEST
			return (u8)*(u8*)(&rom_pl_bin+((address % 0x08000000)/4 ));
		#endif
		//return 0;
	break;
	case 13:
		if(cpuEEPROMEnabled)
			//return eepromRead(address); //saves not yet
			return 0;
		else
			goto unreadable;
	break;
	case 14:
		if(cpuSramEnabled | cpuFlashEnabled)
			//return flashRead(address);
			return 0;
		else
			goto unreadable;
			
		if(cpuEEPROMSensorEnabled == true) {
			switch(address & 0x00008f00) {
				case 0x8200:
					//return systemGetSensorX() & 255; //sensor not yet
				case 0x8300:
					//return (systemGetSensorX() >> 8)|0x80; //sensor not yet
				case 0x8400:
					//return systemGetSensorY() & 255; //sensor not yet
				case 0x8500:
					//return systemGetSensorY() >> 8; //sensor not yet
				break;
			}
			return 0; //for now until eeprom is implemented
		}
	break;
	//default
	default:
	unreadable:
		
		//arm read
		if(armstate==0) {
			//value = CPUReadMemoryQuick(gba, reg[15].I);
			#ifdef DEBUGEMU
				printf("byte:[ARM] default read! (%x) ",(unsigned int)address);
			#endif
			
			return virtread_word(exRegs[0xf]&0xfffffffe);
		} 
		
		//thumb read
		else{
			//value = CPUReadHalfWordQuick(gba, reg[15].I) |
			//CPUReadHalfWordQuick(gba, reg[15].I) << 16;
			#ifdef DEBUGEMU
			printf("byte:[THMB] default read! (%x) ",(unsigned int)address);
			#endif
			return ( (virtread_hword((exRegs[0xf]&0xfffffffe))) | ((virtread_hword((exRegs[0xf]&0xfffffffe)))<< 16) ); //half word duplicate
		}
		
		
		if(cpuDmaLast) {
			return cpuDmaLast & 0xFF;
		} 
		else{
			if((armstate)==0){
				#ifdef DEBUGEMU
				printf("byte:[ARM] default GBAmem read! (%x) ",(unsigned int)address);
				#endif
				//return CPUReadByteQuick(gba, reg[15].I+(address & 3));
				return cpuread_bytefast((exRegs[0xf]&0xfffffffe)+((address & 3)));
			} 
			else{
				#ifdef DEBUGEMU
				printf("byte:[THMB] default GBAmem read! (%x) ",(unsigned int)address);
				#endif
				
				//return CPUReadByteQuick(gba, reg[15].I+(address & 1));
				return cpuread_bytefast((exRegs[0xf]&0xfffffffe)+((address & 1)));
			}
		}
		
	break;
}

}

//u16 CPUGBA reads
//old: CPUReadHalfWord(GBASystem *gba, u32 address)
u16 __attribute__ ((hot)) cpuread_hword(u32 address){

//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

u32 value=0;
switch(address >> 24) {
	case 0:
	fastldr((u8*)&dummyreg, exRegs, (0xf), 32,0); 
		if ((dummyreg) >> 24) {
			if(address < 0x4000) {
				//value = ldru16extasm((u32)(u8*)biosProtected,(address&2)); 	//value = READ16LE(((u16 *)&biosProtected[address&2]));
				//#ifdef DEBUGEMU
				//printf("hword biosProtected read! ");
				//#endif
				
				#ifdef DEBUGEMU
				printf("hword bios read! ");
				#endif
				//value = ldru16extasm((u32)(u8*)bios,address);
				value = u16read((u32)bios+address);
			} 
			else 
				goto unreadable;
		} 
		else {
			#ifdef DEBUGEMU
			printf("hword bios read! ");
			#endif
			//value = READ16LE(((u16 *)&bios[address & 0x3FFE]));
			value = u16read((u32)bios+(address & 0x3FFE));
		}
	break;
	case 2:
			#ifdef DEBUGEMU
			printf("hword workRAM read! ");
			#endif
			//value = READ16LE(((u16 *)&workRAM[address & 0x3FFFE]));
			value = u16read((u32)workRAM+(address & 0x3FFFE));
	break;
	case 3:
			#ifdef DEBUGEMU
			printf("hword internalRAM read! ");
			#endif
			//value = READ16LE(((u16 *)&internalRAM[address & 0x7ffe]));
			value = u16read((u32)internalRAM+(address & 0x7ffe));
	break;
	case 4:
		if((address < 0x4000400) && ioReadable[address & 0x3fe]){
			#ifdef DEBUGEMU
			printf("hword IO read! ");
			#endif
			//value =  READ16LE(((u16 *)&ioMem[address & 0x3fe]));
			value = u16read((u32)iomem+(address & 0x7ffe));				
			if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E)){
				if (((address & 0x3fe) == 0x100) && timer0On)
					//ori: value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload); //timer top - timer period
					value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
					
				else {
					if (((address & 0x3fe) == 0x104) && timer1On && !(GBATM1CNT & 4))
						//ori: value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
						value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
					else
						if (((address & 0x3fe) == 0x108) && timer2On && !(GBATM2CNT & 4))
							//ori: value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
							value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
						else
							if (((address & 0x3fe) == 0x10C) && timer3On && !(GBATM3CNT & 4))
								//ori: value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer2ClockReload);
								value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer2ClockReload);
				}
			}
		}
		else 
			goto unreadable;
    break;
	case 5:
		#ifdef DEBUGEMU
			printf("hword palram read! ");
		#endif
		//value = READ16LE(((u16 *)&paletteRAM[address & 0x3fe]));
		value = u16read((u32)palram+(address & 0x3fe));
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			#ifdef DEBUGEMU
			printf("hword vram read! ");
			#endif
		//value = READ16LE(((u16 *)&vram[address]));
		value = u16read((u32)vram+address);
	break;
	case 7:
			#ifdef DEBUGEMU
			printf("hword oam read! ");
			#endif
		//value = READ16LE(((u16 *)&oam[address & 0x3fe]));
		value = u16read((u32)oam+(address & 0x3fe));
	break;
	case 8: case 9: case 10: case 11: case 12:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			//value = rtcRead(address);	//RTC not yet
		}
		else
			//value = READ16LE(((u16 *)&rom[address & 0x1FFFFFE]));
			
		#ifndef ROMTEST
			return stream_readu16(address % 0x08000000); //gbareads are never word aligned. (why would you want that?!)
		#endif
		
		#ifdef ROMTEST
			return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
		#endif
		
	break;
	case 13:
		if(cpuEEPROMEnabled)
		// no need to swap this
		//return  eepromRead(address);	//saving not yet
    goto unreadable;
  
	case 14:
		if(cpuFlashEnabled | cpuSramEnabled){
		//no need to swap this
		//return flashRead(address);	//saving not yet
		}
	//default
	default:
		unreadable:
			
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				printf("hword default read! ");
				#endif
				//value = CPUReadMemoryQuick(gba, reg[15].I);
				value=cpuread_wordfast((exRegs[0xf]&0xfffffffe));
			
			//thumb read
			} 
			else{
				#ifdef DEBUGEMU
				printf("hword default read! (x2) ");
				#endif
				
				//value = CPUReadHalfWordQuick(gba, reg[15].I) |
				//CPUReadHalfWordQuick(gba, reg[15].I) << 16;
				value = cpuread_hwordfast((exRegs[0xf]&0xfffffffe)) | (cpuread_hwordfast((exRegs[0xf]&0xfffffffe)) << 16);
			}
			
			
			if(cpuDmaLast) {
				value = cpuDmaLast & 0xFFFF;
			} 
			else{
				//thumb?
				if((armstate)==1) {
					#ifdef DEBUGEMU
					printf("[THMB]hword default read! ");
					#endif
				
					//value = CPUReadHalfWordQuick(gba, reg[15].I + (address & 2));
					value	= cpuread_hwordfast((exRegs[0xf]&0xfffffffe)+(address & 2));
				} 
				//arm?
				else{
					#ifdef DEBUGEMU
					printf("[ARM]hword default read! ");
					#endif
				
					//value = CPUReadHalfWordQuick(gba, reg[15].I);
					value	= cpuread_hwordfast((exRegs[0xf]&0xfffffffe));
				}
			}
			
    break;
}

if(address & 1) {
	value = (value >> 8) | (value << 24);
}

return value;
}

//u32 CPUGBA reads
//old: CPUReadMemory(GBASystem *gba, u32 address)
u32 __attribute__ ((hot)) cpuread_word(u32 address){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

	u32 value=0;
	switch(address >> 24) {
		case 0:
			fastldr((u8*)&dummyreg, exRegs, (0xf), 32,0); //pc
			if(dummyreg >> 24) {
				if(address < 0x4000) {
					//value = READ32LE(((u32 *)&biosProtected));
					//value = ldru32asm((u32)(u8*)biosProtected,0x0);
					//printf("IO access!");
					//#ifdef DEBUGEMU
					//printf("Word biosProtected read! ");
					//#endif
					#ifdef DEBUGEMU
						printf("Word bios read! (%x)[%x] ",(unsigned int)((u32)(u8*)bios+address),(unsigned int)*(u32*)((u32)(u8*)bios+address));
					#endif
				
					value = u32read((u32)bios+address);
				}
				else 
					goto unreadable;
			}
			else{
				//printf("any access!");
				//value = READ32LE(((u32 *)&bios[address & 0x3FFC]));
				//ori:
				//value = ldru32extasm((u32)(u8*)bios,(address & 0x3FFC));
				//#ifdef DEBUGEMU
				//printf("Word biosProtected read! ");
				//#endif
				
				#ifdef DEBUGEMU
					printf("Word bios read! (%x)[%x] ",(unsigned int)((u32)(u8*)bios+address),(unsigned int)*(u32*)((u32)(u8*)bios+address));
				#endif
			
				value = u32read((u32)bios+address);
			}
		break;
		case 2:
			#ifdef DEBUGEMU
			printf("Word workRAM read! ");
			#endif
			
			//value = READ32LE(((u32 *)&workRAM[address & 0x3FFFC]));
			value = u32read((u32)workRAM+(address & 0x3FFFC));
		break;
		case 3:
			//stacks should be here..
			#ifdef DEBUGEMU
			printf("Word internalRAM read! ");
			#endif
			
			//value = READ32LE(((u32 *)&internalRAM[address & 0x7ffC]));
			value = u32read((u32)internalRAM+(address & 0x7ffC));
		break;
		case 4: //ioReadable is mapped
			if((address < 0x4000400) && ioReadable[address & 0x3fc]) {
				//u16 or u32
				if(ioReadable[(address & 0x3fc) + 2]) {
					#ifdef DEBUGEMU
					printf("Word iomem read! ");
					#endif
					
					//value = READ32LE(((u32 *)&ioMem[address & 0x3fC]));
					value = u32read((u32)iomem+(address & 0x3fC));
				} 
				else{
					#ifdef DEBUGEMU
					printf("HWord iomem read! ");
					#endif
					
					//value = READ16LE(((u16 *)&ioMem[address & 0x3fc]));
					value = u16read((u32)iomem+(address & 0x3fC));
				}
			}
			else
				goto unreadable;
		break;
		case 5:
			#ifdef DEBUGEMU
			printf("Word palram read! ");
			#endif
					
			//value = READ32LE(((u32 *)&paletteRAM[address & 0x3fC]));
			value = u16read((u32)palram+(address & 0x3fC));
		break;
		case 6:
			address = (address & 0x1fffc);
			if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			
			#ifdef DEBUGEMU
			printf("Word vram read! ");
			#endif
			
			//value = READ32LE(((u32 *)&vram[address]));
			value = u16read((u32)vram+address);	
		break;
		case 7:
			#ifdef DEBUGEMU
			printf("Word oam read! ");
			#endif
			
			//value = READ32LE(((u32 *)&oam[address & 0x3FC]));
			//value = ldru32extasm((u32)(u8*)oam,(address & 0x3FC));
			value = u16read((u32)oam+(address & 0x3FC));
		break;
		case 8: case 9: case 10: case 11: case 12:{
			//#ifdef DEBUGEMU
			//printf("Word gba read! (%x) ",(unsigned int)address);
			//#endif
			
			#ifndef ROMTEST
				return (u32)(stream_readu32(address % 0x08000000)); //gbareads are OK and from HERE should NOT be aligned (that is module dependant)
			#endif
			
			#ifdef ROMTEST
				return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
			#endif
			
		break;
		}
		case 13:
			//if(cpuEEPROMEnabled)
				// no need to swap this
					//return eepromRead(address);	//save not yet
		break;
		goto unreadable;
		case 14:
			if((cpuFlashEnabled==1) || (cpuSramEnabled==1) )
				// no need to swap this
					//return flashRead(address);	//save not yet
				return 0;
		break;
		
		default:
		unreadable:
			//printf("default wordread");
			//struct: map[index].address[(u8*)address]
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				printf("WORD:default read! (%x)[%x] ",(unsigned int)address,(unsigned int)value);
				#endif
				
				//value = CPUReadMemoryQuick(gba, reg[15].I);
				value=cpuread_wordfast((exRegs[0xf]&0xfffffffe));
				
			//thumb read
			} 
			else{
				//value = CPUReadHalfWordQuick(gba, reg[15].I) |
				//CPUReadHalfWordQuick(gba, reg[15].I) << 16;
				#ifdef DEBUGEMU
				printf("HWORD:default read! (%x)[%x] ",(unsigned int)address,(unsigned int)value);
				#endif
				value=cpuread_hwordfast((u16)((exRegs[0xf]&0xfffffffe))) | (cpuread_hwordfast((u16)((exRegs[0xf]&0xfffffffe)))<<16) ;
			}
	}
	//shift by n if not aligned (from thumb for example)
	if(address & 3) {
		int shift = (address & 3) << 3;
			value = (value >> shift) | (value << (32 - shift));
	}

return value;
}

//CPU WRITES
//ori: CPUWriteByte(GBASystem *gba, u32 address, u8 b)
u32 __attribute__ ((hot)) cpuwrite_byte(u32 address,u8 b){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 2:
		#ifdef DEBUGEMU
		printf("writebyte: writing to workRAM");
		#endif
		
		//workRAM[address & 0x3FFFF] = b;
		u32store((u32)workRAM+(address & 0x3FFFF),b);
	break;
	case 3:
		#ifdef DEBUGEMU
		printf("writebyte: writing to internalRAM");
		#endif
		
		//internalRAM[address & 0x7fff] = b;
		u32store((u32)internalRAM+(address & 0x7fff),b);
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
					#ifdef DEBUGEMU
						printf("writebyte: updating AUDIO/CNT MAP");
					#endif
					//soundEvent(gba, address&0xFF, b); //sound not yet
				break;
				case 0x301: // HALTCNT, undocumented
					if(b == 0x80)
						//stop = true;
					//holdState = 1;
					//holdType = -1;
					cpuNextEvent = cpuTotalTicks; //ori: cpuNextEvent = cpuTotalTicks;
					
				break;
				default:{ // every other register
						u32 lowerBits = (address & 0x3fe);
						if(address & 1) {
							#ifdef DEBUGEMU
								printf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE(&ioMem[lowerBits]) & 0x00FF) | (b << 8));
							cpu_updateregisters(lowerBits, ( (((u16*)iomem)[lowerBits]) & 0x00FF) | (b << 8));
						} 
						else {
							#ifdef DEBUGEMU
								printf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE((u32)&iomem[lowerBits]) & 0xFF00) | b);
							cpu_updateregisters(lowerBits, ( (((u16*)iomem)[lowerBits]) & 0xFF00) | (b));
						}
				}
				break;
			}
		}
		//else 
			//goto unwritable;
    break;
	case 5:
		
		#ifdef DEBUGEMU
		printf("writebyte: writing to palram");
		#endif
		
		//no need to switch
		//*((u16 *)&paletteRAM[address & 0x3FE]) = (b << 8) | b;
		u16store((u32)palram+(address & 0x3FE),(b << 8) | b);
	break;
	case 6:
		address = (address & 0x1fffe);
		if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;

		// no need to switch
		// byte writes to OBJ VRAM are ignored
		if ((address) < objtilesaddress[((GBADISPCNT&7)+1)>>2]){
			#ifdef DEBUGEMU
			printf("writebyte: writing to vram");
			#endif
		
			//*((u16 *)&vram[address]) = (b << 8) | b;
			//stru16inlasm((u32)(u8*)vram,(address),(b << 8) | b);
			u16store((u32)vram+address,((b << 8) | b));
		}
    break;
	case 7:
		// no need to switch
		// byte writes to OAM are ignored
		// *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
    break;
	case 13:
		if(cpuEEPROMEnabled) {
			//eepromWrite(address, b);	//saves not yet
			break;
		}
	break;
	//goto unwritable;
	
	case 14: default:
		
		#ifdef DEBUGEMU
		printf("writebyte: default write!");
		#endif
		
		virtwrite_byte(address,b);
	break;
  }

return 0;
}

//ori: CPUWriteHalfWord(GBASystem *gba, u32 address, u16 value)
u32 __attribute__ ((hot)) cpuwrite_hword(u32 address, u16 value){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 2:
		#ifdef DEBUGEMU
		printf("writehword: workRAM write!");
		#endif
		
		//WRITE16LE(((u16 *)&workRAM[address & 0x3FFFE]),value);
		u16store((u32)workRAM+(address & 0x3FFFE),(value&0xffff));
	break;
	case 3:
		#ifdef DEBUGEMU
		printf("writehword: internalRAM write!");
		#endif
		
		//WRITE16LE(((u16 *)&internalRAM[address & 0x7ffe]), value);
		u16store((u32)internalRAM+(address & 0x7ffe),value);
	break;
	case 4:
		if(address < 0x4000400){
			printf("writehword: IO regs write!");
			//CPUUpdateRegister(gba, address & 0x3fe, value);
			cpu_updateregisters(address & 0x3fe, value);
		}
		//else 
		//	goto unwritable;
    break;
	case 5:
		#ifdef DEBUGEMU
		printf("writehword: palram write!");
		#endif
			
		//WRITE16LE(((u16 *)&paletteRAM[address & 0x3fe]), value);
		u16store((u32)palram+(address & 0x3fe),value);
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		#ifdef DEBUGEMU
		printf("writehword: vram write!");
		#endif
		
		//WRITE16LE(((u16 *)&vram[address]), value);
		u16store((u32)vram+address,value);
	break;
	case 7:
		#ifdef DEBUGEMU
		printf("writehword: oam write!");
		#endif
		
		//WRITE16LE(((u16 *)&oam[address & 0x3fe]), value);
		u16store((u32)oam+(address & 0x3fe),value);
		
	break;
	case 8:
	case 9:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
			/* if(!rtcWrite(address, value))
				goto unwritable; */ //rtc not yet
				return 0; //temporal, until rtc is fixed
		} 
		/* else if(!agbPrintWrite(address, value)) 
			goto unwritable; */ //agbprint not yet
			
			else return 0; //temporal, until rtc is fixed
    break;
	case 13:
		if(cpuEEPROMEnabled) {
			//eepromWrite(address, (u8)value); //save not yet
			break;
		}
    break;
	//goto unwritable;
	
	case 14: default:
		#ifdef DEBUGEMU
		printf("writehword: default write!");
		#endif
		
		virtwrite_hword(address,value);
	break;
}

return 0;
}

//ori: CPUWriteMemory(GBASystem *gba, u32 address, u32 value)
u32 __attribute__ ((hot)) cpuwrite_word(u32 address, u32 value){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(address >> 24) {
	case 0x02:
		#ifdef DEBUGEMU
		printf("writeword: writing to workRAM");
		#endif
		
		//WRITE32LE(((u32 *)&workRAM[address & 0x3FFFC]), value);
		u32store((u32)workRAM+(address & 0x3FFFC),value);
	break;
	case 0x03:
		#ifdef DEBUGEMU
		printf("writeword: writing to internalRAM: (%x)<-[%x]",(unsigned int)((u32)(u8*)internalRAM+(address & 0x7ffC)),(unsigned int)value);
		#endif
		
		//WRITE32LE(((u32 *)&internalRAM[address & 0x7ffC]), value);
		//stru32inlasm((u32)(u8*)internalRAM,(address & 0x7ffC),value);
		//*(u32*)((u32)(u8*)internalRAM+(address & 0x7ffC))=value; //alt
		
		//DOES NOT WORK BECAUSE internalRAM HANDLES (U8*)
		//internalRAM[address & 0x7ffC]=value;
		//THIS FIXES IT
		u32store((u32)internalRAM+(address & 0x7ffC),value);
	break;
	case 0x04:
		if(address < 0x4000400) {
			//CPUUpdateRegister(gba, (address & 0x3FC), value & 0xFFFF);
			cpu_updateregisters(address & 0x3FC, (value & 0xFFFF));
			
			//CPUUpdateRegister(gba, (address & 0x3FC) + 2, (value >> 16));
			cpu_updateregisters((address & 0x3FC) + 2, (value >> 16));
			#ifdef DEBUGEMU
				printf("writeword: updating IO MAP");
			#endif
		} 
		else {
			virtwrite_word(address,value); //goto unwritable;
			printf("#inminent lockup!");
		}
	break;
	case 0x05:
		#ifdef DEBUGEMU
		printf("writeword: writing to palram");
		#endif
		//WRITE32LE(((u32 *)&paletteRAM[address & 0x3FC]), value);
		u32store((u32)palram+(address & 0x3FC),value);
	break;
	case 0x06:
		address = (address & 0x1fffc);
			if (((GBADISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		
		#ifdef DEBUGEMU
		printf("writeword: writing to vram");
		#endif
		//WRITE32LE(((u32 *)&vram[address]), value);
		u32store((u32)vram+address,value);
	break;
	case 0x07:
		#ifdef DEBUGEMU
		printf("writeword: writing to oam");
		#endif
		
		//WRITE32LE(((u32 *)&oam[address & 0x3fc]), value);
		u32store((u32)oam+(address & 0x3fc),value);
	break;
	case 0x0D:
		if(cpuEEPROMEnabled) {
			//eepromWrite(address, value); //saving not yet
			#ifdef DEBUGEMU
			printf("writeword: writing to eeprom");
			#endif
			break;
		}
   break;
   //goto unwritable;
	
	// default
	case 0x0E: default:{
		#ifdef DEBUGEMU
		printf("writeword: address[%x]:(%x) fallback to default",(unsigned int) (address+0x4),(unsigned int)value);
		#endif
	
		virtwrite_word(address,value);
		break;
	}
}

return 0;	
}

////////////////////////////////VBA CPU read mechanism end////////////////////////////////////////// 

//data stack manipulation calls

//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 
// 0 = for CPU swap opcodes (cpurestore/cpubackup) modes
// 1 = for all LDMIAVIRT/STMIAVIRT OPCODES
// 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending

//1) create a call that retrieves data from thumb SP GBA
u32 __attribute__ ((hot)) ldmiavirt(u8 * output_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped,u8 order){ //access 8 16 or 32
int cnt=0;
int offset=0;

//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order

switch(byteswapped){
case(0): 	
	while(cnt<16){
		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
	
			if (access == (u8)8){
			
			}
			else if (access == (u8)16){
			
			}
			else if (access == (u8)32){
				if ((order&1) == 0){
					*((u32*)output_buf+(offset))=*((u32*)stackptr+(cnt)); // *(level_addr[0] + offset) //where offset is index depth of (u32*) / OLD
					//printf(" ldmia:%x[%x]",offset,*((u32*)stackptr+(cnt)));
				}
				else{
					*((u32*)output_buf-(offset))=*((u32*)stackptr-(cnt));
				}
			}
		offset++; //linear offset
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
			
			if ((order&1) == 0)
				*((u32*)output_buf+(offset))=(exRegs[0xf]&0xfffffffe);
			
			else
				*((u32*)output_buf-(offset))=(exRegs[0xf]&0xfffffffe);
		}
	
	cnt++; //non linear register offset
	}
break;

//if you come here again because values aren't r or w, make sure you actually copy them from the register list to be copied
//this can't be modified as breaks ldmia gbavirtreg handles (read/write). SPECIFICALLY DONE FOR gbavirtreg[0] READS. don't use it FOR ANYTHING ELSE.

case(1):
	while(cnt<16) {
		if((regs>>cnt) & 0x1){
		//printf("%x ",cnt);
		
		if (access == (u8)8)
			*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset));
		
		else if (access == (u8)16){
			//printf("%x ",cnt);
			//printf("%x ",(u32*)stackptr);
			//for(i=0;i<16;i++){
			//printf("%x ", *((u32*)input_buf+i*4));
			//}
			//printf("%x ",cnt);
			//printf("ldmia:%d[%x] ", offset,*((u32*)stackptr+(offset)));
			*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
		}	
		
		else if (access == (u8)32){
			
			if ((order&1) == 0){
				//printf("%x ",cnt);
				//printf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//printf("%x ", *((u32*)stackptr+i*4));
				//}
				//printf("%x ",cnt);
				//printf("ldmia:%d[%x] ", offset,*((u32*)stackptr+(cnt)));
				*((u32*)output_buf+(offset))= *((u32*)stackptr+(cnt));
			}
			else{
				*((u32*)output_buf-(offset))= *((u32*)stackptr-(cnt));
			}
		}
			
		offset++;
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (PUSH-POP MANAGEMENT)
case (2):

	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
			//printf("ldmia virt: %x ",cnt);
		
			if (access == (u8)8)
				*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset)) ;
		
			else if (access == (u8)16){
				//printf("%x ",cnt);
				//printf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//printf("%x ", *((u32*)input_buf+i*4));
				//}
				//printf("%x ",cnt);
					*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
			}	
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//printf("%x ",cnt);
					//printf("%x ",(u32*)stackptr);
					//for(i=0;i<16;i++){
					//printf("%x ", *((u32*)stackptr+i*4));
					//}
					//printf("%x ",cnt);
		
					//printf("(2)ldmia:%x[%x]",offset,*((u32*)stackptr+(offset)));
					*((u32*)output_buf+(cnt))= *((u32*)stackptr+(offset));
				}
				else{
					*((u32*)output_buf-(cnt))= *((u32*)stackptr-(offset));
				}
			}
			offset++;
		}
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			
			if ((order&1) == 0){
				//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				exRegs[0xf]=*((u32*)stackptr+(offset));
			}
			else{
				exRegs[0xf]=*((u32*)stackptr-(offset));
			}
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (LDMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
case(3): 	
	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
		//printf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//Ok but direct reads
					//*(u32*) ( (*((u32*)output_buf+ 0)) + (offset*4) )=*((u32*)stackptr+(cnt));
					//printf(" ldmia%x:srcvalue[%x]",cnt, *((u32*)stackptr+(cnt)) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
			
					//handled reads is GBA CPU layer specific:
					*((u32*)(output_buf+(cnt*4)))=cpuread_word((u32)(stackptr+(offset*4)));
					//printf(" (3)ldmia%x:srcvalue[%x]",cnt, cpuread_word(stackptr+(offset*4)));
				}
				else{
					*((u32*)(output_buf-(cnt*4)))=cpuread_word((u32)(stackptr-(offset*4)));
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				//Ok but direct reads
				//printf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				//exRegs[0xf]=*((u32*)stackptr+(offset));
			
				//handled reads is GBA CPU layer specific:
				exRegs[0xf]=cpuread_word((u32)(stackptr+(cnt*4)));
				break;
			}
			else{
				exRegs[0xf]=cpuread_word((u32)(stackptr-(cnt*4)));
				break;
			}
		}
	cnt++;
	}
break;

} //switch

return 0;
}

//2)create a call that stores desired r0-r7 reg (value held) into SP
//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 1 = not swapped (direct rw)
//	/ 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending
u32 __attribute__ ((hot)) stmiavirt(u8 * input_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped, u8 order){ 
	//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order
	int cnt=0;
	int offset=0;
	switch(byteswapped){
		//SPECIFICALLY DONE FOR CPUBACKUP/RESTORE; don't use it FOR ANYTHING ELSE.
		case(0):{
			while(cnt<16) {
				if( ((regs>>cnt) & 0x1) && cnt!=15 ){
					if (access == (u8)8){
					}
					else if (access == (u8)16){
					}
					else if (access == (u8)32){	
						if ((order&1) == 0){
							*((u32*)stackptr+(cnt))= (* ((u32*)input_buf + offset)); //linear reads (each register is set on their corresponding offset from the buffer)
							//printf(" stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ offset)); );
						}
						else{
							*((u32*)stackptr-(cnt))= (* ((u32*)input_buf - offset)); 
						}
					}
					offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
				}
				
				else if (( ((regs>>cnt) & 0x1) && cnt==15 )){		
					if ((order&1) == 0){
						//Ok but direct writes
						//*(((u32*)stackptr)+(offset))=(exRegs[0xf]&0xfffffffe);
						//printf("saved rom: %x -> stack",(exRegs[0xf]&0xfffffffe)); //if rom overwrites this is why
					
						exRegs[0xf]=(* ((u32*)input_buf + offset));
						break;
					}
					else{
						exRegs[0xf]=(* ((u32*)input_buf - offset));
						break;
					}
				}
				
			cnt++;
			}
		}
		break;

		case(1):{
			while(cnt<16) {
				if( ((regs>>cnt) & 0x1) && cnt!=15 ){
					//printf("%x ",cnt);
					if (access == (u8)8){
					}
				
					else if (access == (u8)16){
						if ((order&1) == 0){
							//printf("stmia:%d[%x] ", offset,*((u32*)input_buf+(offset)));
							*((u16*)stackptr+(cnt))= *((u16*)input_buf+(offset));
						}
						else{
							*((u16*)stackptr-(cnt))= *((u16*)input_buf-(offset));
						}
					}	
				
					else if (access == (u8)32){
						if ((order&1) == 0){
							//printf("stmia:%d[%x]", offset,*(((u32*)input_buf)+(cnt)));
							*((u32*)stackptr+(offset))= (* ((u32*)input_buf+ cnt));
						}
						else{
							*((u32*)stackptr-(offset))= (* ((u32*)input_buf- cnt));
						}
					}
					
					offset++;
				}
				
				else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
					if ((order&1) == 0){
						*((u32*)stackptr+offset)=(exRegs[0xf]&0xfffffffe);
						//rom=(u32)*((u32*)input_buf+(offset));
						//printf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
					}
					else{
						*((u32*)stackptr-offset)=(exRegs[0xf]&0xfffffffe);
						//rom=(u32)*((u32*)input_buf+(offset));
						//printf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
					}
				}
				cnt++;
			}
		}
		break;

		//EXCLUSIVE FOR STACK (PUSH-POP stack<mode>_curr framepointer MANAGEMENT)
		case(2):{
			while(cnt<16) {
				if((regs>>cnt) & 0x1){
					//printf("%x ",cnt);
					if (access == (u8)8){
					}
					
					else if (access == (u8)16){
					}
					
					else if (access == (u8)32){				
						if ((order&1) == 0){
							*((u32*)stackptr+(offset)) = (* ((u32*)input_buf+ cnt));	//reads from n register offset into full ascending stack
							//printf(" (2)stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ cnt)) ); // stmia:  rn value = *(level_addsrc[0] + offset)
						}
						else{
							*((u32*)stackptr-(offset)) = (* ((u32*)input_buf- cnt));
						}
					}
						
					offset++;
				}
				cnt++;
			}
		}
		break;

		//EXCLUSIVE FOR STACK (STMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
		case(3):{
			while(cnt<16) {
				if( ((regs>>cnt) & 0x1) && (cnt!=15)){
					//printf("%x ",cnt);
					if (access == (u8)8){
					}
					else if (access == (u8)16){
					}
					else if (access == (u8)32){ 
						if ((order&1) == 0){	
							//OK writes but direct access
							//*((u32*)stackptr+(offset))= *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) );	//reads from register offset[reg n] from [level1] addr into full ascending stack
							//printf(" stmia%x:srcvalue[%x]",cnt, *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) ) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
					
							//handled writes is GBA CPU layer specific:
							cpuwrite_word((u32)(stackptr+(offset*4)), *((u32*)input_buf+cnt) );
							//printf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
						}
						else{
							//handled writes is GBA CPU layer specific:
							cpuwrite_word((u32)(stackptr-(offset*4)), *((u32*)input_buf-cnt) );
							//printf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
						}
					}
					offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
				}
				else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
					if ((order&1) == 0){
						//Ok but direct writes
						//*(((u32*)stackptr)+(offset))=rom;
						//printf("saved rom: %x -> stack",rom); //if rom overwrites this is why
				
						//handled writes is GBA CPU layer specific:
						cpuwrite_word((u32)(stackptr+(offset*4)), (exRegs[0xf]&0xfffffffe));
						//printf("read value:%x",cpuread_word(stackptr+(offset*4)));
						break;
					}
					else{
						//handled writes is GBA CPU layer specific:
						cpuwrite_word((u32)(stackptr-(offset*4)), (exRegs[0xf]&0xfffffffe));
						//printf("read value:%x",cpuread_word(stackptr+(offset*4)));
						break;
					}
				}
				cnt++;	//offset for non-linear register stacking
			}
		}
		break;
	} //switch

	return 0;
}

//fast single ldr / str opcodes for virtual environment gbaregs
u32 __attribute__ ((hot)) faststr(u8 * input_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
	switch(byteswapped){
		//0: gbavirtreg handles.
		case(0):{
			if (access == (u8)8){
				switch(regs){
					case(0xf):
						exRegs[0xf]=(* ((u8*)input_buf+ 0));
					break;
					
					default:
						gbareg[regs] = (* ((u8*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
						//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
					break;
				} //don't care after this point
			}
			else if (access == (u8)16){
				switch(regs){
					case(0xf):
						exRegs[0xf]=(* ((u16*)input_buf+ 0));
					break;
					
					default:
						gbareg[regs] = (* ((u16*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
						//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
					break;
				} //don't care after this point
			}
			else if (access == (u8)32){
				
				switch(regs){
					case(0xf):
						exRegs[0xf]=(* ((u32*)input_buf+ 0));
					break;
					
					default:
						gbareg[regs] = (* ((u32*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
						//printf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
					break;
				} //don't care after this point
			}
		}
		break;
	}

	return 0;
}
//gbaregs[] arg 2
u32 __attribute__ ((hot)) fastldr(u8 * output_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
	switch(byteswapped){
		//0: gbavirtreg handles.
		case(0):{
			if (access == (u8)8){
				switch(regs){
					case(0xf):
						*((u8*)output_buf+(0))=(exRegs[0xf]&0xfffffffe);
					break;
					
					default:
						*((u8*)output_buf+(0))= gbareg[regs];
						//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
					break;
				}//don't care after this point
			}
			else if (access == (u8)16){
					switch(regs){
					case(0xf):
						*((u16*)output_buf+(0))=(exRegs[0xf]&0xfffffffe);
					break;
					
					default:
						*((u16*)output_buf+(0))= gbareg[regs];
						//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
					break;
				}//don't care after this point
			}
			else if (access == (u8)32){
			
				switch(regs){
					case(0xf):
						*((u32*)output_buf+(0))=(exRegs[0xf]&0xfffffffe);
					break;
					
					default:
						*((u32*)output_buf+(0))= gbareg[regs] ;
						//printf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
					break;
				}//don't care after this point
			}
		break;
		}
	}
	return 0;
}

//u32 stackframepointer / u32 #Imm to be added to stackframepointer
 
//3)create a call that add from SP address #Imm, so SP is updated /* detect CPSR so each stack has its stack pointer updated */
u32 addspvirt(u32 stackptr,int ammount){
	//sanitize stack top depending on gbastckmodeadr_curr (base stack ptr depending on CPSR mode)
	stackptr=(u32)updatestackfp((u32*)stackptr, (u32*)gbastckmodeadr_curr);
	
	//update new stack fp size
	stackptr=addsasm((int)stackptr,ammount);
	
	//printf("%x",stackptr);
	return (u32)stackptr;
}

//4)create a call that sub from SP address #Imm, so SP is updated
u32 subspvirt(u32 stackptr,int ammount){
	//sanitize stack top depending on gbastckmodeadr_curr (base stack ptr depending on CPSR mode)
	stackptr=(u32)updatestackfp((u32*)stackptr, (u32*)gbastckmodeadr_curr);

	//update new stack fp size
	stackptr=subsasm((int)stackptr,ammount);
	
	//printf("%x",stackptr);
	return (u32)stackptr;
}


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


void vblank_thread(){

}


void hblank_thread(){
	//emulatorgba(cpsrvirt); //not yet until it is stable (don't think so)
	cpuTotalTicks++;
}

void vcount_thread(){
	gbacpu_refreshvcount();	
	GBAVCOUNT++;
}

//process saving list
void save_thread(u32 * srambuf){
	//check for u32 * sound_block, u32 freq, and other remaining lists from the block_list emu
}

//GBA Render threads
u32 gbavideorender(){
	*(u32*)(0x04000000)= (0x8030000F) | (1 << 16); //nds GBADISPCNT
	
	u8 *pointertobild = (u8 *)(0x6820000);
	int iy=0;
	for(iy = 0; iy <160; iy++){
		dmaTransferWord(3, (uint32)pointertobild, (uint32)(0x6200000/*bgGetGfxPtr(bgrouid)*/+512*(iy)), 480);
		pointertobild+=512;
	}
	return 0;
}
