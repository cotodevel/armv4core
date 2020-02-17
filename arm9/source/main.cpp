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
#include "buffer.h"
#include "fileBrowse.h"
#include "dswnifi_lib.h"
#include "ipcfifoTGDSUser.h"

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

char curChosenBrowseFile[MAX_TGDSFILENAME_LENGTH+1];
char biospath[MAX_TGDSFILENAME_LENGTH+1];
char savepath[MAX_TGDSFILENAME_LENGTH+1];
char patchpath[MAX_TGDSFILENAME_LENGTH+1];

/* THUMB DISASSEMBLER (little endian) */ //format { 0xc5, 0xc0-- };
unsigned char buf[1*2]; //buffer for 16 thumb instructions
struct DInst insts[10] = {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}}; //_DInst insts[10] = {0};
struct DecomposeInfo info = {0}; //_DecomposeInfo info = {0};
struct TInst tinst = {0}; //TInst t = {0};
/* END THUMB DISASSEMBLER */

int toggle=0; //toggler for browsefile() IRQ vsync-able
bool dldiload = false;
bool dirloaded;

//ticker for emulator loop
u32 cpucore_tick=0;

DIR *pdir;
struct dirent *pent;

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

static inline void menuShow(){
	clrscr();
	printf("     ");
	printf("     ");
	
	printf("ARMV4Core: ARM7TDMI Emulator ");
	printf("(Select): Run CPU ");
	printf("(Start): Clear screen. ");
	printf("(A): CPU Info. ");
	printf("Available heap memory: %d", getMaxRam());
}

u32 emulatorgba(){
	//1) read GBAROM entrypoint
	//2) reserve registers r0-r15, stack pointer , LR, PC and stack (for USR, AND SYS MODES)
	//3) get pointers from all reserved memory areas (allocated)
	//4) use this function to fetch addresses from GBAROM, patch swi calls (own BIOS calls), patch interrupts (by calling correct vblank draw, sound)
	//patch IO access , REDIRECT VIDEO WRITES TO ALLOCATED VRAM & VRAMIO [use switch and intercalls for asm]

	//btw entrypoint is always ARM code 

	if(cpuStart == true){
		
		u32 new_instr=armfetchpc((uint32)exRegs[0xf]&0xfffffffe);
		#ifdef DEBUGEMU
			printf("/*****************/");
			printf(" rom:%x [%x]",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)new_instr);
		#endif
		
		//CPUfetch depending on CPUmode
		(armstate==0)?disarmcode(new_instr):disthumbcode(new_instr);	

		//refresh vcount & disptat here before cpuloop
		lcdTicks = ((*(u32*)0x04000006) &0xfff); //use vcounter for tick generation 

		cpuloop(cpucore_tick);	//1 list per hblank
		cpucore_tick++;
		if(cpucore_tick>10001) 
			cpucore_tick=0;
	}
	else
		cpuStart = true;

	//increase PC depending on CPUmode
	(armstate==0)?exRegs[0xf]+=4:exRegs[0xf]+=2;
	return 0;
}

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
	switch_dswnifi_mode(dswifi_idlemode);
	/*			TGDS 1.5 Standard ARM9 Init code end	*/
	
	printf("Available heap memory: %d", getMaxRam());
	
	//render TGDSLogo from a LZSS compressed file
	RenderTGDSLogoSubEngine((uint8*)&TGDSLogoLZSSCompressed[0], TGDSLogoLZSSCompressed_size);
	
	biospath[0] = 0;
	savepath[0] = 0;
	patchpath[0] = 0;
	
	// GBA EMU INIT
	DisableIrq(IRQ_TIMER3);
	
	#ifndef ROMTEST
	char startPath[MAX_TGDSFILENAME_LENGTH+1];
	strcpy(startPath,"/");
	while( ShowBrowser((char *)startPath, (char *)curChosenBrowseFile) == true ){	//as long you keep using directories ShowBrowser will be true
		//navigating DIRs here...
	}
	#endif
	
	//show gbadata
	//printgbainfo (curChosenBrowseFile);

	//debugging is enabled at startup
	isdebugemu_defined();

	/******************************************************** GBA EMU INIT CODE *********************************************************************/
	bool extram = false; //enabled for dsi
	bool failed = !CPULoadRom(curChosenBrowseFile, extram);
	if(failed)
	{
		printf("CPULoadRom: failed");
		while(1);
	}
	printf("OK");
	
	useBios = false;
	CPUInit(biospath, useBios,false);

	bios_cpureset();

	bios_registerramreset(0xFF);

	//execute_arm7_command(0xc0700100,0x1FFFFFFF,gba_frame_line);

	//IEBACKUP = 0;

	//old entrypoint: gba map cant reach this ... so
	//exRegs[0xf] = (u32)&gba_setup;
	
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

	//printf("payload 1st op:%x / payload size: %d ",(PATCH_BOOTCODE_PTR[0]),PATCH_BOOTCODE_SIZE);
	nds_2_gba_copy((u8*)&somebuf[0], gba_stack_src, PATCH_BOOTCODE_SIZE*4);

	//printf("gba read @ %x:%x ",(u32)(u8*)gba_src,CPUReadMemory((u32)(u8*)gba_src));
	printf("nds payload set correctly! payload size: %d",(int)PATCH_BOOTCODE_SIZE*4);
	
	//int size_dumped = ram2file_nds((const char*)"fat:/armv4dump.bin",(u8*)&puzzle_original[0],puzzle_original_size);

	//re enable when opcodes are implemented
	#else
		//jump to homebrew rom : But rom is mapped to 0x08000000 because branch relative precalculated offsets are from 0x08000000 base.
		
		//so you cant recompile payloads that use branch relocated addresses from a map different than 0x08000000 (if you plan to stream them from that map)
		//so: rom is set at 0x08000000 and streamed instead ROP approach
		
		//nds_2_gba_copy(rom,gba_src,puzzle_original_size);
		//printf("nds gba homebrew set correctly @ %x! payload size: %d",gba_src,puzzle_original_size);
		
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
	
	clrscr();
	printf(" - ");
	printf(" - ");
	printf(" - ");
	printf(" - ");
	
	printf("R15: %x", exRegs[15]);
	while(1==1){}
	
	while (1){
		scanKeys();
		if(keysPressed() & KEY_A){
			clrscr();
			printf("     ");
			printGBACPU();
			scanKeys();
			while(keysPressed() & KEY_A){
				scanKeys();
			}
		}
		
		if (keysPressed() & KEY_START){
			clrscr();
			printf("     ");
		}
			
		if(keysPressed() & KEY_SELECT){
			emulatorgba();
		}
		
		handleARM9SVC();	/* Do not remove, handles TGDS services */
		IRQVBlankWait();
	}
	
	return 0;
}
