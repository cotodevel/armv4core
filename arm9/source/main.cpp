//arm9 main libs
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>

#include <nds/bios.h>
#include <nds/system.h>

#include <nds/memory.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
//#include <dswifi9.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>
#include "stdint.h"

#ifdef __cplusplus
#include <vector>
#include <algorithm>
#endif

//arm9 video
#include <nds/arm9/math.h>
#include <nds/arm9/video.h>
#include <nds/arm9/videoGL.h>
#include <nds/arm9/trig_lut.h>
#include <nds/arm9/sassert.h>

#include ".\main.h"
#include ".\settings.h"

//zlib (gzip) and zip
#include "zlib/zlib.h"
#include "zlib/zip/unzip.h"

//CPU
#include "gbacore/opcode.h"
#include "gbacore/util.h"
#include "gbacore/bios.h"
#include "gbacore/translator.h"
#include "gbacore/gba.arm.core.h"
#include "gbacore/spinlock.h"

//filesystem
#include "disk/file_browse.h"
#include "disk/disc.h"
#include "disk/fatfile.h"
#include "disk/directory.h"
#include "disk/partition.h"
#include "disk/mem_allocate.h"
#include "disk/bit_ops.h"
#include "disk/file_allocation_table.h"
#include "disk/cache.h"
#include "disk/lock.h"
#include "disk/directory.h"
#include "disk/filetime.h"
#include "disk/fatmore.h"

//PU and stack managmt
#include "pu/pu.h"
#include "pu/supervisor.h"
#include "pu/virtualize.h"

#include "./hbmenustub/nds_loader_arm9.h"

//rom
#include "./rom/puzzle_original.h"

#include "./wifi.h"

/*
//linker (bin) objects
extern unsigned int rs_sappy_bin;
extern unsigned int rs_sappy_bin_size;
extern unsigned int battle_bin;
extern unsigned int battle_bin_size;
*/

/*
//splashscreen
#include ".\hbmenu_banner.h"

//ARM/THUMB disassembler :)
#include ".\armdis.h"

*/

//NDS9
//IPC: 04000180
//IE: 04000210
//IF: 04000214


#define CLUSTER_FREE	0x00000000
#define	CLUSTER_EOF		0x0FFFFFFF
#define CLUSTER_FIRST	0x00000002

//---------------------------------------------------------------
// FAT constants
#define CLUSTER_EOF_16	0xFFFF

#define FILE_LAST 0x00
#define FILE_FREE 0xE5

#ifndef EOF
#define EOF -1
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif

//browseforfile screen offsets
#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10
/*
	b	startUp
	
storedFileCluster:
	.word	0x0FFFFFFF		@ default BOOT.NDS
initDisc:
	.word	0x00000001		@ init the disc by default
wantToPatchDLDI:
	.word	0x00000001		@ by default patch the DLDI section of the loaded NDS
@ Used for passing arguments to the loaded app
argStart:
	.word	_end - _start
argSize:
	.word	0x00000000
dldiOffset:
	.word	_dldi_start - _start
dsiSD:
	.word	0
*/

//unzip buffer
//char * uncompr = (char*) calloc(1024*1024,sizeof(char)); //for temp decomp buffer
//uLong uncomprLen = 1024*1024;


/*jmp from inline
asm volatile(
	"\tbx %0\n"
	: : "r" (0x02FFFE04)
);


struct GSF_FILE{
	char psftag[220];//psftag[50001]; //int psftag[12500]; //minigsf 
	char libname[64];
	bool gsfloaded;
	char gsflib[20];
	char *program; //8 bytes + 1*20 bytes + 1 byte + 1*64 bytes + 1*220 bytes = 406 bytes
	char *reserved;
	
};// __attribute__((aligned (8))); //GSF_FILE GSF_STR; 
//#pragma pack (2) //2 byte alignment

//alloc'd block into NDSRAM
//(void *) calloc (size , datablock size)
struct GSF_FILE * gsfptr = (GSF_FILE*) calloc(406,1);

*/

/* ////////////////////////////////////zlib
//const char hello[] = "if you see this zlib is working nicely!!";

//const char dictionary[] = "hello";
uLong len = (uLong)strlen(hello)+1;

//static const char* myVersion = ZLIB_VERSION;

//unsigned char decompressedgsf[1024*1024]; //1MB decompr buffer

// INSIDE FUNCTION ZLIB 
if (zlibVersion()[0] != myVersion[0]){
    printf("incompatible zlib version\n");
} 
else if (strcmp(zlibVersion(), ZLIB_VERSION) != 0){
    printf("warning: different zlib version\n");
}

//(dest, destLen, source, sourceLen)
//3rd arg (const unsigned char*)file, 4th arg (unsigned int long)GSFSIZE

//-->> works for zip:static Byte *compr, *uncompr;
//-->> works for zip:uLong comprLen = (1024*1024);
//-->> works for zip:uLong uncomprLen = comprLen;

//-->> works for zip:compr    = (Byte*)calloc((uInt)comprLen, 1);
//-->> works for zip:uncompr  = (Byte*)calloc((uInt)uncomprLen, 1);


res = compress(compr,&comprLen,(const Bytef*)hello,len);
switch(res){
case(Z_OK):
printf("stream compress status: OK! (%x) BUFFER:(%x) \n ",Z_OK,(int)&compr);
break;
case(Z_STREAM_END):
printf("stream compress status: error, unexpected end! (%x)",Z_STREAM_END);
break;
case(Z_NEED_DICT):
printf("stream compress status: error, need dictionary! (%x)",Z_NEED_DICT);
break;
case(Z_ERRNO):
printf("stream compress status: error, cant open file! (%x)",Z_ERRNO);
break;
case(Z_STREAM_ERROR):
printf("stream compress status: error, corrupted source stream! (%x)",Z_STREAM_ERROR);
break;
case(Z_DATA_ERROR):
printf("stream compress status: error, data not zlib ! (%x)",Z_DATA_ERROR);
break;
case(Z_MEM_ERROR):
printf("stream compress status: error, memory source corrupted! (%x)",Z_MEM_ERROR);
break;
case(Z_BUF_ERROR):
printf("stream compress status: error, not enough room in the output buffer! (%x)",Z_BUF_ERROR);
break;
}

//uncompress(uncompr, &uncomprLen, compr, comprLen);
// reserved buffer for uncompress: uncompr / size for res buffer : &uncomprLen
//res=uncompress(uncompr,&uncomprLen,compr,comprLen);(unsigned long)SBANKSZ
res=uncompress(decompressedgsf,&uncomprLen,compr,comprLen);
switch(res){
case(Z_OK):
printf("stream decomp status: OK! (%x) \n ",Z_OK);
break;
case(Z_STREAM_END):
printf("stream decomp status: error, unexpected end! (%x)",Z_STREAM_END);
break;
case(Z_NEED_DICT):
printf("stream decomp status: error, need dictionary! (%x)",Z_NEED_DICT);
break;
case(Z_ERRNO):
printf("stream decomp status: error, cant open file! (%x)",Z_ERRNO);
break;
case(Z_STREAM_ERROR):
printf("stream decomp status: error, corrupted source stream! (%x)",Z_STREAM_ERROR);
break;
case(Z_DATA_ERROR):
printf("stream decomp status: error, data not zlib ! (%x)",Z_DATA_ERROR);
break;
case(Z_MEM_ERROR):
printf("stream decomp status: error, memory source corrupted! (%x)",Z_MEM_ERROR);
break;
case(Z_BUF_ERROR):
printf("stream decomp status: error, not enough room in the output buffer! (%x)",Z_BUF_ERROR);
break;
}

printf("DATA at (%x):  \n",(int)&uncompr);
for (res=0;res<strlen(hello);res++){
printf("%c ",decompressedgsf[res]);
if ( (res % 9) == 0 ) printf("\n");
}

//free(compr);
free(uncompr);

*/
//////////////////////ZIP 
/*	strcat(temppath,(char*)"test.zip");
			int unzoutput=unzip(temppath, uncompr, uncomprLen);
			

	if(UNZ_END_OF_LIST_OF_FILE==unzoutput){
		printf("UNZ_END_OF_LIST_OF_FILE \n");
	}
	else if(UNZ_ERRNO==unzoutput){
		printf("error ZLIB deflate \n");
	}
	else if(UNZ_EOF==unzoutput){
		printf("error EOF FOUND \n");
	}
	else if(UNZ_PARAMERROR==unzoutput){
		printf("error UNZ_PARAMERROR \n");
	}
	else if(UNZ_BADZIPFILE==unzoutput){
		printf("error UNZ_BADZIPFILE \n");
	}
	else if(UNZ_INTERNALERROR==unzoutput){
		printf("error UNZ_INTERNALERROR \n");
	}
	else if(UNZ_CRCERROR==unzoutput){
		printf("error UNZ_CRCERROR \n");
	}
	else{
	printf("\x1b[22;1H OK. buf:[%p], Filesz:[%d] \n",uncompr,unzoutput);
	
				for(i=0;i<unzoutput;i++){
					printf("%c ",uncompr[i]);
				}
	
	}*/


//0x03800000 - 0x038082xx = 0x82B4 (33xxx bytesld) - 0x038082B5 - 0x03810000 = 0x7D4C free 
//note:

//  380FFDCh  ..  NDS7 Debug Stacktop / Debug Vector (0=None)
//  380FFF8h  4   NDS7 IRQ Check Bits (hardcoded RAM address)
//  380FFFCh  4   NDS7 IRQ Handler (hardcoded RAM address)

//linker (bin) objects (main.h)

/*
inline __attribute__((always_inline))
int wramtst(int wram,int top){
extern int temp,temp2;
temp=wramtst1(wram,top);
if(temp==alignw(top))
	iprintf("ARM9 test1/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
else 
	iprintf("ARM9RAM tst 1/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp,EWRAM,temp);
temp2=wramtst2(wram,top);
if(temp2==alignw(top))
	iprintf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
else if ((temp2-2)==alignw(top))
	iprintf("ARM9 test2/2: OK (%d bytes @ 0x%lu+0x%lu) \n !!aligned read!!",temp2,EWRAM,temp2);
else 
	iprintf("ARM9RAM tst 2/2:\n FAIL (%d bytes @ 0x%lu+0x%lu) \n",temp2,EWRAM,temp2);
return 0;
}
*/

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

/*
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
*/


//did we use dldi?
bool dldiload = false;

int main(void) {

//fifo setups
irqInit();
fifoInit();

//so far code is stable, this will cause lockups definitely
#ifdef MPURECONFIG
	setgbamap(); //does not Reconfig MPU memory properties (VECTORS set to 0x00000000) anymore :)
	//iprintf("setgbamap();\n"); //<-------CAUSES SERIOUS FREEZES
#endif

installBootStub(false);

videoSetMode(MODE_5_2D);
vramSetBankA(VRAM_A_MAIN_BG);

// set up our bitmap background
//bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0,0);
//decompress(hbmenu_bannerBitmap, BG_GFX,  LZ77Vram);

// Subscreen as a console
videoSetModeSub(MODE_0_2D);
vramSetBankH(VRAM_H_SUB_BG);

//bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0,0);
consoleDemoInit();    

biosPath[0] = 0;
savePath[0] = 0;
patchPath[0] = 0;

#ifndef ROMTEST
//dldi stuff
iprintf("Init Fat...");

if(fatInitDefault()){
	dldiload = true;
	getcwd (temppath, PATH_MAX); //fat:/ 
}
else{
	iprintf("\x1b[20;1H failed to init FAT driver \n");
	iprintf("\x1b[21;1H Current dldi driver[%s]: \n",_io_dldi_stub.friendlyName);
}

//show gbadata iprintf("\x1b[21;1H
strcat(temppath,(char*)"/gba/rs-pzs.gba");
//printgbainfo (temppath);
#endif

//debugging is enabled at startup
isdebugemu_defined();

/******************************************************** GBA EMU INIT CODE *********************************************************************/
bool extram = false; //enabled for dsi

iprintf("CPULoadRom...");
bool failed = !CPULoadRom(temppath,extram);
if(failed)
{
    printf("failed");
	while(1);
}
iprintf("OK\n");


useBios = false;
iprintf("CPUInit\n");
CPUInit(biosPath, useBios,false);

iprintf("CPUReset\n");
CPUReset();

iprintf("BIOS_RegisterRamReset\n");
BIOS_RegisterRamReset(0xFF);


iprintf("arm7init\n");
//execute_arm7_command(0xc0700100,0x1FFFFFFF,gba_frame_line);

iprintf("irqinit\n");
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

//iprintf("payload 1st op:%x / payload size: %d \n",(PATCH_BOOTCODE_PTR[0]),PATCH_BOOTCODE_SIZE);
nds_2_gba_copy((u8*)&somebuf[0],gba_stack_src,PATCH_BOOTCODE_SIZE*4);

//iprintf("gba read @ %x:%x \n",(u32)(u8*)gba_src,CPUReadMemory((u32)(u8*)gba_src));
iprintf("nds payload set correctly! payload size: %d\n",(int)PATCH_BOOTCODE_SIZE*4);

exRegs[0xe]=(u32)gba_entrypoint;
exRegs[0xf]=(u32)(u8*)gba_stack_src;

//int size_dumped = ram2file_nds((const char*)"fat:/armv4dump.bin",(u8*)&puzzle_original[0],puzzle_original_size);

//re enable when opcodes are implemented
#else
    //jump to homebrew rom : But rom is mapped to 0x08000000 because branch relative precalculated offsets are from 0x08000000 base.
    
	//so you cant recompile payloads that use branch relocated addresses from a map different than 0x08000000 (if you plan to stream them from that map)
	//so: rom is set at 0x08000000 and streamed instead ROP approach
	
	//nds_2_gba_copy(rom,gba_src,puzzle_original_size);
    //iprintf("nds gba homebrew set correctly @ %x! payload size: %d\n",gba_src,puzzle_original_size);

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
	
	#ifndef CUSTOMIRQ
	
	//LIBNDS irq
	//REG_IME = IME_DISABLE;
	iprintf("LIBNDS IRQ handling.");
	
	//custom IRQ handlers don't allow jumping to these handlers
	irqSet(IRQ_VBLANK, vblank_thread); //remove for debug
	irqSet(IRQ_HBLANK,hblank_thread);
	irqSet(IRQ_VCOUNT,vcount_thread);

	//upon data transfer IRQ_CARD
	irqEnable(IRQ_VBLANK | IRQ_VCOUNT | IRQ_HBLANK );
	
    iprintf("connecting..");
    
    if(Wifi_InitDefault(true) == true) 
    {
        iprintf("Connected : NDS IP Client Address: %s \n\n ",(char*)print_ip((u32)Wifi_GetIP()));
    }
    else
        iprintf("Failed to connect! \n\n ");
        
	
	while(1) {
		//swiIntrWait(1,IRQ_VBLANK|IRQ_VCOUNT | IRQ_HBLANK |IRQ_FIFO_NOT_EMPTY);
		swiWaitForVBlank();
	}
	
	#else
	
	while(1) {

		//custom IRQ code here
	}
	#endif
return 0;

}