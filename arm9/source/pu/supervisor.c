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
