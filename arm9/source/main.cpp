/*

			Copyright (C) 2017  Coto
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
USA

*/

#include "main.h"
#include "translator.h"
#include "opcode.h"
#include "keypadTGDS.h"
#include "biosTGDS.h"
#include "dldi.h"

//disassembler (thumb)

#include "armstorm/arm.h" //THUMB DISASSEMBLER
#include "armstorm/armstorm.h" //THUMB DISASSEMBLER
#include "armstorm/common.h" //THUMB DISASSEMBLER
#include "armstorm/thumb.h" //THUMB DISASSEMBLER
#include "armstorm/thumb_db.h" //THUMB DISASSEMBLER

//PU and stack managmt
#include "pu/pu.h"
#include "pu/supervisor.h"
#include "gba.arm.core.h"

#include "bios.h"

#include "TGDSLogoLZSSCompressed.h"
#include "global_settings.h"

/* THUMB DISASSEMBLER (little endian) */ //format { 0xc5, 0xc0-- };
unsigned char buf[1*2]; //buffer for 16 thumb instructions
struct DInst insts[10] = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}}; //_DInst insts[10] = {0};
struct DecomposeInfo info = {0}; //_DecomposeInfo info = {0};
struct TInst tinst = {0}; //TInst t = {0};
/* END THUMB DISASSEMBLER */

GBASystem gba; //this creates a proper GBASystem pointer to struct (stack) - across C++ from struct GBASystem (C)
//gbaHeader_t *gbaGamehdr;

extern struct gbaheader_t gbaheader;

//0x03800000 - 0x038082xx = 0x82B4 (33xxx bytesld) - 0x038082B5 - 0x03810000 = 0x7D4C free 
//note:

//  380FFDCh  ..  NDS7 Debug Stacktop / Debug Vector (0=None)
//  380FFF8h  4   NDS7 IRQ Check Bits (hardcoded RAM address)
//  380FFFCh  4   NDS7 IRQ Handler (hardcoded RAM address)

//linker (bin) objects (main.h)

//vector for dirlibs
int toggle=0; //toggler for browsefile() IRQ vsync-able

bool dldiload = false;
bool dirloaded;

//ticker for emulator loop
u32 cpucore_tick=0;

//fifo
extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

typedef struct{
	u32 Version;
	u32 listentr;
} __attribute__ ((__packed__)) patch_t;
patch_t patchheader;

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

DIR *pdir;
struct dirent *pent;

/*
inline __attribute__((always_inline))
int wramtst(int wram,int top){
extern int temp,temp2;
temp=wramtst1(wram,top);
if(temp==alignw(top))
	printf("ARM9 test1/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
else 
	printf("ARM9RAM tst 1/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
temp2=wramtst2(wram,top);
if(temp2==alignw(top))
	printf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
else if ((temp2-2)==alignw(top))
	printf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n !!aligned read!!",temp2,EWRAM,temp2);
else 
	printf("ARM9RAM tst 2/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
return 0;
}
*/

int u32count_values(u32 u32word){
int i=0,cntr=0;

while(cntr<32){
	if (((u32word>>cntr)&0xf)>0){
		i++;
	}
	cntr+=4;
}
return i;
}


//psg noise test int temp5=0;

//Timers
//(data counter)
//4000100h - TM0CNT_L - Timer 0 Counter/Reload (R/W)
//4000104h - TM1CNT_L - Timer 1 Counter/Reload (R/W)
//4000108h - TM2CNT_L - Timer 2 Counter/Reload (R/W)
//400010Ch - TM3CNT_L - Timer 3 Counter/Reload (R/W)

//Writing to these registers initializes the <reload> value (but does not directly affect the current counter value). 
//Reading returns the current <counter> value (or the recent/frozen counter value if the timer has been stopped).
//The reload value is copied into the counter only upon following two situations: Automatically upon timer overflows, 
//or when the timer start bit becomes changed from 0 to 1.
//Note: When simultaneously changing the start bit from 0 to 1, and setting the reload value at the same time (by a single 32bit I/O operation), 
//then the newly written reload value is recognized as new counter value.

//(controller)
//4000102h - TM0CNT_H - Timer 0 Control (R/W)
//4000106h - TM1CNT_H - Timer 1 Control (R/W)
//400010Ah - TM2CNT_H - Timer 2 Control (R/W)
//400010Eh - TM3CNT_H - Timer 3 Control (R/W)
//  Bit   Expl.
//  0-1   Prescaler Selection (0=F/1, 1=F/64, 2=F/256, 3=F/1024)
//  2     Count-up Timing   (0=Normal, 1=See below)
//  3-5   Not used
//  6     Timer IRQ Enable  (0=Disable, 1=IRQ on Timer overflow)
//  7     Timer Start/Stop  (0=Stop, 1=Operate)
//  8-15  Not used
//When Count-up Timing is enabled, the prescaler value is ignored, instead the time is incremented each time when the previous counter overflows. This function cannot be used for Timer 0 (as it is the first timer).
//F = System Clock (16.78MHz).


//timer 0,1,2,3 (write only) TVAL
u16 TMXCNT_LW(u8 TMXCNT,int TVAL){
		*(u32*)(0x04000100+(TMXCNT*4))=TVAL;
	return 0;
}

//timer 0,1,2,3 (read only) CUR_TVAL
u16 TMXCNT_LR(u8 TMXCNT){
	return (*(u16*)(0x04000100+(TMXCNT*4)));
}

//timer 0,1,2,3 controller
u16 TMXCNT_HW(u8 TMXCNT, u8 prescaler,u8 countup,u8 status){
		*(u32*)(0x04000100+(TMXCNT*4+2))=(prescaler<<0)|(countup<<2)|(status<<7);
	return 0;
}

u32 emulatorgba(){

//1) read GBAROM entrypoint
//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
//3) get pointers from all reserved memory areas (allocated)
//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]

//btw entrypoint is always ARM code 

if(gba.cpustate==true){
	
	u32 new_instr=armfetchpc((uint32)rom);
	#ifdef DEBUGEMU
		printf("/*****************/");
		printf("\n rom:%x [%x]\n",(unsigned int)rom,(unsigned int)new_instr);
	#endif
	
	//CPUfetch depending on CPUmode
	(armstate==0)?disarmcode(new_instr):disthumbcode(new_instr);	

	//refresh vcount & disptat here before cpuloop
	gba.lcdticks=((*(u32*)0x04000006) &0xfff); //use vcounter for generation ticks

	cpuloop(cpucore_tick);	//1 list per hblank / threads from gba.lcdticks into IF
	cpucore_tick++;
	if(cpucore_tick>10001) 
		cpucore_tick=0;
}
else
	gba.cpustate=true;

//read input is done already -> gba.GBAP1

//increase PC depending on CPUmode
(armstate==0)?rom+=4:rom+=2;

//before anything, interrupts (GBA generated) are checked on NDS9 IRQ.s PU.C exceptirq()

//old dcache is discarded
//DC_InvalidateAll(); 
//DC_FlushAll();

return 0;
}

char temppath[255 * 2];
char biospath[255 * 2];
char savepath[255 * 2];
char patchpath[255 * 2];

int main(int _argc, sint8 **_argv) {

	/*			TGDS 1.5 Standard ARM9 Init code start	*/
	bool project_specific_console = true;	//set default console or custom console: custom console
	GUI_init(project_specific_console);
	GUI_clear();

	sint32 fwlanguage = (sint32)getLanguage();
	
	clrscr();
	printf("     ");
	printf("     ");
	
	#ifdef ARM7_DLDI
	setDLDIARM7Address((u32 *)TGDSDLDI_ARM7_ADDRESS);	//Required by ARM7DLDI!
	#endif
	
	int ret=FS_init();
	if (ret == 0)
	{
		printf("FS Init ok.");
	}
	else if(ret == -1)
	{
		printf("FS Init error.");
	}
	//switch_dswnifi_mode(dswifi_idlemode);	//causes freezes.
	/*			TGDS 1.5 Standard ARM9 Init code end	*/
	
	printf("Available heap memory: %d", getMaxRam());
	
	//render TGDSLogo from a LZSS compressed file
	RenderTGDSLogoSubEngine((uint8*)&TGDSLogoLZSSCompressed[0], TGDSLogoLZSSCompressed_size);
	
	biospath[0] = 0;
	savepath[0] = 0;
	patchpath[0] = 0;

	// GBA EMU INIT//
	//show gbadata printf("\x1b[21;1H
	strcat(temppath,(char*)"/gba/rs-pzs.gba");
	//printgbainfo (temppath);

	//debugging is enabled at startup
	isdebugemu_defined();

	/******************************************************** GBA EMU INIT CODE *********************************************************************/
	bool extram = false; //enabled for dsi

	printf("CPULoadRom...");
	bool failed = !CPULoadRom(temppath,extram);
	if(failed)
	{
		printf("failed");
		while(1);
	}
	printf("OK\n");


	useBios = false;
	printf("CPUInit\n");
	CPUInit(biospath, useBios,false);

	printf("CPUReset\n");
	bios_cpureset();

	printf("BIOS_RegisterRamReset\n");
	bios_registerramreset(0xFF);

	printf("arm7init\n");
	//execute_arm7_command(0xc0700100,0x1FFFFFFF,gba_frame_line);

	printf("irqinit\n");
	//IEBACKUP = 0;

	//bios calls (flush) destroyed sp13 for usr mode
	exRegs[0xd]=gbavirtreg_r13usr[0];
	  
	//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
	updatecpuflags(1,cpsrvirt,0x10);


	//old entrypoint: gba map cant reach this ... so
	//exRegs[0xf] = (u32)&gba_setup;

	//new entrypoint: execute ROP only if we use real GBA ROM
	gba_entrypoint = (u32)0x08000000;

	//re enable when opcodes are implemented
	#ifndef ROMTEST
	u8* gba_stack_src =(u8*)0x03000000;

	//new
	u32 * PATCH_BOOTCODE_PTR = (u32*)&gba_setup;
	//required if buffer from within a destructable function
	static u8 somebuf[32*1024];
	u32 rom_pool_end = 0x03007f00;  //the last ARM disassembled opcode (top) from our function.
	int PATCH_BOOTCODE_SIZE = 0;
	PATCH_BOOTCODE_SIZE = extract_word(PATCH_BOOTCODE_PTR,(PATCH_BOOTCODE_PTR[0]),(u32*)&somebuf[0],rom_pool_end,32);

	//printf("payload 1st op:%x / payload size: %d \n",(PATCH_BOOTCODE_PTR[0]),PATCH_BOOTCODE_SIZE);
	nds_2_gba_copy((u8*)&somebuf[0],gba_stack_src,PATCH_BOOTCODE_SIZE*4);

	//printf("gba read @ %x:%x \n",(u32)(u8*)gba_src,CPUReadMemory((u32)(u8*)gba_src));
	printf("nds payload set correctly! payload size: %d\n",(int)PATCH_BOOTCODE_SIZE*4);

	exRegs[0xe]=(u32)gba_entrypoint;
	exRegs[0xf]=(u32)(u8*)gba_stack_src;

	//int size_dumped = ram2file_nds((const char*)"fat:/armv4dump.bin",(u8*)&puzzle_original[0],puzzle_original_size);

	//re enable when opcodes are implemented
	#else
		//jump to homebrew rom : But rom is mapped to 0x08000000 because branch relative precalculated offsets are from 0x08000000 base.
		
		//so you cant recompile payloads that use branch relocated addresses from a map different than 0x08000000 (if you plan to stream them from that map)
		//so: rom is set at 0x08000000 and streamed instead ROP approach
		
		//nds_2_gba_copy(rom,gba_src,puzzle_original_size);
		//printf("nds gba homebrew set correctly @ %x! payload size: %d\n",gba_src,puzzle_original_size);

		exRegs[0xe]=(u32)gba_entrypoint;
		exRegs[0xf]=(u32)gba_entrypoint;
	//re enable when opcodes are implemented
	#endif

	#ifdef SPINLOCK_CODE
	//spinlock code

	//spinlock_createproc(u8 process_id,u8 status,u32cback_ptr new_callback);
	spinlock_createproc(0,0,(u32cback_ptr) vblank_thread);
	spinlock_createproc(1,1,(u32cback_ptr) 0);
	spinlock_createproc(2,1,(u32cback_ptr) 0);
	spinlock_createproc(3,1,(u32cback_ptr) 0);
	spinlock_createproc(4,1,(u32cback_ptr) 0);
	spinlock_createproc(5,1,(u32cback_ptr) 0);
	spinlock_createproc(6,1,(u32cback_ptr) 0);
	spinlock_createproc(7,1,(u32cback_ptr) 0);
	spinlock_createproc(8,1,(u32cback_ptr) 0);
	spinlock_createproc(9,1,(u32cback_ptr) 0);

	#endif

	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();
	nopinlasm();

	while (1)
	{
		scanKeys();
		if (keysPressed() & KEY_A){
			printf("test:%d",rand()&0xff);
		}
		
		if (keysPressed() & KEY_B){
			GUI_clear();
		}
		
		handleARM9SVC();	/* Do not remove, handles TGDS services */
		IRQVBlankWait();
	}
	
	return 0;
}
