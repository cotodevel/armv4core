#include "gbacore.h"

#include <nds.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>

//NDS7 clock
#include <time.h> 
#include "clock.h"
#include "gba_ipc.h"

//emu clock
#include "rtc.h"

#include "../pu/supervisor.h"
#include "../pu/pu.h"
#include "../util/opcode.h"
#include "../util/util.h"
#include "../util/buffer.h"
#include "../util/translator.h"
#include "../util/bios.h"

//disk
#include "../disk/stream_disk.h"

//player instance
#include "../arm9.h"

//save
#include "savechip.h"

#include "../settings.h"

//GBA CPU opcodes
u32 __attribute__ ((hot)) ndsvcounter(){
	return (u32read(0x04000006)&0x1ff);
}

//DMAs must exist because GBA logic offers linear DMA reads (fast) and it is used by retail code
u32 __attribute__ ((hot)) dma_gbavirt(u32 source,u32 dest,u32 wordcnt,u16 control,u8 dma_ch){
int offset_src=0;
int offset_dest=0;
int cnter=0; //steps in 2 or 4 bytes
if((dma_ch==0) || (dma_ch==1) || (dma_ch==2)){
	if(wordcnt==0){
		//wordcnt=0x4000; //default by GBACore
		wordcnt=0x10;
	}
	wordcnt=(wordcnt&0x7fff); //14bit wordcount field
}
else if (dma_ch==3){
	if(wordcnt==0){
		//wordcnt=0x10000;
		wordcnt=0x10;
	}
	wordcnt=(wordcnt&0x1ffff); //16bit wordcount field
}
else
	return 0; //quit if DMA channel isn't 0--3

if(((control>>10)&1)==0){ //because each loop is half-word depth (16bit)

	while(0<wordcnt){
		
		DC_InvalidateAll(); //clean and invalidate caches
		
			cpuwrite_hword(dest+(offset_dest*4), cpuread_hword(source+(offset_src*4))); //hword aligned
			
			//dest addr control
			switch((control>>5)&3){
				case(0):
					offset_dest++; //incr
				break;
				case(1):
					offset_dest--; //decr
				break;
				case(2):
					offset_dest=0; //fixed
				break;
				case(3):
					//increment reload
				break;
			}
			
			//src addr control
			switch((control>>7)&3){
				case(0):
					offset_src++; //incr
				break;
				case(1):
					offset_src--; //decr
				break;
				case(2):
					offset_src=0; //fixed
				break;
				case(3):
					//prohibited
				break;
			}
			
		wordcnt-=0x2;
		cnter++;
		drainwrite();
	}
}
else{	//because each loop is word depth (32bit) transfers

	while(0<wordcnt){
		DC_InvalidateAll(); //clean and invalidate caches
			cpuwrite_word(dest+(offset_dest*4), cpuread_word(source+(offset_src*4))); //hword aligned
			
			//dest addr control
			switch((control>>5)&3){
				case(0):
					offset_dest++; //incr
				break;
				case(1):
					offset_dest--; //decr
				break;
				case(2):
					offset_dest=0; //fixed
				break;
				case(3):
					//increment reload
				break;
			}
			
			//src addr control
			switch((control>>7)&3){
				case(0):
					offset_src++; //incr
				break;
				case(1):
					offset_src--; //decr
				break;
				case(2):
					offset_src=0; //fixed
				break;
				case(3):
					//prohibited
				break;
			}
			
		wordcnt-=0x4;
		cnter++;
		drainwrite();
	}
}

#ifdef DEBUGEMU
	iprintf("\n copied (%d)bytes from DMA! | src:%x | dest:%x",(int)(cnter*4),(unsigned int)source,(unsigned int)dest);
#endif
return 0;
}


//CPULoop-> processes registers from various mem and generate threads for each one
u32 __attribute__ ((hot)) cpu_calculate(){
	int clockticks=0; //actual CPU ticking (heartbeat)
		
		//***********************(CPU prefetch cycle lapsus)********************************/
		// variable used by the CPU core
		gba.cpubreakloop = false;
		gba.cpunextevent = cpuupdateticks(cputotalticks); // returns thread time
		
		//cause spammed cpu_xxxreads: any broken PC address, will abuse cpureads.
		#ifdef PREFETCH_ENABLED
		
			//prefetch part
			//cpsr is set in updatecpuflags. //armNextPC->fetchnextpc module
			u32 fetchnextpc=armnextpc(rom);
		
			//idles in this state because swi idle and CPU enabled
			if(!gba.holdstate && !gba.switicks) {
				if ((fetchnextpc & 0x0803FFFF) == 0x08020000) //armNextPC
					gba.busprefetchcount=0x100;
			}
		#endif
		
		/////////////////////////(CPU prefetch cycle lapsus end)///////////////////////////
	
		clockticks = cpuupdateticks(cputotalticks); // returns thread time post prefetch
		//cputotalticks += clockticks; //we let HBLANK increase totalticks
		
		if (gba.switicks){
			gba.switicks-=clockticks;
			if (gba.switicks<0)
				gba.switicks = 0;
		}
			
		clockticks = (clockticks - gba.cpunextevent);	//delta thread time post prefetch
		
		////////////////////////////////////////THREADS//////////////////////////////////////////
		//irq execute
		if (gba.irqticks){
			gba.irqticks -= clockticks;
			if (gba.irqticks<0)
				gba.irqticks = 0;
		}
		
		//LCD execute
		gba.lcdticks -= clockticks;
		
		///////////////////////////////////// Interrupt thread //////////////////////////////////////
		if(gbavirt_ifmasking && (gbavirt_imemasking & 1) && gba.armirqenable){
			int res = gbavirt_ifmasking & gbavirt_iemasking;
			if(gba.stopstate)
				res &= 0x3080;
			if(res) {
				if (gba.intstate){
					if (!gba.irqticks){
						cpuirq(0x12); //CPUInterrupt();
						gba.intstate = false;
						gba.holdstate = false;
						gba.stopstate = false;
						gba.holdtype = 0;
					}
				}
				else{
					if (!gba.holdstate){
						gba.intstate = true;
						gba.irqticks=7;
					if (gba.cpunextevent> gba.irqticks)
						gba.cpunextevent = gba.irqticks;
					}
					else{
						cpuirq(0x12); //CPUInterrupt();
						gba.holdstate = false;
						gba.stopstate = false;
						gba.holdtype = 0;
					}
				}
				// Stops the SWI Ticks emulation if an IRQ is executed
				//(to avoid problems with nested IRQ/SWI)
				if (gba.switicks)
					gba.switicks = 0;
			}
		}
		
		
		/////////////////////////////////VBLANK/HBLANK THREAD/////////////////////////////////////////////////////////
		//not this one
		
		if(gba.DISPSTAT & 2) { //HBLANK period
			//draw
			//gbavideorender((u32)gba.vidram, (u32)hblankdelta);
		
			// if in H-Blank, leave it and move to drawing mode (deadzone ticks even while hblank is 0)
			gba.lcdticks += 1008;
			
			//last (visible) vertical line
			if(gba.VCOUNT == 160) {
				gba.count++;	//framecount
				u32 joy = 0;	// update joystick information
			
				joy = systemreadjoypad(-1);
				gba.P1 = 0x03FF ^ (joy & 0x3FF);
				if(gba.cpueepromsensorenabled){
					//systemUpdateMotionSensor(); //
				}
				UPDATE_REG(0x130 , gba.P1);	//UPDATE_REG(0x130, P1);
				u16 P1CNT =	cpuread_hwordfast(((u32)gba.iomem[0x132]));	//u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
				// this seems wrong, but there are cases where the game
				// can enter the stop state without requesting an IRQ from
				// the joypad.
				if((P1CNT & 0x4000) || gba.stopstate) {
					u16 p1 = (0x3FF ^ gba.P1) & 0x3FF;
					if(P1CNT & 0x8000) {
						if(p1 == (P1CNT & 0x3FF)) {
							gbavirt_ifmasking |= 0x1000;
							UPDATE_REG(0x202 , gbavirt_ifmasking);	
						}
					} 
					else {
						if(p1 & P1CNT) {
							gbavirt_ifmasking |= 0x1000;
							UPDATE_REG(0x202 , gbavirt_ifmasking);
						}
					}
				}
				
				gba.DISPSTAT |= 1; 			//V-BLANK flag set (period 160..226)
				gba.DISPSTAT &= 0xFFFD;		//UNSET V-COUNTER FLAG
				//UPDATE_REG(0x04 , gba.DISPSTAT);
				
				if(gba.DISPSTAT & 0x0008) {
					gbavirt_ifmasking |= 1;
					UPDATE_REG(0x202 , gbavirt_ifmasking);
				}
				
				//CPUCheckDMA(1, 0x0f); 	//DMA is HBLANK period now
			}
			
			//Reaching last (non-visible) line
			else if(gba.VCOUNT >= 228){ 	
				gba.DISPSTAT &= 0xFFFC;
				gba.VCOUNT = 0;
			}
			
			
			if(hblankdelta>=240){
				gba.DISPSTAT &= 0xFFFD; //(unset) toggle HBLANK flag only when entering/exiting VCOUNTer
				hblankdelta=0;
			}
		}
		
		//Entering H-Blank (VCOUNT time) 
		//note: (can't set VCOUNT ticking on HBLANK because this is always HBLANK time on NDS interrupts (stuck VCOUNT from 0x04000006)
		else{
			
			gba.DISPSTAT |= 2;					//(set) toggle HBLANK flag only when entering/exiting VCOUNTer
			gba.lcdticks += 224;
			gba.VCOUNT=ndsvcounter();
			//gba.VCOUNT++; 					//vcount method read from hardware from now on
			
			//CPUCheckDMA(2, 0x0f); 	//DMA is HBLANK period now
			
			if(gba.DISPSTAT & 16) {
				gbavirt_ifmasking |= 2;
				UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, IF);
			}
			
			//////////////////////////////////////VCOUNTER Thread ////////////////////////////////////////////////////////////
			if( (gba.VCOUNT) == (gba.DISPSTAT >> 8) ) { //V-Count Setting (LYC) == VCOUNT = IRQ VBLANK
				gba.DISPSTAT |= 4;
				
				if(gba.DISPSTAT & 0x20) {
					gbavirt_ifmasking |= 4;
					UPDATE_REG(0x202, gbavirt_ifmasking);		 //UPDATE_REG(0x202, gba->IF);
				}
			}
			else {
				gba.DISPSTAT &= 0xFFFB;
				//UPDATE_REG(0x4, gba->DISPSTAT);
			}		
		}
		
		/*
		if((gba.layerenabledelay) >0){
			gba.layerenabledelay--;
			if (gba.layerenabledelay==1)
				gba.layerenable = gba.layersettings & gba.DISPCNT;
		}
		*/
		
		UPDATE_REG(0x04, gba.DISPSTAT);	//UPDATE_REG(0x04, gba.DISPSTAT);
		UPDATE_REG(0x06, gba.VCOUNT);	//UPDATE_REG(0x06, VCOUNT);
		
		
		/*int bye=0; 
		for(bye=0;bye<15;bye++){
			iprintf("read(cpuread) :%x | read (vcountvirt) :%x \n ", (unsigned int)cpuread_word(0x04000006),(unsigned int)gba.VCOUNT);
		}
		
		while(1);
		*/
		
		//**********************************DMA THREAD*************************************/
		//not this one either
		//DMA checks are done on the HBLANK process /one DMA each HBLANK so doesn't take up that much CPU time
		//we need to update DMA's IO (gba.DM0CNT_L) before, so WHEN READING FROM WORDS we get that value from MAP
		//checks if DMAXCNT's is enabled and there is WORD transfers queued so DMAXTransfers occur
		if( ((((gba.DM0CNT_H)>>15)&1)==1) ){
		
			dma_gbavirt(gba.dma0source,gba.dma0dest,(u16)(gba.DM0CNT_L&0xffff),(u16)(gba.DM0CNT_H&0xffff),0);
		
			if((gba.DM0CNT_H>>14)&0x1){ //irq enable?
				gbavirt_ifmasking|=(1<<8);
			}
			#ifdef DEBUGEMU
				iprintf("[DMA0 served!] \n");
			#endif
			gba.DM0CNT_H&=0x7fff;
			gba.DM0CNT_L&= ~(0x7fff); //clean all 14 bits except MSB
		}
	
		else if( ((((gba.DM1CNT_H)>>15)&1)==1) ){
		
			dma_gbavirt(gba.dma1source,gba.dma1dest,(u16)(gba.DM1CNT_L&0xffff),(u16)(gba.DM1CNT_H&0xffff),1);
		
			if((gba.DM1CNT_H>>14)&0x1){ //irq enable?
				gbavirt_ifmasking|=(1<<9);
			}
			#ifdef DEBUGEMU
				iprintf("[DMA1 served!] \n");
			#endif
			gba.DM1CNT_H&=0x7fff;
			gba.DM1CNT_L&= ~(0x7fff); //clean all 14 bits except MSB
		}
	
		else if( ((((gba.DM2CNT_H)>>15)&1)==1) ){
		
			dma_gbavirt(gba.dma2source,gba.dma2dest,(u16)(gba.DM2CNT_L&0xffff),(u16)(gba.DM2CNT_H&0xffff),2);
		
			if((gba.DM2CNT_H>>14)&0x1){ //irq enable?
				gbavirt_ifmasking|=(1<<10);
			}
			#ifdef DEBUGEMU
				iprintf("[DMA2 served!] \n");
			#endif
			gba.DM2CNT_H&=0x7fff;
			gba.DM2CNT_L&= ~(0x7fff); //clean all 14 bits except MSB
		}
	
		else if( ((((gba.DM3CNT_H)>>15)&1)==1) ){
		
			dma_gbavirt(gba.dma3source,gba.dma3dest,(u16)(gba.DM3CNT_L&0xffff),(u16)(gba.DM3CNT_H&0xffff),3);
		
			if((gba.DM3CNT_H>>14)&0x1){ //irq enable?
				gbavirt_ifmasking|=(1<<11);
			}
			#ifdef DEBUGEMU
				iprintf("[DMA3 served!] \n");
			#endif
			gba.DM3CNT_H&=0x7fff;
			gba.DM3CNT_L&= ~(0x1ffff); //clean all 16 bits except MSB
		}

return 0;
}

u32 __attribute__ ((hot)) cpu_fdexecute(){

//1) read GBAROM entrypoint
//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
//3) get pointers from all reserved memory areas (allocated)
//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]

//btw entrypoint is always ARM code 

//u32 debugopcode=0;

if(gba.cpustate==true){
	
	if(armstate==0){
		//Fetch Decode & Execute ARM
		//Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever Word is)
		u32 new_instr=armfetchpc_arm((rom&0xfffffffe)); 
		#ifdef DEBUGEMU
			iprintf("/******ARM******/");
			iprintf("\n rom:%x [%x]\n",(unsigned int)rom,(unsigned int)new_instr);
		#endif
		disarmcode(new_instr);
		
	}
	else{
		//Fetch Decode & Execute THUMB
		//Half Word alignment for opcode fetch. (doesn't ignore opcode's bit[0] globally, bc fetch will carry whatever HalfWord is)
		u16 new_instr=armfetchpc_thumb((rom&0xfffffffe));
		#ifdef DEBUGEMU
			iprintf("/******THUMB******/");
			iprintf("\n rom:%x [%x]\n",(unsigned int)rom,(unsigned int)new_instr);
		#endif
		disthumbcode(new_instr);
		
	}
	//HBLANK Handles CPU_EMULATE(); //checks and interrupts all the emulator kind of requests (IRQ,DMA)
}
else
	gba.cpustate=true;

/*
if(rom==0x081e2b31){
	iprintf("opcode for PC:(%x) [%x] \n",(unsigned int)rom,(unsigned int)armfetchpc_thumb((rom&0xfffffffe)));
	while(1);
}
*/

//read input is done already -> gba.P1

//increase PC depending on CPUmode
if (armstate==0)
	rom=rom+4;
else
	rom=rom+2;

//before anything, interrupts (GBA generated) are checked on NDS9 IRQ.s PU.C exceptirq()

//old dcache is discarded (required for translator & hardware differen RAM sync mechanism)
DC_InvalidateAll(); 

return 0;
}

//cpu interrupt
u32 cpuirq(u32 cpumode){

		//Enter cpu<mode>
		updatecpuflags(1,cpsrvirt,cpumode);
		gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]=rom; //gba->reg[14].I = PC;
		
		if(!armstate)//if(!savedState)
			gbavirtreg_cpu[0xe]+=2; //gba->reg[14].I += 2;
		armstate=0;//true
		gba.armirqenable = false;
		
		//refresh jump opcode in biosprotected vector
		gba.biosprotected[0] = 0x02;
		gba.biosprotected[1] = 0xc0;
		gba.biosprotected[2] = 0x5e;
		gba.biosprotected[3] = 0xe5;

		//set PC
		rom=gbavirtreg_cpu[0xf]=0x18;
		
		//swi_virt(swinum);
		
		//Restore CPU<mode>
		updatecpuflags(1,cpsrvirt,spsr_last&0x1F);

return 0;
}


//generate thread time based upon lcdticks
int __attribute((hot)) cpuupdateticks(int ticking){
	gba.lcdticks=ticking; //use (external) tick count for generation ticks
	
	if(gba.soundticks < gba.lcdticks){
		gba.soundticks+=gba.lcdticks; //element thread is set to zero when thread is executed.
		return (gba.soundticks);
	}
	else if (gba.switicks) {
		if (gba.switicks < gba.lcdticks){
			gba.switicks+=gba.lcdticks;
			return (gba.switicks);
		}
	}
	else if (gba.irqticks) {
		if (gba.irqticks < gba.lcdticks){
			gba.irqticks+=gba.lcdticks;
			return (gba.irqticks);
		}
	}
	/*
	//end of cycle , reset ticks
	else {
		gba.soundticks=0;
		gba.switicks=0;
		gba.irqticks=0;
	}
	*/
	return 0;
}


//hardware < - > virtual environment update CPU stats

//IO address, u32 value
u16 __attribute__ ((hot)) cpu_updateregisters(u16 address, u16 value){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

switch(address){
	case 0x00:{ 
		// we need to place the following code in { } because we declare & initialize variables in a case statement
		if((value & 7) > 5) {
			// display modes above 0-5 are prohibited
			gba.DISPCNT = (value & 7);
		}
		bool change = (0 != ((gba.DISPCNT ^ value) & 0x80)); //forced vblank? (1 means delta time vram access)
		bool changeBG = (0 != ((gba.DISPCNT ^ value) & 0x0F00));
		u16 changeBGon = ((~gba.DISPCNT) & value) & 0x0F00; // these layers are being activated
		
		gba.DISPCNT = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
		UPDATE_REG(0x00 , gba.DISPCNT); //UPDATE_REG(0x00, gba.DISPCNT);

		if(changeBGon) {
			gba.layerenabledelay = 4;
			gba.layerenable = gba.layersettings & value & (~changeBGon);
		}
		else {
			gba.layerenable = gba.layersettings & value;
		}

		gba.windowon = (gba.layerenable & 0x6000) ? true : false;
		if(change && !((value & 0x80))) {
			if(!(gba.DISPSTAT & 1)) {
				gba.lcdticks = 1008;
				//      VCOUNT = 0;
				//      UPDATE_REG(0x06, VCOUNT);
				gba.DISPSTAT &= 0xFFFC;
				UPDATE_REG(0x04 , gba.DISPSTAT); //UPDATE_REG(0x04, gba->DISPSTAT);
			}
		//        (*renderLine)(); //draw line
		}
		// we only care about changes in BG0-BG3
		if(changeBG) {
		}
	break;
    }
	case 0x04:
		gba.DISPSTAT = (value & 0xFF38) | (gba.DISPSTAT & 7);
		UPDATE_REG(0x04 , gba.DISPSTAT);	//UPDATE_REG(0x04, gba->DISPSTAT);
    break;
	case 0x06:
		// not writable
    break;
	case 0x08:
		gba.BG0CNT = (value & 0xDFCF);
		UPDATE_REG(0x08 , gba.BG0CNT);	//UPDATE_REG(0x08, gba->BG0CNT);
    break;
	case 0x0A:
		gba.BG1CNT = (value & 0xDFCF);
		UPDATE_REG(0x0A , gba.BG1CNT);	//UPDATE_REG(0x0A, gba->BG1CNT);
    break;
	case 0x0C:
		gba.BG2CNT = (value & 0xFFCF);
		UPDATE_REG(0x0C , gba.BG2CNT);	//UPDATE_REG(0x0C, gba->BG2CNT);
    break;
	case 0x0E:
		gba.BG3CNT = (value & 0xFFCF);
		UPDATE_REG(0x0E , gba.BG3CNT);	//UPDATE_REG(0x0E, gba->BG3CNT);
    break;
	case 0x10:
		gba.BG0HOFS = value & 511;
		UPDATE_REG(0x10 , gba.BG0HOFS);	//UPDATE_REG(0x10, gba->BG0HOFS);
    break;
	case 0x12:
		gba.BG0VOFS = value & 511;	
		UPDATE_REG(0x12 , gba.BG0VOFS);	//UPDATE_REG(0x12, gba->BG0VOFS);
    break;
	case 0x14:
		gba.BG1HOFS = value & 511;
		UPDATE_REG(0x14 , gba.BG1HOFS);	//UPDATE_REG(0x14, gba->BG1HOFS);
    break;
	case 0x16:
		gba.BG1VOFS = value & 511;
		UPDATE_REG(0x16 , gba.BG1VOFS);	//UPDATE_REG(0x16, gba->BG1VOFS);
    break;
	case 0x18:
		gba.BG2HOFS = value & 511;
		UPDATE_REG(0x18 , gba.BG2HOFS);	//UPDATE_REG(0x18, gba->BG2HOFS);
    break;
	case 0x1A:
		gba.BG2VOFS = value & 511;
		UPDATE_REG(0x1A , gba.BG2VOFS);	//UPDATE_REG(0x1A, gba->BG2VOFS);
    break;
	case 0x1C:
		gba.BG3HOFS = value & 511;
		UPDATE_REG(0x1C , gba.BG3HOFS);	//UPDATE_REG(0x1C, gba->BG3HOFS);
    break;
	case 0x1E:
		gba.BG3VOFS = value & 511;
		UPDATE_REG(0x1E , gba.BG3VOFS);	//UPDATE_REG(0x1E, gba->BG3VOFS);
    break;
	case 0x20:
		gba.BG2PA = value;
		UPDATE_REG(0x20 , gba.BG2PA);	//UPDATE_REG(0x20, gba->BG2PA);
    break;
	case 0x22:
		gba.BG2PB = value;
		UPDATE_REG(0x22 , gba.BG2PB);	//UPDATE_REG(0x22, gba->BG2PB);
    break;
	case 0x24:
		gba.BG2PC = value;
		UPDATE_REG(0x24 , gba.BG2PC);	//UPDATE_REG(0x24, gba->BG2PC);
    break;
	case 0x26:
		gba.BG2PD = value;
		UPDATE_REG(0x26 , gba.BG2PD);	//UPDATE_REG(0x26, gba->BG2PD);
    break;
	case 0x28:
		gba.BG2X_L = value;
		UPDATE_REG(0x28 , gba.BG2X_L);	//UPDATE_REG(0x28, gba->BG2X_L);
		gba.gfxbg2changed |= 1;
    break;
	case 0x2A:
		gba.BG2X_H = (value & 0xFFF);
		UPDATE_REG(0x2A , gba.BG2X_H);	//UPDATE_REG(0x2A, gba->BG2X_H);
		gba.gfxbg2changed |= 1;
    break;
	case 0x2C:
		gba.BG2Y_L = value;
		UPDATE_REG(0x2C , gba.BG2Y_L);	//UPDATE_REG(0x2C, gba->BG2Y_L);
		gba.gfxbg2changed |= 2;
    break;
	case 0x2E:
		gba.BG2Y_H = value & 0xFFF;
		UPDATE_REG(0x2E , gba.BG2Y_H);	//UPDATE_REG(0x2E, gba->BG2Y_H);
		gba.gfxbg2changed |= 2;
    break;
	case 0x30:
		gba.BG3PA = value;
		UPDATE_REG(0x30 , gba.BG3PA);		//UPDATE_REG(0x30, gba->BG3PA);
    break;
	case 0x32:
		gba.BG3PB = value;
		UPDATE_REG(0x32 , gba.BG3PB);		//UPDATE_REG(0x32, gba->BG3PB);
    break;
	case 0x34:
		gba.BG3PC = value;
		UPDATE_REG(0x34 , gba.BG3PC);		//UPDATE_REG(0x34, gba->BG3PC);
    break;
	case 0x36:
		gba.BG3PD = value;
		UPDATE_REG(0x36 , gba.BG3PD);		//UPDATE_REG(0x36, gba->BG3PD);
    break;
	case 0x38:
		gba.BG3X_L = value;
		UPDATE_REG(0x38 , gba.BG3X_L);		//UPDATE_REG(0x38, gba->BG3X_L);
		gba.gfxbg3changed |= 1;
    break;
	case 0x3A:
		gba.BG3X_H = value & 0xFFF;
		UPDATE_REG(0x3A , gba.BG3X_H);		//UPDATE_REG(0x3A, gba->BG3X_H);
		gba.gfxbg3changed |= 1;
    break;
	case 0x3C:
		gba.BG3Y_L = value;
		UPDATE_REG(0x3C , gba.BG3Y_L);		//UPDATE_REG(0x3C, gba->BG3Y_L);
		gba.gfxbg3changed |= 2;
    break;
	case 0x3E:
		gba.BG3Y_H = value & 0xFFF;
		UPDATE_REG(0x3E , gba.BG3Y_H);		//UPDATE_REG(0x3E, gba->BG3Y_H);
		gba.gfxbg3changed |= 2;
    break;
	case 0x40:
		gba.WIN0H = value;
		UPDATE_REG(0x40 , gba.WIN0H);			//UPDATE_REG(0x40, gba->WIN0H);
    break;
	case 0x42:
		gba.WIN1H = value;
		UPDATE_REG(0x42 , gba.WIN1H);			//UPDATE_REG(0x42, gba->WIN1H);
    break;
	case 0x44:
		gba.WIN0V = value;
		UPDATE_REG(0x44 , gba.WIN0V);			//UPDATE_REG(0x44, gba->WIN0V);
    break;
	case 0x46:
		gba.WIN1V = value;
		UPDATE_REG(0x46 , gba.WIN1V);			//UPDATE_REG(0x46, gba->WIN1V);
    break;
	case 0x48:
		gba.WININ = value & 0x3F3F;
		UPDATE_REG(0x48 , gba.WININ);			//UPDATE_REG(0x48, gba->WININ);
    break;
	case 0x4A:
		gba.WINOUT = value & 0x3F3F;
		UPDATE_REG(0x4A , gba.WINOUT);		//UPDATE_REG(0x4A, gba->WINOUT);
    break;
	case 0x4C:
		gba.MOSAIC = value;
		UPDATE_REG(0x4C , gba.MOSAIC);		//UPDATE_REG(0x4C, gba->MOSAIC);
    break;
	case 0x50:
		gba.BLDMOD = value & 0x3FFF;
		UPDATE_REG(0x50 , gba.BLDMOD);		//UPDATE_REG(0x50, gba->BLDMOD);
		gba.fxon = ((gba.BLDMOD>>6)&3) != 0;
    break;
	case 0x52:
		gba.COLEV = value & 0x1F1F;
		UPDATE_REG(0x52 , gba.COLEV);			//UPDATE_REG(0x52, gba.COLEV);
    break;
	case 0x54:
		gba.COLY = value & 0x1F;
		UPDATE_REG(0x54 , gba.COLY);			//UPDATE_REG(0x54, gba->COLY);
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
	
	//////////////////////////////////////DMA Writeback variables-> IO MEM
	case 0xB0:
		gba.DM0SAD_L = value;
		UPDATE_REG(0xB0 , gba.DM0SAD_L);	//UPDATE_REG(0xB0, gba->DM0SAD_L);
    break;
	case 0xB2:
		gba.DM0SAD_H = value & 0x07FF;
		UPDATE_REG(0xB2 , gba.DM0SAD_H);	//UPDATE_REG(0xB2, gba->DM0SAD_H);
    break;
	case 0xB4:
		gba.DM0DAD_L = value;
		UPDATE_REG(0xB4 , gba.DM0DAD_L);	//UPDATE_REG(0xB4, gba->DM0DAD_L);
    break;
	case 0xB6:
		gba.DM0DAD_H = value & 0x07FF;
		UPDATE_REG(0xB6 , gba.DM0DAD_H);	//UPDATE_REG(0xB6, gba->DM0DAD_H);
    break;
	case 0xB8:
		gba.DM0CNT_L = value & 0x3FFF;
		UPDATE_REG(0xB8 , 0);	//UPDATE_REG(0xB8, 0);
    break;
	case 0xBA:{
		bool start = ((gba.DM0CNT_H ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		gba.DM0CNT_H = value;
		UPDATE_REG(0xBA , gba.DM0CNT_H);	//UPDATE_REG(0xBA, gba->DM0CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma0source = gba.DM0SAD_L | (gba.DM0SAD_H << 16);
			gba.dma0dest = gba.DM0DAD_L | (gba.DM0DAD_H << 16);
			//CPUCheckDMA(gba, 0, 1); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xBC:
		gba.DM1SAD_L = value;
		UPDATE_REG(0xBC , gba.DM1SAD_L);	//UPDATE_REG(0xBC, gba->DM1SAD_L);
    break;
	case 0xBE:
		gba.DM1SAD_H = value & 0x0FFF;
		UPDATE_REG(0xBE , gba.DM1SAD_H);	//UPDATE_REG(0xBE, gba->DM1SAD_H);
    break;
	case 0xC0:
		gba.DM1DAD_L = value;
		UPDATE_REG(0xC0 , gba.DM1DAD_L);	//UPDATE_REG(0xC0, gba->DM1DAD_L);
    break;
	case 0xC2:
		gba.DM1DAD_H = value & 0x07FF;
		UPDATE_REG(0xC2 , gba.DM1DAD_H);	//UPDATE_REG(0xC2, gba->DM1DAD_H);
    break;
	
	//this is NOT RTC! (0x080000cx) != (0x040000cx), can be confusing.
	
	case 0xC4:
		
		gba.DM1CNT_L = value & 0x3FFF;
		
    break;
	case 0xC6:{	
		bool start = ((gba.DM1CNT_H ^ value) & 0x8000) ? true : false;
		value &= 0xF7E0;

		gba.DM1CNT_H = value;
		UPDATE_REG(0xC6 , gba.DM1CNT_H);	//UPDATE_REG(0xC6, gba->DM1CNT_H);

		
		if(start && (value & 0x8000)) {
			gba.dma1source = gba.DM1SAD_L | (gba.DM1SAD_H << 16);
			gba.dma1dest = gba.DM1DAD_L | (gba.DM1DAD_H << 16);
			//CPUCheckDMA(gba, 0, 2); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xC8:
	
		gba.DM2SAD_L = value;
		UPDATE_REG(0xC8 , gba.DM2SAD_L);	//UPDATE_REG(0xC8, gba->DM2SAD_L);
		
	break;
	
	case 0xCA:
		gba.DM2SAD_H = value & 0x0FFF;
		UPDATE_REG(0xCA , gba.DM2SAD_H);	//UPDATE_REG(0xCA, gba->DM2SAD_H);
    break;
	case 0xCC:
		gba.DM2DAD_L = value;
		UPDATE_REG(0xCC , gba.DM2DAD_L);	//UPDATE_REG(0xCC, gba->DM2DAD_L);
    break;
	case 0xCE:
		gba.DM2DAD_H = value & 0x07FF;
		UPDATE_REG(0xCE , gba.DM2DAD_H);	//UPDATE_REG(0xCE, gba->DM2DAD_H);
    break;
	case 0xD0:
		gba.DM2CNT_L = value & 0x3FFF;
		UPDATE_REG(0xD0 , gba.DM2CNT_L);	//UPDATE_REG(0xD0, 0);
    break;
	case 0xD2:{
		bool start = ((gba.DM2CNT_H ^ value) & 0x8000) ? true : false;
		
		value &= 0xF7E0;
		
		gba.DM2CNT_H = value;
		UPDATE_REG(0xD2 , gba.DM2CNT_H);	//UPDATE_REG(0xD2, gba->DM2CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma2source = gba.DM2SAD_L | (gba.DM2SAD_H << 16);
			gba.dma2dest = gba.DM2DAD_L | (gba.DM2DAD_H << 16);

			//CPUCheckDMA(gba, 0, 4); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
    break;
	case 0xD4:
		gba.DM3SAD_L = value;
		UPDATE_REG(0xD4 , gba.DM3SAD_L);	//UPDATE_REG(0xD4, gba->DM3SAD_L);
    break;
	case 0xD6:
		gba.DM3SAD_H = value & 0x0FFF;
		UPDATE_REG(0xD6 , gba.DM3SAD_H);	//UPDATE_REG(0xD6, gba->DM3SAD_H);
    break;
	case 0xD8:
		gba.DM3DAD_L = value;
		UPDATE_REG(0xD8 , gba.DM3DAD_L);	//UPDATE_REG(0xD8, gba.DM3DAD_L);
    break;
	case 0xDA:
		gba.DM3DAD_H = value & 0x0FFF;
		UPDATE_REG(0xDA , gba.DM3DAD_H);	//UPDATE_REG(0xDA, gba->DM3DAD_H);
    break;
	case 0xDC:
		gba.DM3CNT_L = value;
		UPDATE_REG(0xDC , gba.DM3CNT_L);	//UPDATE_REG(0xDC, 0);
    break;
	case 0xDE:{
		bool start = ((gba.DM3CNT_H ^ value) & 0x8000) ? true : false;
		
		value &= 0xFFE0;
		
		gba.DM3CNT_H = value;
		UPDATE_REG(0xDE , gba.DM3CNT_H);	//UPDATE_REG(0xDE, gba->DM3CNT_H);

		if(start && (value & 0x8000)) {
			gba.dma3source = gba.DM3SAD_L | (gba.DM3SAD_H << 16);
			gba.dma3dest = gba.DM3DAD_L | (gba.DM3DAD_H << 16);
			//CPUCheckDMA(gba, 0, 8); //launch DMA hardware , user dma args , serve them and unset dma used bits
		}
    }
	
	////////////////////////////////////// DMA END
	
    break;
	case 0x100:
		gba.timer0reload = value;
    break;
	case 0x102:
		gba.timer0value = value;
		//gba.timerOnOffDelay|=1; //added delta before activating timer?
		gba.cpunextevent = cputotalticks; //ori: gba.cputotalticks;
    break;
	case 0x104:
		gba.timer1reload = value;
    break;
	case 0x106:
		gba.timer1value = value;
		//gba->timerOnOffDelay|=2;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;
	case 0x108:
		gba.timer2reload = value;
    break;
	case 0x10A:
		gba.timer2value = value;
		//gba.timerOnOffDelay|=4;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;
	case 0x10C:
		gba.timer3reload = value;
    break;
	case 0x10E:
		gba.timer3value = value;
		//gba->timerOnOffDelay|=8;
		gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
    break;

	case 0x130:
		gba.P1 |= (value & 0x3FF);
		UPDATE_REG(0x130 , gba.P1);	//UPDATE_REG(0x130, gba->P1);
	break;

	case 0x132:
		UPDATE_REG(0x132 , value & 0xC3FF);	//UPDATE_REG(0x132, value & 0xC3FF);
	break;

	case 0x200:
		gbavirt_iemasking = (value & 0x3FFF);
		UPDATE_REG(0x200 , gbavirt_iemasking);	//UPDATE_REG(0x200, gba->IE);
		if ((gbavirt_imemasking & 1) && (gbavirt_ifmasking & gbavirt_iemasking) && (i_flag==true))
			gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks; //acknowledge cycle & program flow
	break;
	case 0x202:
		gbavirt_ifmasking ^= (value & gbavirt_ifmasking);
		UPDATE_REG(0x202 , gbavirt_ifmasking);	//UPDATE_REG(0x202, gba.IF);
    break;
	case 0x204:{
		gba.memorywait[0x0e] = gba.memorywaitseq[0x0e] = gamepakramwaitstate[value & 3];

		gba.memorywait[0x08] = gba.memorywait[0x09] = gamepakwaitstate[(value >> 2) & 3];
		gba.memorywaitseq[0x08] = gba.memorywaitseq[0x09] =
		gamepakwaitstate0[(value >> 4) & 1];

		gba.memorywait[0x0a] = gba.memorywait[0x0b] = gamepakwaitstate[(value >> 5) & 3];
		gba.memorywaitseq[0x0a] = gba.memorywaitseq[0x0b] =
		gamepakwaitstate1[(value >> 7) & 1];

		gba.memorywait[0x0c] = gba.memorywait[0x0d] = gamepakwaitstate[(value >> 8) & 3];
		gba.memorywaitseq[0x0c] = gba.memorywaitseq[0x0d] =
		gamepakwaitstate2[(value >> 10) & 1];
		
		/* //not now
		for(i = 8; i < 15; i++) {
			gba.memorywait32[i] = gba.memorywait[i] + gba.memorywaitseq[i] + 1;
			gba.memorywaitseq32[i] = gba.memorywaitseq[i]*2 + 1;
		}*/

		if((value & 0x4000) == 0x4000) {
			gba.busprefetchenable = true;
			gba.busprefetch = false;
			gba.busprefetchcount = 0;
		} 
		else {
			gba.busprefetchenable = false;
			gba.busprefetch = false;
			gba.busprefetchcount = 0;
		}
		UPDATE_REG(0x204 , value & 0x7FFF); //UPDATE_REG(0x204, value & 0x7FFF);

	}
    break;
	case 0x208:
		gbavirt_imemasking = value & 1;
		UPDATE_REG(0x208 , gbavirt_imemasking); //UPDATE_REG(0x208, gba->IME);
		if ((gbavirt_imemasking & 1) && (gbavirt_ifmasking & gbavirt_iemasking) && (i_flag==true))
			gba.cpunextevent = cputotalticks;	//ori: gba.cpunextevent = gba.cputotalticks;
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
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]);
return u32read(addr_field | (address & 0x3FFFF));
}

u16 virtread_hword(u32 address){
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]);
return u16read(addr_field | (address & 0x3FFFF));
}

u8 virtread_byte(u32 address){
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]);
return u8read(addr_field | (address & 0x3FFFF));
}

//direct GBA CPU writes
u32 virtwrite_word(u32 address,u32 data){
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]);
	u32store(addr_field | (address & 0x3FFFF), data);
return 0;
}

u16 virtwrite_hword(u32 address,u16 data){
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]);
	u16store(addr_field | (address & 0x3FFFF), data);
return 0;
}

u8 virtwrite_byte(u32 address,u8 data){
	u32 addr_field = addresslookup(address, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (address & 0x3FFFF);
	u8store(addr_field | (address & 0x3FFFF), data);
return 0;
}

//hamming weight read from memory CPU opcodes

//#define CPUReadByteQuick(gba, addr)
//(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]

u8 __attribute__ ((hot)) cpuread_bytefast(u8 address){
/*
#ifdef DEBUGEMU
	debuggeroutput();
#endif
*/
return  u8read(
		(u32)&(gba).map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask]
		);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadHalfWordQuick(gba, addr)
//READ16LE(((u16*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
  
u16 __attribute__ ((hot)) cpuread_hwordfast(u16 address){
/*
#ifdef DEBUGEMU
	debuggeroutput();
#endif
*/
return  u16read(
		(u32)&(gba).map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask]
		);
}

//this call resembles the original gba memory addressing through vector map[i]
//#define CPUReadMemoryQuick(gba, addr) 
//READ32LE(((u32*)&(gba)->map[(addr)>>24].address[(addr) & (gba)->map[(addr)>>24].mask]))
		
u32 __attribute__ ((hot)) cpuread_wordfast(u32 address){
/*
#ifdef DEBUGEMU
	debuggeroutput();
#endif
*/
return  u32read(
		((u32)&gba.map[(address)>>24].address[(address) & (gba).map[(address)>>24].mask])
		);
}

////////////////////////////////////VBA CPU read mechanism/////////////////////////////////////////////// 
//u8 CPUGBA reads
//old: CPUReadByte(GBASystem *gba, u32 address);
u8 __attribute__ ((hot)) cpuread_byte(u32 address){
//#ifdef DEBUGEMU
//debuggeroutput();
//#endif

switch(address >> 24) {
	case 0:
		if ((rom) >> 24) { //PC fetch
			if(address < 0x4000) {
				#ifdef DEBUGEMU
					iprintf("byte: gba.bios protected read! (%x) \n",(unsigned int)address);
				#endif
				//return gba->biosProtected[address & 3];
				return u8read((u32)gba.biosprotected+(address & 3));
			} 
			else 
				goto unreadable;
		}
	#ifdef DEBUGEMU
		iprintf("byte: gba.bios read! (%x) \n",(unsigned int)address);
	#endif
	
		//return gba->bios[address & 0x3FFF];
		return u8read((u32)gba.bios+(address & 0x3FFF));
	break;
	case 2:
		#ifdef DEBUGEMU
			iprintf("byte: gba.workram read! (%x) \n",(unsigned int)address);
		#endif
		
		//return gba->workRAM[address & 0x3FFFF];
		return u8read((u32)gba.workram+(address & 0x3FFFF));
	break;
	case 3:
		#ifdef DEBUGEMU
			iprintf("byte: gba.intram read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->internalRAM[address & 0x7fff];
		//ARM reads are 
		return u8read((u32)gba.intram+(address & 0x7fff));
		
	break;
	case 4:
		if((address < 0x4000400) && gba.ioreadable[address & 0x3ff]){
			#ifdef DEBUGEMU
				iprintf("byte: gba.IO read! (%x) \n",(unsigned int)address);
			#endif
			
			//ori: return gba->ioMem[address & 0x3ff];
			return READ_IOREG8(address & 0x3ff);
		}
		else 
			goto unreadable;
	break;
	case 5:
		#ifdef DEBUGEMU
			iprintf("byte: gba.palram read! (%x) \n",(unsigned int)address);
		#endif
		
		//return gba->paletteRAM[address & 0x3ff];
		return u8read((u32)gba.palram+(address & 0x3ff));
	break;
	case 6:
		address = (address & 0x1ffff);
		if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;
		#ifdef DEBUGEMU
			iprintf("byte: gba.vidram read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->vram[address];
		return u8read((u32)gba.vidram+(address & 0xffffffff));
	break;
	case 7:
		#ifdef DEBUGEMU
			iprintf("byte: gba.oam read! (%x) \n",(unsigned int)address);
		#endif
		//return gba->oam[address & 0x3ff];
		return u8read((u32)gba.oam+(address & 0x3ff));
	break;
	case 8: case 9: case 10: case 11: case 12:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			
			//value = rtcRead(address);
			
			//calculate RTC/Sensor/ETC thread
			//	80000C4h - I/O Port Data (selectable W or R/W)
			// 	bit0-3  Data Bits 0..3 (0=Low, 1=High)
			//  bit4-15 not used (0)

			//80000C6h - I/O Port Direction (for above Data Port) (selectable W or R/W)
			//bit0-3  Direction for Data Port Bits 0..3 (0=In, 1=Out)
			//bit4-15 not used (0)

			//80000C8h - I/O Port Control (selectable W or R/W)
			//bit0    Register 80000C4h..80000C8h Control (0=Write-Only, 1=Read/Write)
			//bit1-15 not used (0)
			//In write-only mode, reads return 00h (or possible other data, if the rom contains non-zero data at that location).
			
			//check IO port...:
			//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
			//is readable GPIO? 
			//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
			//[0] (1 high- can be accesed | 2 low - cannot be accessed)
			struct GBASystem * gba_inst = &gba;
			if( ((gpioread8(gba_inst,2)&1)==1) && ((gpioread8(gba_inst,0)&1)==1) ){
				
				//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
				switch(address&0xff){
					case(0xc4):
						//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 8);
						#endif
						
						process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
						return (u8)(gpioread8(gba_inst,0));
					break;
					
					case(0xc6):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 8);
						#endif
						
						process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
						return (u8)(gpioread8(gba_inst,1));
					break;
					
					case(0xc8):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 8);
						#endif
						
						process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
						return (u8)(gpioread8(gba_inst,2));
					break;
				}
				
			}
			else{
				#ifndef ROMTEST
					//return (u8)(stream_readu8(address % 0x08000000)); //gbareads SHOULD NOT be aligned NON-POSIX STANDARD
					return (u8) readu8gbarom(address& 0x01FFFFFE);
				#endif
				
				#ifdef ROMTEST
					return (u8)*(u8*)(&rom_pl_bin+((address % 0x08000000)/4 ));
				#endif
			}
		}
		else{
			#ifndef ROMTEST
				//return (u8)(stream_readu8(address % 0x08000000)); //gbareads SHOULD NOT be aligned NON-POSIX STANDARD
				return (u8) readu8gbarom(address& 0x01FFFFFE);
			#endif
			
			#ifdef ROMTEST
				return (u8)*(u8*)(&rom_pl_bin+((address % 0x08000000)/4 ));
			#endif
		}
	break;
	case 0xd:
		if(gba.cpueepromenabled)
			//return eepromRead(address); //saves not yet
			return 0;
		else
			goto unreadable;
	break;
	case 0xe:
		if(gba.cpusramenabled | gba.cpuflashenabled)
			//return flashRead(address);
			return flashread();
			
		else
			goto unreadable;
			
		if(gba.cpueepromsensorenabled) {
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
	default:{
	unreadable:
		
		//arm read
		if(armstate==0) {
			//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
			#ifdef DEBUGEMU
				iprintf("byte:[ARM] default read! (%x) \n",(unsigned int)address);
			#endif
			
			return virtread_word(rom);
		} 
		
		//thumb read
		else{
			//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
			//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
			#ifdef DEBUGEMU
			iprintf("byte:[THMB] default read! (%x) \n",(unsigned int)address);
			#endif
			return ( (virtread_hword(rom)) | ((virtread_hword(rom))<< 16) ); //half word duplicate
		}
		
	break;
	}
}

return 0;
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
		if ((rom) >> 24) { //PC fetch
			if(address < 0x4000) {
				//value = ldru16extasm((u32)(u8*)gba.biosprotected,(address&2)); 	//value = READ16LE(((u16 *)&gba->biosProtected[address&2]));
				//#ifdef DEBUGEMU
				//iprintf("hword gba.biosprotected read! \n");
				//#endif
				
				#ifdef DEBUGEMU
				iprintf("hword gba.bios read! \n");
				#endif
				//value = ldru16extasm((u32)(u8*)gba.bios,address);
				value = u16read((u32)gba.bios + (address & 0x3FFE));
			} 
			else 
				goto unreadable;
		} 
		else {
			#ifdef DEBUGEMU
			iprintf("hword gba.bios read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->bios[address & 0x3FFE]));
			value = u16read((u32)gba.bios + (address & 0x3FFE));
		}
	break;
	case 2:
			#ifdef DEBUGEMU
			iprintf("hword gba.workram read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->workRAM[address & 0x3FFFE]));
			value = u16read((u32)gba.workram + (address & 0x3FFFE));
	break;
	case 3:
			#ifdef DEBUGEMU
			iprintf("hword gba.intram read! \n");
			#endif
			//value = READ16LE(((u16 *)&gba->internalRAM[address & 0x7ffe]));
			value = u16read((u32)gba.intram + (address & 0x7ffe));
	break;
	case 4:
		if((address < 0x4000400) && gba.ioreadable[address & 0x3fe]){
			#ifdef DEBUGEMU
			iprintf("hword gba.IO read! \n");
			#endif
			
			//value =  READ16LE(((u16 *)&gba->ioMem[address & 0x3fe]));
			value = READ_IOREG16(address & 0x3fe);
				
				if (((address & 0x3fe)>0xFF) && ((address & 0x3fe)<0x10E)){
					
					if (((address & 0x3fe) == 0x100) && gba.timer0on)
						//ori: value = 0xFFFF - ((gba.timer0ticks - gba.cputotalticks) >> gba.timer0clockreload); //timer top - timer period
						value = 0xFFFF - ((gba.timer0ticks - cputotalticks) >> gba.timer0clockreload);
						
					else {
						if (((address & 0x3fe) == 0x104) && gba.timer1on && !(gba.TM1CNT & 4))
							//ori: value = 0xFFFF - ((gba.timer1ticks - gba.cputotalticks) >> gba.timer1clockreload);
							value = 0xFFFF - ((gba.timer1ticks - cputotalticks) >> gba.timer1clockreload);
						else
							if (((address & 0x3fe) == 0x108) && gba.timer2on && !(gba.TM2CNT & 4))
								//ori: value = 0xFFFF - ((gba.timer2ticks - gba.cputotalticks) >> gba.timer2clockreload);
								value = 0xFFFF - ((gba.timer2ticks - cputotalticks) >> gba.timer2clockreload);
							else
								if (((address & 0x3fe) == 0x10C) && gba.timer3on && !(gba.TM3CNT & 4))
									//ori: value = 0xFFFF - ((gba.timer3ticks - gba.cputotalticks) >> gba.timer2clockreload);
									value = 0xFFFF - ((gba.timer3ticks - cputotalticks) >> gba.timer2clockreload);
					}
				}
		}
		else 
			goto unreadable;
    break;
	case 5:
		#ifdef DEBUGEMU
			iprintf("hword gba.palram read! \n");
		#endif
		//value = READ16LE(((u16 *)&gba->paletteRAM[address & 0x3fe]));
		value = u16read((u32)gba.palram + (address & 0x3fe));
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			#ifdef DEBUGEMU
			iprintf("hword gba.vidram read! \n");
			#endif
		//value = READ16LE(((u16 *)&gba->vram[address]));
		value = u16read((u32)gba.vidram + (address&0xffffffff));
	break;
	case 7:
			#ifdef DEBUGEMU
			iprintf("hword gba.oam read! \n");
			#endif
		//value = READ16LE(((u16 *)&gba->oam[address & 0x3fe]));
		value = u16read((u32)gba.oam + (address & 0x3fe));
	break;
	case 8: case 9: case 10: case 11: case 12:
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			//value = rtcRead(address);	//RTC not yet
			
			//calculate RTC/Sensor/ETC thread
			//	80000C4h - I/O Port Data (selectable W or R/W)
			// 	bit0-3  Data Bits 0..3 (0=Low, 1=High)
			//bit4-15 not used (0)

			//80000C6h - I/O Port Direction (for above Data Port) (selectable W or R/W)
			//bit0-3  Direction for Data Port Bits 0..3 (0=In, 1=Out)
			//bit4-15 not used (0)

			//80000C8h - I/O Port Control (selectable W or R/W)
			//bit0    Register 80000C4h..80000C8h Control (0=Write-Only, 1=Read/Write)
			//bit1-15 not used (0)
			//In write-only mode, reads return 00h (or possible other data, if the rom contains non-zero data at that location).

			//check IO port...:
			//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
			//is readable GPIO? 
			//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
			//[0] (1 high- can be accesed | 2 low - cannot be accessed)
			struct GBASystem * gba_inst = &gba;
			if( ((gpioread8(gba_inst,2)&1)==1) && ((gpioread8(gba_inst,0)&1)==1) ){
				
				//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
				switch(address&0xff){
					case(0xc4):
						//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 16);
						#endif
			
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						return (u16)(gpioread16(gba_inst,0));
					break;
					
					case(0xc6):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 16);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						return (u16)(gpioread16(gba_inst,1));
					break;
					
					case(0xc8):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, 0, 0, 16);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						return (u16)(gpioread16(gba_inst,2));
					break;
				}
				
			}
			else{
				#ifndef ROMTEST
					//return (u16)(stream_readu8(address % 0x08000000)); //gbareads SHOULD NOT be aligned NON-POSIX STANDARD
					return (u16) readu16gbarom(address& 0x01FFFFFE);
				#endif
				
				#ifdef ROMTEST
					return (u16)*(u8*)(&rom_pl_bin+((address % 0x08000000)/4 ));
				#endif
			}
		}
		else{
			//value = READ16LE(((u16 *)&gba->rom[address & 0x1FFFFFE]));
			
			#ifndef ROMTEST
				//return stream_readu16(address % 0x08000000); 
				
				return readu16gbarom(address& 0x01FFFFFE); //gbareads are never word aligned. (why would you want that?!)
			#endif
		
			#ifdef ROMTEST
				return u16read((u16)(&rom_pl_bin+((address % 0x08000000)/4 )));
			#endif
		}
	break;
	case 0xd:
		if(gba.cpueepromenabled)
		// no need to swap this
		//return  eepromRead(address);	//saving not yet
    goto unreadable;
  
	case 0xe:
		if(gba.cpuflashenabled | gba.cpusramenabled){
		//no need to swap this
		//return flashRead(address);	//saving not yet
		return flashread();
		}
	//default
	default:{
	unreadable:
			
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				iprintf("hword default read! \n");
				#endif
				//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
				value=cpuread_wordfast(rom);
			
			//thumb read
			} 
			else{
				#ifdef DEBUGEMU
				iprintf("hword default read! (x2) \n");
				#endif
				
				//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
				//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
				value = cpuread_hwordfast(rom) | (cpuread_hwordfast(rom) << 16);
			}
	break;
	}
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
			
			if((rom) >> 24) { //PC fetch
				if(address < 0x4000) {
					//value = READ32LE(((u32 *)&gba->biosProtected));
					//value = ldru32asm((u32)(u8*)gba.biosprotected,0x0);
					//iprintf("IO access!");
					//#ifdef DEBUGEMU
					//iprintf("Word gba.biosprotected read! \n");
					//#endif
					#ifdef DEBUGEMU
						iprintf("Word gba.bios read! (%x)[%x] \n",(unsigned int)((u32)(u8*)gba.bios+address),(unsigned int)*(u32*)((u32)(u8*)gba.bios+address));
					#endif
				
					value = u32read((u32)gba.bios + (address & 0x3FFC));
				}
				else 
					goto unreadable;
			}
			else{
				//iprintf("any access!");
				//value = READ32LE(((u32 *)&gba->bios[address & 0x3FFC]));
				//ori:
				//value = ldru32extasm((u32)(u8*)gba.bios,(address & 0x3FFC));
				//#ifdef DEBUGEMU
				//iprintf("Word gba.biosprotected read! \n");
				//#endif
				
				#ifdef DEBUGEMU
					iprintf("Word gba.bios read! (%x)[%x] \n",(unsigned int)((u32)(u8*)gba.bios+address),(unsigned int)u32read((u32)gba.bios + address));
				#endif
				
				value = u32read((u32)gba.bios + (address & 0x3FFC));
			}
		break;
		case 2:
			#ifdef DEBUGEMU
			iprintf("Word gba.workram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->workRAM[address & 0x3FFFC]));
			value = u32read((u32)gba.workram + (address & 0x3FFFC));
		break;
		case 3:
			//stacks should be here..
			#ifdef DEBUGEMU
			iprintf("Word gba.intram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->internalRAM[address & 0x7ffC]));
			value = u32read((u32)gba.intram + (address & 0x7ffC));
		break;
		case 4: //ioReadable is mapped
			if((address < 0x4000400) && gba.ioreadable[address & 0x3fc]) {
				//u16 or u32 (aligned)
				if(gba.ioreadable[(address & 0x3fc) + 2]) {
					//value = READ32LE(((u32 *)&gba->ioMem[address & 0x3fC]));
					value = READ_IOREG32(address & 0x3fe);
					#ifdef DEBUGEMU
						iprintf("Word gba.iomem read![%x] \n",(unsigned int)value);
					#endif
				}
				else{
					#ifdef DEBUGEMU
						iprintf("HWord gba.iomem read! \n");
					#endif
					
					//value = READ16LE(((u16 *)&gba->ioMem[address & 0x3fc]));
					value = READ_IOREG16(address & 0x3fe);
				}
				//nds DMA
				//if((address>=0x040000b0) || (address<=0x040000DE)){
				//	iprintf("DMA Request!");
				//}
				//todo: dmawrite control & wordcnt add into registers (so it is taken later on the HBLANK thread of DMA)
			}
			else
				goto unreadable;
		break;
		case 5:
			#ifdef DEBUGEMU
			iprintf("Word gba.palram read! \n");
			#endif
					
			//value = READ32LE(((u32 *)&gba->paletteRAM[address & 0x3fC]));
			value = u32read((u32)gba.palram + (address & 0x3fC));
		break;
		case 6:
			address = (address & 0x1fffc);
			if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000)){
				value = 0;
				break;
			}
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
			
			#ifdef DEBUGEMU
			iprintf("Word gba.vidram read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->vram[address]));
			value = u32read((u32)gba.vidram + (address));	
		break;
		case 7:
			#ifdef DEBUGEMU
			iprintf("Word gba.oam read! \n");
			#endif
			
			//value = READ32LE(((u32 *)&gba->oam[address & 0x3FC]));
			//value = ldru32extasm((u32)(u8*)gba.oam,(address & 0x3FC));
			value = u32read((u32)gba.oam + (address & 0x3FC));
		break;
		case 8: case 9: case 10: case 11: case 12:{
			
			if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			
				//value = rtcRead(address);	//RTC not yet
				//calculate RTC/Sensor/ETC thread
				//	80000C4h - I/O Port Data (selectable W or R/W)
				// 	bit0-3  Data Bits 0..3 (0=Low, 1=High)
				//bit4-15 not used (0)

				//80000C6h - I/O Port Direction (for above Data Port) (selectable W or R/W)
				//bit0-3  Direction for Data Port Bits 0..3 (0=In, 1=Out)
				//bit4-15 not used (0)

				//80000C8h - I/O Port Control (selectable W or R/W)
				//bit0    Register 80000C4h..80000C8h Control (0=Write-Only, 1=Read/Write)
				//bit1-15 not used (0)
				//In write-only mode, reads return 00h (or possible other data, if the rom contains non-zero data at that location).

				//check IO port...:
				//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
				//is readable GPIO? 
				//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
				//[0] (1 high- can be accesed | 2 low - cannot be accessed)
				struct GBASystem * gba_inst = &gba;
				if( ((gpioread8(gba_inst,2)&1)==1) && ((gpioread8(gba_inst,0)&1)==1) ){
				
					//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
					switch(address&0xff){
						case(0xc4):
							//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
							#ifdef DEBUGEMU
								debugrtc_gbavirt(gba_inst,address, 0, 0, 32);
							#endif
							
							process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
							return (u32)(gpioread32(gba_inst,0));
						break;
					
						case(0xc6):
							#ifdef DEBUGEMU
								debugrtc_gbavirt(gba_inst,address, 0, 0, 32);
							#endif
							
							process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
							return (u32)(gpioread32(gba_inst,1));
						break;
					
						case(0xc8):
							#ifdef DEBUGEMU
								debugrtc_gbavirt(gba_inst,address, 0, 0, 32);
							#endif
							
							process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
							return (u32)(gpioread32(gba_inst,2));
						break;
					}
				
				}
				else{
					#ifndef ROMTEST
						//return (u32)(stream_readu32(address % 0x08000000)); //gbareads SHOULD NOT be aligned NON-POSIX STANDARD
						return (u32) readu32gbarom(address& 0x01FFFFFE);
					#endif
				
					#ifdef ROMTEST
						return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
					#endif
				}
			}
			else{
		
				#ifndef ROMTEST
					//return (u32)(stream_readu32(address % 0x08000000)); //gbareads are OK and from HERE should NOT be aligned (that is module dependant)
					//NON-POSIX
					return (u32)readu32gbarom(address& 0x01FFFFFE);
				#endif
			
				#ifdef ROMTEST
					//return (u32)*(u32*)(&rom_pl_bin+((address % 0x08000000)/4 ));
					return u32read((u32)(&rom_pl_bin+((address % 0x08000000)/4 )),0x0);
				#endif
			}
		break;
		}
		case 0xd:
			//if(gba.cpueepromenabled)
				// no need to swap this
					//return eepromRead(address);	//save not yet
		break;
		goto unreadable;
		case 0xe:
			if((gba.cpuflashenabled==1) || (gba.cpusramenabled==1) )
				// no need to swap this
					//return flashRead(address);	//save not yet
				return flashread();
		break;
		
		default:{
		unreadable:
			//iprintf("default wordread");
			//struct: map[index].address[(u8*)address]
			//arm read
			if(armstate==0) {
				#ifdef DEBUGEMU
				iprintf("WORD:default read! (%x)[%x] \n",(unsigned int)address,(unsigned int)value);
				#endif
				
				//value = CPUReadMemoryQuick(gba, gba->reg[15].I);
				value=cpuread_wordfast(rom);
				
			//thumb read
			} 
			else{
				//value = CPUReadHalfWordQuick(gba, gba->reg[15].I) |
				//CPUReadHalfWordQuick(gba, gba->reg[15].I) << 16;
				#ifdef DEBUGEMU
				iprintf("HWORD:default read! (%x)[%x] \n",(unsigned int)address,(unsigned int)value);
				#endif
				value=cpuread_hwordfast((u16)(rom)) | (cpuread_hwordfast((u16)(rom))<<16) ;
			}
		break;
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
		iprintf("writebyte: writing to gba.workram");
		#endif
		
		//gba->workRAM[address & 0x3FFFF] = b;
		u8store((u32)gba.workram+(address & 0x3FFFF),b);
	break;
	case 3:
		#ifdef DEBUGEMU
		iprintf("writebyte: writing to gba.intram");
		#endif
		
		//gba->internalRAM[address & 0x7fff] = b;
		u8store((u32)gba.intram+(address & 0x7fff),b);
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
						iprintf("writebyte: updating AUDIO/CNT MAP");
					#endif
					//soundEvent(gba, address&0xFF, b); //sound not yet
				break;
				case 0x301: // HALTCNT, undocumented
					if(b == 0x80)
						//gba.stop = true;
					//gba->holdState = 1;
					//gba->holdType = -1;
					gba.cpunextevent = cputotalticks; //ori: gba.cpunextevent = gba.cputotalticks;
					
				break;
				default:{ // every other register
						u32 lowerBits = (address & 0x3fe);
						if(address & 1) {
							#ifdef DEBUGEMU
								iprintf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE(&gba->ioMem[lowerBits]) & 0x00FF) | (b << 8));
							cpu_updateregisters(lowerBits, (u8read(gba.iomem[lowerBits])) | (b << 8));
						} 
						else {
							#ifdef DEBUGEMU
								iprintf("writebyte: updating IO MAP");
							#endif
							//CPUUpdateRegister(gba, lowerBits, (READ16LE((u32)&gba.iomem[lowerBits]) & 0xFF00) | b);
							cpu_updateregisters(lowerBits, ( (u16read(gba.iomem[lowerBits])) & 0xFF00) | (b));
						}
				}
				break;
			}
		}
    break;
	case 5:
		
		#ifdef DEBUGEMU
		iprintf("writebyte: writing to gba.palram");
		#endif
		
		//VRAM & PALRAM does not accept 8 bit writes (instead 16 bit writes is used)
		//no need to switch
		//*((u16 *)&gba->paletteRAM[address & 0x3FE]) = (b << 8) | b;
		u16store((u32)gba.palram + (address & 0x3FE),(b << 8) | b);
	break;
	case 6:
		address = (address & 0x1fffe);
		if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
			return 0;
		if ((address & 0x18000) == 0x18000)
			address &= 0x17fff;

		// no need to switch
		// byte writes to OBJ VRAM are ignored
		if ((address) < objtilesaddress[((gba.DISPCNT&7)+1)>>2]){
			#ifdef DEBUGEMU
			iprintf("writebyte: writing to gba.vidram");
			#endif
			
			//VRAM & PALRAM does not accept 8 bit writes (instead 16 bit writes is used)
			//*((u16 *)&gba->vram[address]) = (b << 8) | b;
			//stru16inlasm((u32)(u8*)gba.vidram,(address),(b << 8) | b);
			u16store((u32)gba.vidram + (address&0xffffffff),(b << 8) | b);
		}
    break;
	case 7:
		// no need to switch
		// byte writes to OAM are ignored
		// *((u16 *)&oam[address & 0x3FE]) = (b << 8) | b;
    break;
	case 8:
	case 9:{	
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8){
			
			//check IO port...:
			//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
			//is readable GPIO? 
			//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
			//[0] (1 high- can be accesed | 2 low - cannot be accessed)
			
			struct GBASystem * gba_inst = &gba;
			
			//assert signal HIGH
			gpiowrite8(gba_inst,0,0x1);
			
			if((gpioread8(gba_inst,0)&1)==1){
				#ifdef DEBUGEMU
					iprintf("[writebyte]GPIO cmd is asserted high [%x] \n ",(u8)(gpioread8(gba_inst,0)));
				#endif
				//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
				switch(address&0xff){
					case(0xc4):
						//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, b, 1, 8);
						#endif
						
						process_iocmd(gba_inst,(u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite8(gba_inst,0,b);
						
					break;
					
					case(0xc6):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, b, 1, 8);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite8(gba_inst,1,b);
						
					break;
					
					case(0xc8):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, b, 1, 8);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite8(gba_inst,2,b);
					break;
				}
				
			}
			else {
				iprintf("[writebyte]GPIO cmd is asserted low [%x] \n ",(u8)(gpioread8(gba_inst,0)));
			}

			return 0; //end write event
		}
		
	break;
	}
	
	case 0xd:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, b);	//saves not yet
			break;
		}
	break;
	//goto unwritable;
	
	case 0xe: default:
		
		#ifdef DEBUGEMU
		iprintf("writebyte: default write!");
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
			iprintf("writehword: gba.workram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->workRAM[address & 0x3FFFE]),value);
		u16store((u32)gba.workram+(address & 0x3FFFE),value);
	break;
	case 3:
		#ifdef DEBUGEMU
			iprintf("writehword: gba.intram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->internalRAM[address & 0x7ffe]), value);
		u16store((u32)gba.intram+(address & 0x7ffe),value);
	break;
	case 4:
		if(address < 0x4000400){
			iprintf("writehword: gba.IO regs write!");
			cpu_updateregisters(address & 0x3fe, value);
		}
		//ignore everything else @ 0x040004xx
    break;
	case 5:
		#ifdef DEBUGEMU
			iprintf("writehword: gba.palram write!");
		#endif
			
		//WRITE16LE(((u16 *)&gba->paletteRAM[address & 0x3fe]), value);
		u16store((u32)gba.palram+(address & 0x3fe),value);
	break;
	case 6:
		address = (address & 0x1fffe);
			if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		#ifdef DEBUGEMU
			iprintf("writehword: gba.vidram write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->vram[address]), value);
		u16store((u32)gba.vidram+(address&0xffffffff),value);
	break;
	case 7:
		#ifdef DEBUGEMU
			iprintf("writehword: gba.oam write!");
		#endif
		
		//WRITE16LE(((u16 *)&gba->oam[address & 0x3fe]), value);
		u16store((u32)gba.oam+(address & 0x3fe),value);
		
	break;
	case 8:
	case 9:
		if(address == 0x080000c4 || address == 0x080000c6 || address == 0x080000c8) {
			
			//check IO port...:
			//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
			//is readable GPIO? 
			//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
			//[0] (1 high- can be accesed | 2 low - cannot be accessed)
			
			struct GBASystem * gba_inst = &gba;
			
			//assert signal HIGH
			gpiowrite8(gba_inst,0,0x1);
			
			if((gpioread8(gba_inst,0)&1)==1){
				#ifdef DEBUGEMU
					iprintf("[writehword]GPIO cmd is asserted high [%x] \n ",(u8)(gpioread8(gba_inst,0)));
				#endif
				//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
				switch(address&0xff){
					case(0xc4):
						//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 16);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite16(gba_inst,0,value);
						
					break;
					
					case(0xc6):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 16);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite16(gba_inst,1,value);
						
					break;
					
					case(0xc8):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 16);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite16(gba_inst,2,value);
					break;
				}
				
			}
			#ifdef DEBUGEMU
			else {
				iprintf("[writehword]GPIO cmd is asserted low [%x] \n ",(u8)(gpioread8(gba_inst,0)));
			}
			#endif

		}
		
	break;
	case 13:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, (u8)value); //save not yet
			break;
		}
    break;
	//goto unwritable;
	
	case 14: default:
		#ifdef DEBUGEMU
			iprintf("writehword: default write!");
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
			iprintf("writeword: writing to gba.workram");
		#endif
		
		//WRITE32LE(((u32 *)&gba->workRAM[address & 0x3FFFC]), value);
		u32store((u32)gba.workram + (address & 0x3FFFC) ,value);
	break;
	case 0x03:
		#ifdef DEBUGEMU
			iprintf("writeword: writing to gba.intram: (%x)<-[%x]",(unsigned int)((u32)(u8*)gba.intram+(address & 0x7ffC)),(unsigned int)value);
		#endif
		
		//WRITE32LE(((u32 *)&gba->internalRAM[address & 0x7ffC]), value);
		u32store((u32)gba.intram + (address & 0x7ffC),value);
	break;
	case 0x04:
		if(address < 0x4000400) {
			//CPUUpdateRegister(gba, (address & 0x3FC), value & 0xFFFF);
			cpu_updateregisters( (address & 0x3FC) , (value & 0xFFFF));
			
			//CPUUpdateRegister(gba, (address & 0x3FC) + 2, (value >> 16));
			cpu_updateregisters( ((address & 0x3FC) + 2), (value >> 16));
			
			#ifdef DEBUGEMU
				iprintf("writeword: updating IO MAP");
			#endif
		}
		else {
			virtwrite_word(address,value); //goto unwritable;
		}
	break;
	case 0x05:
		#ifdef DEBUGEMU
			iprintf("writeword: writing to gba.palram");
		#endif
		//WRITE32LE(((u32 *)&gba->paletteRAM[address & 0x3FC]), value);
		u32store((u32)gba.palram + (address & 0x3FC) ,value);
	break;
	case 0x06:
		address = (address & 0x1fffc);
			if (((gba.DISPCNT & 7) >2) && ((address & 0x1C000) == 0x18000))
				return 0;
			if ((address & 0x18000) == 0x18000)
				address &= 0x17fff;
		
		#ifdef DEBUGEMU
			iprintf("writeword: writing to gba.vidram");
		#endif
		//WRITE32LE(((u32 *)&gba->vram[address]), value);
		u32store((u32)gba.vidram + (address&0xffffffff) ,value);
	break;
	case 0x07:
		#ifdef DEBUGEMU
			iprintf("writeword: writing to gba.oam");
		#endif
		
		//WRITE32LE(((u32 *)&gba->oam[address & 0x3fc]), value);
		u32store((u32)gba.oam + (address & 0x3fc) ,value);
	break;
	case 8:
	case 9:{
		if(address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8) {
			
			//check IO port...:
			//emulate! 80000C8h - I/O Port Control (selectable W or R/W)
			//is readable GPIO? 
			//[2](1 yes | 0 no or reads whatever is at GBAROM area there)
			//[0] (1 high- can be accesed | 2 low - cannot be accessed)
			
			struct GBASystem * gba_inst = &gba;
			
			//assert signal HIGH
			gpiowrite8(gba_inst,0,0x1);
			
			if((gpioread8(gba_inst,0)&1)==1){
				#ifdef DEBUGEMU
					iprintf("[writeword]GPIO cmd is asserted high [%x] \n ",(u8)(gpioread8(gba_inst,0)));
				#endif
				//do proper GPIO/RTC/whatever cmd is bit0-3  Data Bits 0..3
				switch(address&0xff){
					case(0xc4):
						//RTC debug: addr , GPIO command, read=0/write=1 ,  8bit - 16bit - 32 bit)
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 32);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite32(gba_inst,0,value);
						
					break;
					
					case(0xc6):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 32);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite32(gba_inst,1,value);
						
					break;
					
					case(0xc8):
						#ifdef DEBUGEMU
							debugrtc_gbavirt(gba_inst,address, value, 1, 32);
						#endif
						
						process_iocmd(gba_inst, (u8)(gpioread8(gba_inst,0)&0xe));
						gpiowrite32(gba_inst,2,value);
					break;
				}
				
			}
			#ifdef DEBUGEMU
			else {
				iprintf("[writeword]GPIO cmd is asserted low [%x] \n ",(u8)(gpioread8(gba_inst,0)));
			}
			#endif
			return 0; //end write event
		}
	break;
	}
	case 0x0D:
		if(gba.cpueepromenabled) {
			//eepromWrite(address, value); //saving not yet
			#ifdef DEBUGEMU
				iprintf("writeword: writing to eeprom");
			#endif
			break;
		}
   break;
   //goto unwritable;
	
	// default
	case 0x0E: default:{
		#ifdef DEBUGEMU
			iprintf("writeword: address[%x]:(%x) fallback to default",(unsigned int) (address+0x4),(unsigned int)value);
		#endif
		
		virtwrite_word(address,value);
		break;
	}
}

return 0;	
}

////////////////////////////////VBA CPU read mechanism end////////////////////////////////////////// 

//(CPUinterrupts)
//software interrupts service (GBA BIOS calls!)
u32 __attribute__ ((hot)) swi_virt(u32 swinum){
#ifdef DEBUGEMU
debuggeroutput();
#endif

switch(swinum){
	case(0x0):
		//swi #0! : SoftReset
		#ifdef DEBUGEMU
		iprintf("bios_softreset()!");
		#endif
		bios_cpureset();
	break;
	
	case(0x1):
		//swi #1! BIOS_RegisterRamReset
		#ifdef DEBUGEMU
		iprintf("bios_registerramreset()!");
		#endif
		bios_registerramreset(0xFFFF);
	break;
	
	case(0x2):
		//swi #2! Halt
		#ifdef DEBUGEMU
		iprintf("bios_halt()!");
		#endif
		bios_cpuhalt();
	break;
	
	case(0x3):
		//swi #3! Stop/Sleep
		#ifdef DEBUGEMU
		iprintf("bios_stop/sleep()!");
		#endif
		bios_stopsleep();
	break;
	
	case(0x4):
		//swi #4!
	break;
	
	case(0x5):
		//swi #5!
	break;
	
	case(0x6):
		//swi #6! Div
		#ifdef DEBUGEMU
		iprintf("bios_div()!");
		#endif
		bios_div();
	break;
	
	case(0x7):
		//swi #7!
		#ifdef DEBUGEMU
		iprintf("bios_divarm()!");
		#endif
		bios_divarm();
	break;
	
	case(0x8):
		//swi #8!
		#ifdef DEBUGEMU
		iprintf("bios_sqrt()!");
		#endif
		bios_sqrt();
	break;
	
	case(0x9):
		//swi #9!
		#ifdef DEBUGEMU
		iprintf("bios_arctan()!");
		#endif
		bios_arctan();
	break;
		
	case(0xa):
		#ifdef DEBUGEMU
		iprintf("bios_arctan2()!");
		#endif
		bios_arctan2();
	break;
	
	case(0xb):
		#ifdef DEBUGEMU
		iprintf("bios_cpuset()!");
		#endif
		bios_cpuset();
	break;
	
	case(0xc):
		#ifdef DEBUGEMU
		iprintf("bios_cpuset()!");
		#endif
		bios_cpufastset();
	break;
	
	case(0xd):
		#ifdef DEBUGEMU
		iprintf("bios_getbioschecksum()!");
		#endif
		bios_getbioschecksum();
	break;
	
	case(0xe):
		#ifdef DEBUGEMU
		iprintf("bios_bgaffineset()!");
		#endif
		bios_bgaffineset();
	break;
	
	case(0xf):
		#ifdef DEBUGEMU
		iprintf("bios_objaffineset()!");
		#endif
		bios_objaffineset();
	break;
	
	//swi #10! bit unpack
	case(0x10):
		#ifdef DEBUGEMU
		iprintf("bios_bitunpack()!");
		#endif
		bios_bitunpack();
	break;
	
	case(0x11):
		#ifdef DEBUGEMU
		iprintf("bios_lz77uncompwram()!");
		#endif
		bios_lz77uncompwram();
	break;

	case(0x12):
		#ifdef DEBUGEMU
		iprintf("bios_lz77uncompvram()!");
		#endif
		bios_lz77uncompvram();
	break;
	
	case(0x13):
		#ifdef DEBUGEMU
		iprintf("bios_huffuncomp()!");
		#endif
		bios_huffuncomp();
	break;
	
	case(0x14):
		#ifdef DEBUGEMU
		iprintf("bios_rluncompwram()!");
		#endif
		bios_rluncompwram();
	break;
	
	case(0x15):
		#ifdef DEBUGEMU
		iprintf("bios_rluncompvram()!");
		#endif
		bios_rluncompvram();
	break;
	
	case(0x16):
		#ifdef DEBUGEMU
		iprintf("bios_diff8bitunfilterwram()!");
		#endif
		bios_diff8bitunfilterwram();
	break;
	
	case(0x17):
		#ifdef DEBUGEMU
		iprintf("bios_diff8bitunfiltervram()!");
		#endif
		bios_diff8bitunfiltervram();
	break;
	
	case(0x18):
		#ifdef DEBUGEMU
		iprintf("bios_diff16bitunfilter()!");
		#endif
		bios_diff16bitunfilter();
	break;
	
	case(0x1f):
		#ifdef DEBUGEMU
		iprintf("bios_midikey2freq()!");
		#endif
		bios_midikey2freq();
	break;
	
	case(0x2a):
		#ifdef DEBUGEMU
		iprintf("bios_snddriverjmptablecopy()!");
		#endif
		bios_snddriverjmptablecopy();
	break;
	
	default:
	break;
}
return 0;
}

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

//current CPU mode working set of registers
u32  __attribute__((section(".dtcm"))) gbavirtreg_cpu[0x10]; //placeholder for actual CPU mode registers

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

//and FIQ(32) which is r8-r12
u32  __attribute__((section(".dtcm"))) gbavirtreg_fiq[0x5];

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
		//iprintf("set vb irq");
	}
	//enable HBLANK IRQ
	else if (irqtoset == (1<<1)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<4));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<4);
		//iprintf("set hb irq");
	}
	//enable VCOUNTER IRQ
	else if (irqtoset == (1<<2)){
		//stru32inlasm(0x04000004,0x0, *(u32*)(0x04000004) | (1<<5));
		*(u32*)(0x04000004)=(*(u32*)(0x04000004)) | (1<<5);
		//iprintf("set vcnt irq");
	}
	
return 0;
}


//IO opcodes

//debug!
u32 debuggeroutput(){
u32 lnk_ptr;
		__asm__ volatile (
		"mov %[lnk_ptr],#0;" "\n\t"
		"add %[lnk_ptr],%[lnk_ptr], lr" "\n\t"//"sub lr, lr, #8;" "\n\t"
		"sub %[lnk_ptr],%[lnk_ptr],#0x8" 
		:[lnk_ptr] "=r" (lnk_ptr)
		);
iprintf("\n LR callback trace at %x \n", (unsigned int)lnk_ptr);
return 0;
}

//src_address / addrpatches[] (blacklisted) / addrfixes[] (patched ret)
u32 __attribute__ ((hot)) addresslookup(u32 srcaddr, u32 * blacklist, u32 * whitelist){

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
u32 armfetchpc_arm(u32 address){
return cpuread_word(address);
}

u16 armfetchpc_thumb(u32 address){
return cpuread_hword(address);
}

//prefetch next opcode
u32 armnextpc(u32 address){
//0:ARM | 1:THUMB
if(armstate==0)
	return cpuread_word(address+0x4);
//else
return cpuread_hword(address+0x2);
}

//readjoypad
u32 systemreadjoypad(int which){
	//scanKeys();
	u32 res = keysCurrent(); //libnds getkeys
		// disallow L+R or U+D of being pressed at the same time
		if((res & 48) == 48)
		res &= ~16;
		if((res & 192) == 192)
		res &= ~128;
	return res;
}
