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
#include "keypadTGDS.h"
#include "timerTGDS.h"
#include "dmaTGDS.h"
#include "gba.arm.core.h"

//Stack for GBA
__attribute__((section(".dtcm"))) u8 gbastck_usr[0x200];
__attribute__((section(".dtcm"))) u8 gbastck_fiq[0x200];
__attribute__((section(".dtcm"))) u8 gbastck_irq[0x200];
__attribute__((section(".dtcm"))) u8 gbastck_svc[0x200];
__attribute__((section(".dtcm"))) u8 gbastck_abt[0x200];
__attribute__((section(".dtcm"))) u8 gbastck_und[0x200];
//u8 __attribute__((section(".dtcm"))) gbastck_sys[0x200]; //stack shared with usr

//and cpu<mode> all the other backup registers when restoring from FIQ r8-r12
__attribute__((section(".dtcm"))) u32  exRegs_cpubup[0x5];

//////////////////////////////////////////////opcodes that must be virtualized, go here.

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


//fetch current address
u32 armfetchpc(u32 address){

	if(armstate==0)
		address=CPUReadMemory(address);
	else
		address=CPUReadHalfWord(address);
		
return address;
}


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
	
	//summary: cpuupdateticks() == refresh thread time 
	
	cpuBreakLoop = false;
	cpuNextEvent = cpuupdateticks(); //CPUUpdateTicks(); (CPU prefetch cycle lapsus)
	if(cpuNextEvent > ticks)
		cpuNextEvent = ticks;
		
		//prefetch part
		u32 fetchnextpc=0;
		//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
		switch(armstate){
			//ARM code
			case(0):{
				//#ifdef DEBUGEMU
				//	printf("(ARM)PREFETCH ACCESS!! ***");
				//#endif
				fetchnextpc=armnextpc(exRegs[0xf]+4); 
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
				fetchnextpc=armnextpc(exRegs[0xf]+2);
				//#ifdef DEBUGEMU
				//	printf("(THMB)PREFETCH ACCESS END !!*** ");
				//#endif	
			}
			break;
		}
		
		//Trigger a prefetch event
		if(!holdState && !SWITicks) {
			if ((fetchnextpc & 0x0803FFFF) == 0x08020000)
				busPrefetchCount=0x100;
		}
			
		clockticks = cpuupdateticks(); //CPUUpdateTicks(); (CPU cycle lapsus)
		cpuTotalTicks += clockticks;
	
		//if there is a new event: remaining ticks is the new thread time
		if(cpuTotalTicks >= cpuNextEvent){
			int remainingticks = cpuTotalTicks - cpuNextEvent;
			if (SWITicks){
				SWITicks-=clockticks;
				if (SWITicks<0)
					SWITicks = 0;
			}
			
			clockticks = cpuNextEvent;
			
			cpuTotalTicks = 0;
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
							u16 P1CNT =	CPUReadHalfWord(((u32)iomem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
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
							
							//frameCount++; //add later
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
	
	//}//endmain looper

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

//#define CPUReadByteQuick(gba, addr)
//(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]

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
	
	u8 *gbaScreen = (u8 *)(0x6820000);	////NDS BMP rgb15 mode + keyboard
	int iy=0;
	for(iy = 0; iy <160; iy++){
		dmaTransferWord(3, (uint32)gbaScreen, (uint32)(0x6200000/*bgGetGfxPtr(bgrouid)*/+512*(iy)), 480);
		gbaScreen+=512;
	}
	return 0;
}
