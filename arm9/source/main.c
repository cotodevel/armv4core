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
#include "keypadTGDS.h"
#include "biosTGDS.h"
#include "dldi.h"
#include "Util.h"
#include "fileBrowse.h"
#include "dswnifi_lib.h"
#include "ipcfifoTGDSUser.h"
#include "posixHandleTGDS.h"
#include "TGDSMemoryAllocator.h"
#include "spitscTGDS.h"
#include "fatfslayerTGDS.h"
#include "utilsTGDS.h"
#include "loader.h"

//disassembler (thumb)
#include "armstorm/arm.h" //THUMB DISASSEMBLER
#include "armstorm/armstorm.h" //THUMB DISASSEMBLER
#include "armstorm/common.h" //THUMB DISASSEMBLER
#include "armstorm/thumb.h" //THUMB DISASSEMBLER
#include "armstorm/thumb_db.h" //THUMB DISASSEMBLER
#include "gba.arm.core.h"
#include "bios.h"
#include "TGDSLogoLZSSCompressed.h"
#include "global_settings.h"

//ARM7 VRAM core
#include "arm7vram.h"
#include "arm7vram_twl.h"

u32 * getTGDSMBV3ARM7Bootloader(){
	if(__dsimode == false){
		return (u32*)&arm7vram[0];	
	}
	else{
		return (u32*)&arm7vram_twl[0];
	}
}

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

int internalCodecType = SRC_NONE;//Internal because WAV raw decompressed buffers are used if Uncompressed WAV or ADPCM
bool stopSoundStreamUser(){
	
}

void closeSoundUser(){
	//Stubbed. Gets called when closing an audiostream of a custom audio decoder
}

#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("Os")))
#endif
#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
int main(int argc, char **argv) {
	/*			TGDS 1.6 Standard ARM9 Init code start	*/
	//Save Stage 1: IWRAM ARM7 payload: NTR/TWL (0x03800000)
	memcpy((void *)TGDS_MB_V3_ARM7_STAGE1_ADDR, (const void *)0x02380000, (int)(96*1024));	//
	coherent_user_range_by_size((uint32)TGDS_MB_V3_ARM7_STAGE1_ADDR, (int)(96*1024)); //		also for TWL binaries 
	
	//Execute Stage 2: VRAM ARM7 payload: NTR/TWL (0x06000000)
	u32 * payload = NULL;
	if(__dsimode == false){
		payload = (u32*)&arm7vram[0];	
	}
	else{
		payload = (u32*)&arm7vram_twl[0];
	}
	executeARM7Payload((u32)0x02380000, 96*1024, payload);
	
	bool isTGDSCustomConsole = false;	//set default console or custom console: default console
	GUI_init(isTGDSCustomConsole);
	GUI_clear();

	bool isCustomTGDSMalloc = true;
	setTGDSMemoryAllocator(getProjectSpecificMemoryAllocatorSetup(isCustomTGDSMalloc));
	sint32 fwlanguage = (sint32)getLanguage();

	int ret=FS_init();
	if (ret == 0)
	{
		printf("FS Init ok.");
	}
	else{
		printf("FS Init error: %d", ret);
	}
	
	switch_dswnifi_mode(dswifi_idlemode);
	asm("mcr	p15, 0, r0, c7, c10, 4");
	flush_icache_all();
	flush_dcache_all();	
	/*			TGDS 1.6 Standard ARM9 Init code end	*/
	
	//Show logo
	RenderTGDSLogoMainEngine((uint8*)&TGDSLogoLZSSCompressed[0], TGDSLogoLZSSCompressed_size);
	
	/////////////////////////////////////////////////////////Reload TGDS Proj///////////////////////////////////////////////////////////
	char tmpName[256];
	char ext[256];
	if(__dsimode == true){
		char TGDSProj[256];
		char curChosenBrowseFile[256];
		strcpy(TGDSProj,"0:/");
		strcat(TGDSProj, "ToolchainGenericDS-multiboot");
		if(__dsimode == true){
			strcat(TGDSProj, ".srl");
		}
		else{
			strcat(TGDSProj, ".nds");
		}
		//Force ARM7 reload once 
		if( 
			(argc < 2) 
			&& 
			(strncmp(argv[1], TGDSProj, strlen(TGDSProj)) != 0) 	
		){
			REG_IME = 0;
			MPUSet();
			REG_IME = 1;
			char startPath[MAX_TGDSFILENAME_LENGTH+1];
			strcpy(startPath,"/");
			strcpy(curChosenBrowseFile, TGDSProj);
			
			char thisTGDSProject[MAX_TGDSFILENAME_LENGTH+1];
			strcpy(thisTGDSProject, "0:/");
			strcat(thisTGDSProject, TGDSPROJECTNAME);
			if(__dsimode == true){
				strcat(thisTGDSProject, ".srl");
			}
			else{
				strcat(thisTGDSProject, ".nds");
			}
			
			//Boot .NDS file! (homebrew only)
			strcpy(tmpName, curChosenBrowseFile);
			separateExtension(tmpName, ext);
			strlwr(ext);
			
			//pass incoming launcher's ARGV0
			char arg0[256];
			int newArgc = 2;
			if (argc > 2) {
				//Arg0:	Chainload caller: TGDS-MB
				//Arg1:	This NDS Binary reloaded through ChainLoad
				//Arg2: This NDS Binary reloaded through ChainLoad's Argument0
				strcpy(arg0, (const char *)argv[2]);
				newArgc++;
			}
			
			char thisArgv[3][MAX_TGDSFILENAME_LENGTH];
			memset(thisArgv, 0, sizeof(thisArgv));
			strcpy(&thisArgv[0][0], curChosenBrowseFile);	//Arg0:	Chainload caller: TGDS-MB
			strcpy(&thisArgv[1][0], thisTGDSProject);	//Arg1:	NDS Binary reloaded through ChainLoad
			strcpy(&thisArgv[2][0], (char*)arg0);	//Arg2: NDS Binary reloaded through ChainLoad's ARG0
			u32 * payload = getTGDSMBV3ARM7Bootloader();		
			if(TGDSMultibootRunNDSPayload(curChosenBrowseFile, (u8*)payload, newArgc, (char*)&thisArgv) == false){ //should never reach here, nor even return true. Should fail it returns false
				
			}
		}
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	REG_IME = 0;
	set0xFFFF0000FastMPUSettings();
	//TGDS-Projects -> legacy NTR TSC compatibility
	if(__dsimode == true){
		TWLSetTouchscreenTWLMode();
	}
	REG_IME = 1;
	
	setupDisabledExceptionHandler();
	
	biospath[0] = 0;
	savepath[0] = 0;
	patchpath[0] = 0;
	
	// GBA EMU INIT
	irqDisable(IRQ_TIMER3);
	
	#ifndef ROMTEST
	char startPath[MAX_TGDSFILENAME_LENGTH+1];
	strcpy(startPath,"/");
	while( ShowBrowser((char *)startPath, (char *)curChosenBrowseFile) == true ){	//as long you keep using directories ShowBrowser will be true
		//navigating DIRs here...
	}
	
	//Boot .NDS file! (homebrew only)
	strcpy(tmpName, curChosenBrowseFile);
	separateExtension(tmpName, ext);
	strlwr(ext);
	if(strncmp(ext,".nds", 4) == 0){
		char thisArgv[3][MAX_TGDSFILENAME_LENGTH];
		memset(thisArgv, 0, sizeof(thisArgv));
		strcpy(&thisArgv[0][0], TGDSPROJECTNAME);	//Arg0:	This Binary loaded
		strcpy(&thisArgv[1][0], curChosenBrowseFile);	//Arg1:	NDS Binary reloaded
		strcpy(&thisArgv[2][0], "");					//Arg2: NDS Binary ARG0
		u32 * payload = getTGDSMBV3ARM7Bootloader();
		TGDSMultibootRunNDSPayload(curChosenBrowseFile, (u8*)payload, 3, (char*)&thisArgv);
	}
	#endif
	
	//show gbadata
	//printgbainfo (curChosenBrowseFile);

	//debugging is enabled at startup
	isdebugemu_defined();

	/******************************************************** GBA EMU INIT CODE *********************************************************************/
	/*				 allocate segments
	 GBA Memory Map

	General Internal Memory
	  00000000-00003FFF   BIOS - System ROM         (16 KBytes)
	  00004000-01FFFFFF   Not used
	  02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
	  02040000-02FFFFFF   Not used
	  03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
	  03008000-03FFFFFF   Not used
	  04000000-040003FE   I/O Registers
	  04000400-04FFFFFF   Not used
	Internal Display Memory
	  05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
	  05000400-05FFFFFF   Not used
	  06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
	  06018000-06FFFFFF   Not used
	  07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
	  07000400-07FFFFFF   Not used
	External Memory (Game Pak)
	  08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
	  0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
	  0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
	  0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
	  0E010000-0FFFFFFF   Not used
	*/

	initmemory();
	bool extram = false; //enabled for dsi
	bool failed = !CPULoadRom(curChosenBrowseFile, extram);
	if(failed)
	{
		printf("CPULoadRom: failed");
		while(1);
	}
	useBios = false;
	CPUInit(biospath, useBios,false);
	bios_cpureset();
	bios_registerramreset(0xFF);
	
	clrscr();
	printf(" - ");
	printf(" ARMv4 Core start.");
	
	/*
	printf("R15: %x", exRegs[15]);
	while(1==1){}
	*/
	
	//old entrypoint: gba map cant reach this ... so
	//exRegs[0xf] = (u32)&gba_setup;
	
	
	#ifndef ROMTEST
	/*
	//
	//Helper to implement and test ARM/THUMB full set opcodes
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
	*/
	
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
	
	
	
	while (1){
		scanKeys();
		if(keysDown() & KEY_A){
			clrscr();
			printf("     ");
			printGBACPU();
			scanKeys();
			while(keysDown() & KEY_A){
				scanKeys();
			}
		}
		
		if (keysDown() & KEY_START){
			clrscr();
			printf("     ");
		}
			
		if(keysDown() & KEY_SELECT){
			cpu_fdexecute();
			scanKeys();
			while(keysDown() & KEY_SELECT){
				scanKeys();
			}
		}
		
		handleARM9SVC();	/* Do not remove, handles TGDS services */
		IRQVBlankWait();
	}
	
	return 0;
}
