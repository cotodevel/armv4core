#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "Util.h"
#include "main.h"
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
#include "zlib.h"
#include "xenofunzip.h"
#include "gbaemu4ds_fat_ext.h"
#include "translator.h"
#include "gba.arm.core.h"
#include "EEprom.h"
#include "Flash.h"
#include "RTC.h"
#include "Sram.h"
#include "System.h"

#ifdef ROMTEST
#include "rom_pl.h"
#endif

#include "gbaemu4ds_fat_ext.h"

/* exception vector abort handlers
arm variables		status

exceptionrst 		working OK!
exceptionundef		working OK!
exceptionswi		working OK!
exceptionprefabt	working OK!
exceptiondatabt		working OK!
exceptionreserv		working OK!
exceptionirq		working OK!
exceptionfiq		working OK!
*/

/*Processor modes

User	usr	%10000	Normal program execution, no privileges	All
FIQ	fiq	%10001	Fast interrupt handling	All
IRQ	irq	%10010	Normal interrupt handling	All
Supervisor	svc	%10011	Privileged mode for the operating system	All
Abort	abt	%10111	For virtual memory and memory protection	ARMv3+
Undefined	und	%11011	Facilitates emulation of co-processors in hardware	ARMv3+
System	sys	%11111	Runs programs with some privileges	ARMv4+

0x10 usr
0x11 fiq
0x12 irq
0x13 svc
0x17 abt
0x1b und
0x1f sys
*/

/* TGBA (LD memorymap settings)
rom     : ORIGIN = 0x08000000, LENGTH = 32M
gbarom  : ORIGIN = 0x02180000, LENGTH = 3M -512k
gbaew   : ORIGIN = 0x02000000, LENGTH = 256k
ewram   : ORIGIN = 0x02040000, LENGTH = 1M - 256k + 512k = 0x140000 = 1280KB
dtcm    : ORIGIN = 0x027C0000, LENGTH = 16K
vectors : ORIGIN = 0x01FF8000, LENGTH = 16K
itcm    : ORIGIN = 0x01FFC000, LENGTH = 16K
*/

/*
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

/* defaults (NDS)
	Region  Name            Address   Size   Cache WBuf Code Data
	-       Background      00000000h 4GB    -     -    -    -
	0       I/O and VRAM    04000000h 64MB   -     -    R/W  R/W
	1       Main Memory     02000000h 4MB    On    On   R/W  R/W
	2       ARM7-dedicated  027C0000h 256KB  -     -    -    -
	3       GBA Slot        08000000h 128MB  -     -    -    R/W
	4       DTCM            027C0000h 16KB   -     -    -    R/W
	5       ITCM            01000000h 32KB   -     -    R/W  R/W
	6       BIOS            FFFF0000h 32KB   On    -    R    R
	7       Shared Work     027FF000h 4KB    -     -    -    R/W
*/

void exception_dump(char * cause){
	clrscr();
	printf("     ");
	printf("exception_dump(): %s", cause);
	printGBACPU();
}


/* Exception Vectors */
u32 exceptreset(){
	
	printf(" PU exception type: RESET  " );
	while(1);
	return 0;
}

u32 exceptundef(u32 undefopcode){
	/*
	cpu_SetCP15Cnt(cpu_GetCP15Cnt() & ~0x1);
		
	unsigned int *lnk_ptr;
	__asm__ volatile (
	"mov %[lnk_ptr],#0;" "\t"
	"add %[lnk_ptr],%[lnk_ptr], lr" "\t"//"sub lr, lr, #8;" "\t"
	"sub %[lnk_ptr],%[lnk_ptr],#0x8" 
	:[lnk_ptr] "=r" (lnk_ptr)
	);

	printf(" before exception: ");
	if (cpuGetSPSR() & 0x5) printf("thumb mode ");
	else printf("ARM mode ");

	printf(" IN exception: ");
	if (cpuGetCPSR() & 0x5) printf("thumb mode ");
	else printf("ARM mode ");
	printf(" OPCODE: %x",undefopcode);
	printf(" PU exception type: UNDEFINED  at %p (0x%08X) ", lnk_ptr, *(lnk_ptr));
	
	pu_Enable();
	*/
	
	//cpu_SetCP15Cnt(cpu_GetCP15Cnt() & ~0x1);	//MPU turning /off/on causes to jump to 0x00000000
	/*
	printf(" before exception: ");
	if (cpuGetSPSR() & 0x5) printf("thumb mode ");
	else printf("ARM mode ");

	printf(" IN exception: ");
	if (cpuGetCPSR() & 0x5) printf("thumb mode ");
	else printf("ARM mode ");
	*/
	
	char buf[MAX_TGDSFILENAME_LENGTH+1];
	sprintf(buf, "Exception[UNDEFINED]");
	exception_dump(buf);
	while(1==1){
		IRQVBlankWait();
	}

	//pu_Enable();
	return 0;
}

//swi abort part
u32 exceptswi (u32 swiaddress){
	//printf("[swi: %x] ",(unsigned int)swiaddress); //sorry, swi code can¡t handle printf overflows?!!
	//while(1);
	//printf("swi ctm ");
	if (swiaddress == 0x0){
		//swi 0
		//printf("swi 0! ");
		return 0;
	}
	else if(swiaddress == 0x1){
		//swi 1
		//printf("swi 1! ");
		return 0;
	}
	else if(swiaddress == 0x2){
		//swi 2
		//printf("swi 2! ");
		return 0;
	}
	
	else if(swiaddress == 0x4){
		//swi 2
		//printf("swi 4! ");
		return 0;
	}
	
	char buf[MAX_TGDSFILENAME_LENGTH+1];
	sprintf(buf, "Exception[SWI]: range 0..1Fh: Current: %x", swiaddress);
	exception_dump(buf);
	while(1==1){
		IRQVBlankWait();
	}		
	return 0;
}

u32 exceptprefabt(){
	char buf[MAX_TGDSFILENAME_LENGTH+1];
	sprintf(buf, "Exception[PREFETCH ABORT]");
	exception_dump(buf);
	while(1==1){
		IRQVBlankWait();
	}
	return 0;
}

//data_abort handler
u32 exceptdataabt(u32 dabtaddress){
	/*
	//16 bit reads
	if( ((exRegs[0x10]>>5)&1) == 0x1 ){
	
		if((dabtaddress >= 0x08000000) && (dabtaddress < 0x08000200)  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,gbaheaderbuf[(dabtaddress - 0x08000000)/4]);
			return ((gbaheaderbuf[(dabtaddress - 0x08000000)/4])&0xffff);
		}
	
		else if((dabtaddress >= 0x08000200) && (dabtaddress < (romsize+0x08000000))  ){
			//printf(" => data abt! (%x):%x ",dabtaddress, stream_readu16(dabtaddress - 0x08000000));
			return stream_readu16(dabtaddress - 0x08000000);
		}
	}
	//32Bit reads
	else{
	
		if((dabtaddress >= 0x08000000) && (dabtaddress < 0x08000200)  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,gbaheaderbuf[(dabtaddress - 0x08000000)/4]);
			return gbaheaderbuf[(dabtaddress - 0x08000000)/4];
		}
	
		else if((dabtaddress >= 0x08000200) && (dabtaddress < (romsize+0x08000000))  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,stream_readu32(dabtaddress - 0x08000000));
			return stream_readu32(dabtaddress - 0x08000000);
		}
	}
	*/

	char buf[MAX_TGDSFILENAME_LENGTH+1];
	sprintf(buf, "Exception[DATAABORT]: GBAPC: %x", exRegs[0xf] + 12);	//prefetch is +8 +4
	exception_dump(buf);
	while(1==1){
		IRQVBlankWait();
	}
	return 0;
}


u8 * gbawram = NULL;	//[256*1024]; //genuine reference - wram that is 32bit per chunk
u8 * palram = NULL;	//[0x400];
u8 * gbabios = NULL;	//[0x4000];
u8 * gbaintram = NULL;	//[0x8000];
u8 * gbaoam = NULL;	//[0x400];
u8 * saveram = NULL;	//[128*1024]; //128K
u8 * iomem[0x400];

//disk buffer
volatile u32 disk_buf[chucksize]; 

u8 first32krom[32*1024];

int i=0;
struct gbaheader_t gbaheader;

/*
//ldr/str (inline)
inline __attribute__((always_inline))
u32 ldru32inlasm(u32 x1){
u32 y1;
__asm__ volatile(
				"ldr %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u16 ldru16inlasm(u32 x1){
u16 y1;
__asm__ volatile(
				"ldrh %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u8 ldru8inlasm(u32 x1){
u8 y1;
__asm__ volatile(
				"ldrb %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

inline __attribute__((always_inline))
u8 stru8inlasm(u32 x1,u32 x2,u8 y1){
u8 out;
__asm__ volatile(
				"strb %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u16 stru16inlasm(u32 x1,u32 x2,u16 y1){
u16 out;
__asm__ volatile(
				"strh %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u32 stru32inlasm(u32 x1,u32 x2,u32 y1){
u32 out;
__asm__ volatile(
				"str %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

*/

//counts leading zeroes :)
u8 clzero(u32 var){   
    u8 cnt=0;
    u32 var3;
    if (var>0xffffffff) return 0;
   
    var3=var; //copy
    var=0xFFFFFFFF-var;
    while((var>>cnt)&1){
        cnt++;
    }
    if ( (((var3&0xf0000000)>>28) >0x7) && (((var3&0xff000000)>>24)<0xf)){
        var=((var3&0xf0000000)>>28);
        var-=8; //bit 31 can't count to zero up to this point
            while(var&1) {
                cnt++; var=var>>1;
            }
    }
	return cnt;
}


// returns unary(decimal) ammount of bits using the Hamming Weight approach 

//8 bit depth Lookuptable 
//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
const u8 /* __attribute__((section(".dtcm"))) */ minilut[0x10] = {
	0,	1,	1,	2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,		//0n
};

u8 lutu16bitcnt(u16 x){
	return (minilut[x &0xf] + minilut[(x>>4) &0xf] + minilut[(x>>8) &0xf] + minilut[(x>>12) &0xf]);
}

u8 lutu32bitcnt(u32 x){
	return (lutu16bitcnt(x & 0xffff) + lutu16bitcnt(x >> 16));
}


//count bits set in u32 field and return u32 decimal #imm
//printf(" ammount of register bytes: %x",(unsigned int)lookupu16(thumbinstr&0xff));
//u32 temp=0x00000000;
//printf("lut:%d",lutu32bitcnt(temp));	//(thumbinstr&0xff)

const u32  objtilesaddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakramwaitstate[4] = { 4, 3, 2, 8 };
const u8 gamepakwaitstate[4] =  { 4, 3, 2, 8 };
const u8 gamepakwaitstate0[2] = { 2, 1 };
const u8 gamepakwaitstate1[2] = { 4, 1 };
const u8 gamepakwaitstate2[2] = { 8, 1 };
const bool isInRom [16]=
  { false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, false, false };


u32 dummycall(u32 arg){
	return arg;
}

// swaps a 16-bit value
u16 swap16(u16 v){
	return (v<<8)|(v>>8);
}

//CPUUpdateRegister only handles GBA Hardware registers

//Where as UPDATE_REG writes all the GBA io map
u32 UPDATE_REG(u32 address, u32 value){
	WRITE16LE(iomem[address],(u16)value);
	return 0;
}

int utilReadInt2(FILE *f){
  int res = 0;
  int c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c;
  c = fgetc(f);
  if(c == EOF)
    return -1;
  return c + (res<<8);
}

int utilReadInt3(FILE *f){
  int res = 0;
  int c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c;
  c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c + (res<<8);
  c = fgetc(f);
  if(c == EOF)
    return -1;
  return c + (res<<8);
}

void utilGetBaseName(const char *file, char *buffer){
  strcpy(buffer, file);
}

void utilGetBasePath(const char *file, char *buffer){
	strcpy(buffer,file);

	char *p = strrchr(buffer, '\\');

	if(p)
		*p = 0;
}

int LengthFromString(const char * timestring) {
	int c=0,decimalused=0,multiplier=1;
	int total=0;
	if (strlen(timestring) == 0) return 0;
	for (c=strlen(timestring)-1; c >= 0; c--) {
		if (timestring[c]=='.' || timestring[c]==',') {
			decimalused=1;
			total*=1000/multiplier;
			multiplier=1000;
		} else if (timestring[c]==':') multiplier=multiplier*6/10;
		else {
			total+=(timestring[c]-'0')*multiplier;
			multiplier*=10;
		}
	}
	if (!decimalused) total*=1000;
	return total;
}

int VolumeFromString(const char * volumestring) {
	int c=0,decimalused=0,multiplier=1;
	int total=0;
	if(strlen(volumestring) == 0) return 0;
	for(c=strlen(volumestring)-1; c >= 0; c--) {
		if (volumestring[c]=='.' || volumestring[c]==',') {
			decimalused=1;
			total*=1000/multiplier;
			multiplier=1000;
		} 
		else if ((volumestring[c]>='0')&& (volumestring[c]<='9')) {
			total+=(volumestring[c]-'0')*multiplier;
			multiplier*=10;
		}
		else
			break;
	}
	if (!decimalused) total*=1000;
	//return (float) total / 1000.;
	return total;
}


void initmemory(){
	gbawram = (u8*)malloc(256*1024);
	palram = (u8*)malloc(0x400);
	gbabios = (u8*)malloc(0x4000);
	gbaintram = (u8*)malloc(0x8000);
	gbaoam = (u8*)malloc(0x400);
	saveram = (u8*)malloc(128*1024);

	int ramtestsz=wramtstasm((int)workRAM, 256*1024);
	if(ramtestsz==alignw(256*1024))
		memset(gbawram, 0, 256*1024); //Same as workRAM
	else{
		printf(" FAILED ALLOCING GBAEWRAM[%x]:@%d (bytes: %d)",(unsigned int)(u8*)gbawram,ramtestsz,0x10000);
		while(1);
	}
	
	ramtestsz=wramtstasm((int)iomem,0x400);
	if(ramtestsz==alignw(0x400)){
		memset((void*)iomem,0x0,0x400);	//same as u8 ioMem[]
	}
	else{
		printf(" IOMEM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)iomem,ramtestsz);
	}

	ramtestsz=wramtstasm((int)bios,0x4000);	
	if(ramtestsz==alignw(0x4000)){
		memset(gbabios, 0, 0x4000); 	//Same as u8 * bios
	}	
	else{
		printf(" BIOS tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)bios,ramtestsz);
	}
	
	ramtestsz=wramtstasm((int)internalRAM,0x8000);
	if(ramtestsz==alignw(0x8000)){
		memset(gbaintram, 0, 0x8000); //Same as u8* internalRAM
	}
	else{
		printf(" IRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)internalRAM,ramtestsz);
	}
	
	ramtestsz=wramtstasm((int)(u8*)palram,0x400);
	if(ramtestsz==alignw(0x400)){
		memset(palram, 0, 0x400); 
	}
	else {
		printf(" PaletteRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)palram,ramtestsz);
	}
	
	ramtestsz=wramtstasm((int)gbaoam,0x400);
	if(ramtestsz==alignw(0x400)){
		memset(gbaoam, 0, 0x400);	//Same as u8* oam
	}
	else{
		printf(" OAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)oam,ramtestsz);
	}
	
	ramtestsz=wramtstasm((int)saveram, 128*1024);
	if(ramtestsz==alignw(128*1024)){
		memset(saveram, 0, 128*1024);	//128K Contiguous SaveRAM
	}
	else{
		printf(" SAVERAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)oam,ramtestsz);
	}
	
}

void initemu(){
	sound_clock_ticks = 167772; // 1/100 second
	
	//refresh jump opcode in biosProtected vector
	biosProtected[0] = 0x00;
	biosProtected[1] = 0xf0;
	biosProtected[2] = 0x29;
	biosProtected[3] = 0xe1;
	
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

	//GBA address MAP setup
	for(i = 0; i < 256; i++){
		map[i].address = (u8 *)(u32)0;
		map[i].mask = 0;
	}

	for(i = 0; i < 0x400; i++)
		ioReadable[i] = true;
	for(i = 0x10; i < 0x48; i++)
		ioReadable[i] = false;
	for(i = 0x4c; i < 0x50; i++)
		ioReadable[i] = false;
	for(i = 0x54; i < 0x60; i++)
		ioReadable[i] = false;
	for(i = 0x8c; i < 0x90; i++)
		ioReadable[i] = false;
	for(i = 0xa0; i < 0xb8; i++)
		ioReadable[i] = false;
	for(i = 0xbc; i < 0xc4; i++)
		ioReadable[i] = false;
	for(i = 0xc8; i < 0xd0; i++)
		ioReadable[i] = false;
	for(i = 0xd4; i < 0xdc; i++)
		ioReadable[i] = false;
	for(i = 0xe0; i < 0x100; i++)
		ioReadable[i] = false;
	for(i = 0x110; i < 0x120; i++)
		ioReadable[i] = false;
	for(i = 0x12c; i < 0x130; i++)
		ioReadable[i] = false;
	for(i = 0x138; i < 0x140; i++)
		ioReadable[i] = false;
	for(i = 0x144; i < 0x150; i++)
		ioReadable[i] = false;
	for(i = 0x15c; i < 0x200; i++)
		ioReadable[i] = false;
	for(i = 0x20c; i < 0x300; i++)
		ioReadable[i] = false;
	for(i = 0x304; i < 0x400; i++)
		ioReadable[i] = false;

	//vba core init
	GBADISPCNT  = 0x0080;
	GBADISPSTAT = 0x0000;

	#ifdef NOBIOS
		GBAVCOUNT   =  0 ;
	#else
		GBAVCOUNT 	=  0x007E;
	#endif

	GBABG0CNT   = 0x0000;
	GBABG1CNT   = 0x0000;
	GBABG2CNT   = 0x0000;
	GBABG3CNT   = 0x0000;
	GBABG0HOFS  = 0x0000;
	GBABG0VOFS  = 0x0000;
	GBABG1HOFS  = 0x0000;
	GBABG1VOFS  = 0x0000;
	GBABG2HOFS  = 0x0000;
	GBABG2VOFS  = 0x0000;
	GBABG3HOFS  = 0x0000;
	GBABG3VOFS  = 0x0000;
	GBABG2PA    = 0x0100;
	GBABG2PB    = 0x0000;
	GBABG2PC    = 0x0000;
	GBABG2PD    = 0x0100;
	GBABG2X_L   = 0x0000;
	GBABG2X_H   = 0x0000;
	GBABG2Y_L   = 0x0000;
	GBABG2Y_H   = 0x0000;
	GBABG3PA    = 0x0100;
	GBABG3PB    = 0x0000;
	GBABG3PC    = 0x0000;
	GBABG3PD    = 0x0100;
	GBABG3X_L   = 0x0000;
	GBABG3X_H   = 0x0000;
	GBABG3Y_L   = 0x0000;
	GBABG3Y_H   = 0x0000;
	GBAWIN0H    = 0x0000;
	GBAWIN1H    = 0x0000;
	GBAWIN0V    = 0x0000;
	GBAWIN1V    = 0x0000;
	GBAWININ    = 0x0000;
	GBAWINOUT   = 0x0000;
	GBAMOSAIC   = 0x0000;
	GBABLDMOD   = 0x0000;
	GBACOLEV    = 0x0000;
	GBACOLY     = 0x0000;
	GBADM0SAD_L = 0x0000;
	GBADM0SAD_H = 0x0000;
	GBADM0DAD_L = 0x0000;
	GBADM0DAD_H = 0x0000;
	GBADM0CNT_L = 0x0000;
	GBADM0CNT_H = 0x0000;
	GBADM1SAD_L = 0x0000;
	GBADM1SAD_H = 0x0000;
	GBADM1DAD_L = 0x0000;
	GBADM1DAD_H = 0x0000;
	GBADM1CNT_L = 0x0000;
	GBADM1CNT_H = 0x0000;
	GBADM2SAD_L = 0x0000;
	GBADM2SAD_H = 0x0000;
	GBADM2DAD_L = 0x0000;
	GBADM2DAD_H = 0x0000;
	GBADM2CNT_L = 0x0000;
	GBADM2CNT_H = 0x0000;
	GBADM3SAD_L = 0x0000;
	GBADM3SAD_H = 0x0000;
	GBADM3DAD_L = 0x0000;
	GBADM3DAD_H = 0x0000;
	GBADM3CNT_L = 0x0000;
	GBADM3CNT_H = 0x0000;
	GBATM0D     = 0x0000;
	GBATM0CNT   = 0x0000;
	GBATM1D     = 0x0000;
	GBATM1CNT   = 0x0000;
	GBATM2D     = 0x0000;
	GBATM2CNT   = 0x0000;
	GBATM3D     = 0x0000;
	GBATM3CNT   = 0x0000;
	GBAP1       = 0x03FF;
	GBAIE=0x0000;			//  IE       = 0x0000;
	GBAIF=0x0000;			//  IF       = 0x0000;
	GBAIME=0x0000;			// IME      = 0x0000;
	
	//GBA IO Init
	UPDATE_REG(0x00, GBADISPCNT);
	UPDATE_REG(0x06, VCOUNT);
	UPDATE_REG(0x20, GBABG2PA);
	UPDATE_REG(0x26, GBABG2PD);
	UPDATE_REG(0x30, GBABG3PA);
	UPDATE_REG(0x36, GBABG3PD);
	UPDATE_REG(0x130, GBAP1);
	UPDATE_REG(0x88, 0x200);

	#ifndef NOBIOS
	lcdTicks = 1008;
	#else
	lcdTicks = 208;
	#endif

	timer0On = false;
	timer0Ticks = 0;
	timer0Reload = 0;
	timer0ClockReload  = 0;
	timer1On = false;
	timer1Ticks = 0;
	timer1Reload = 0;
	timer1ClockReload  = 0;
	timer2On = false;
	timer2Ticks = 0;
	timer2Reload = 0;
	timer2ClockReload  = 0;
	timer3On = false;
	timer3Ticks = 0;
	timer3Reload = 0;
	timer3ClockReload  = 0;
	dma0Source = 0;
	dma0Dest = 0;
	dma1Source = 0;
	dma1Dest = 0;
	dma2Source = 0;
	dma2Dest = 0;
	dma3Source = 0;
	dma3Dest = 0;
	windowOn = false;
	frameCount = 0;
	saveType = 0;
	layerEnable = GBADISPCNT & layerSettings;

	//OK so far
	map[0].address = bios;
	map[0].mask = 0x3FFF;
	map[2].address = workRAM;
	map[2].mask = 0x3FFFF;
	map[3].address = internalRAM; 
	map[3].mask = 0x7FFF;
	map[4].address = iomem;
	map[4].mask = 0x3FF;
	map[5].address = palram;
	map[5].mask = 0x3FF;
	map[6].address = vram;
	map[6].mask = 0x1FFFF;
	map[7].address = oam;
	map[7].mask = 0x3FF;
	map[8].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	 //ROM entrypoint
	map[8].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/ //MASK for GBA addressable ROMDATA
	map[9].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[9].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
	map[10].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[10].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
	map[12].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[12].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/

	//Cpu_Stack_USR EQU 0x03007F00 ; GBA USR stack adress
	//Cpu_Stack_IRQ EQU 0x03007FA0 ; GBA IRQ stack adress
	//Cpu_Stack_SVC EQU 0x03007FE0 ; GBA SVC stack adress

	//reg[13].I = 0x03007F00;
	//reg[15].I = 0x08000000;
	//reg[16].I = 0x00000000;
	//reg[R13_IRQ].I = 0x03007FA0;
	//reg[R13_SVC].I = 0x03007FE0;
	//armIrqEnable = true;
	
}

void u32store(u32 address, u32 value){
	*(u32*)(address)=value;
}

void u16store(u32 address, u16 value){
	*(u16*)(address)=value;
}

void u8store(u32 addr, u8 value){
	*(u8*)(addr)=value;
}

u32 u32read(u32 address){
	return *(u32*)(address);
}

u16 u16read(u32 address){
	return *(u16*)(address);
}

u8 u8read(u32 addr){
	return *(u8*)(addr);
}

u32 READ32LE(u8 * addr){
    return u32read((u32)(u8*)addr);
}

u16 READ16LE(u8 * addr){
    return u16read((u32)(u8*)addr);
}

void WRITE32LE(u8 * x,u32 v){
	u32store((u32)x,v);
}

void WRITE16LE(u8 * x,u16 v){
	u16store((u32)x,v);
}

//0: no, it wasn't enabled when compiled.
//1: yes, it was enabled when compiled.
u8 was_debugemu_enabled=0;

//toggles if debugemu is specified. Run at startup ONLY.
void isdebugemu_defined(){
	#ifdef DEBUGEMU
		was_debugemu_enabled=1;
	#endif
}


//loops-executes the emulator until pc specified
int executeuntil_pc(u32 target_pc){
	int opcodes_executed=0;
	
	#ifdef DEBUGEMU
	#undef DEBUGEMU
	#endif

	//execute until desired opcode:
	while(target_pc != exRegs[0xf]){
		cpu_fdexecute();
		opcodes_executed++;
	}

	if(was_debugemu_enabled==1){
		#define DEBUGEMU
	}

	return opcodes_executed;
}

void CPUInit(const char *biosFileName, bool useBiosFile,bool extram)
{
	initemu();
	
#ifdef WORDS_BIGENDIAN
	if(!cpuBiosSwapped) {
		for(unsigned int i = 0; i < sizeof(myROM)/4; i++) {
			WRITE32LE((u8*)&myROM[i], myROM[i]);
		}
		cpuBiosSwapped = true;
	}
#endif
	gbaSaveType = 0;
	eepromInUse = 0;
	saveType = 0;
	useBios = false;
  
	/*
	if(useBiosFile) {
		//literally load the bios into u8* bios buffer
	}
	*/
	
	if(!useBios){
		memcpy(bios, myROM, sizeof(myROM));
	}

	int i = 0;
	biosProtected[0] = 0x00;
	biosProtected[1] = 0xf0;
	biosProtected[2] = 0x29;
	biosProtected[3] = 0xe1;

	for(i = 0; i < 256; i++) {
		int count = 0;
		int j;
		for(j = 0; j < 8; j++)
			if(i & (1 << j))
        count++;
    
		cpuBitsSet[i] = count;
    
		for(j = 0; j < 8; j++)
			if(i & (1 << j))
				break;
		cpuLowestBitSet[i] = j;
	}

	for(i = 0; i < 0x400; i++)
		ioReadable[i] = true;
	for(i = 0x10; i < 0x48; i++)
		ioReadable[i] = false;
	for(i = 0x4c; i < 0x50; i++)
		ioReadable[i] = false;
	for(i = 0x54; i < 0x60; i++)
		ioReadable[i] = false;
	for(i = 0x8c; i < 0x90; i++)
		ioReadable[i] = false;
	for(i = 0xa0; i < 0xb8; i++)
		ioReadable[i] = false;
	for(i = 0xbc; i < 0xc4; i++)
		ioReadable[i] = false;
	for(i = 0xc8; i < 0xd0; i++)
		ioReadable[i] = false;
	for(i = 0xd4; i < 0xdc; i++)
		ioReadable[i] = false;
	for(i = 0xe0; i < 0x100; i++)
		ioReadable[i] = false;
	for(i = 0x110; i < 0x120; i++)
		ioReadable[i] = false;
	for(i = 0x12c; i < 0x130; i++)
		ioReadable[i] = false;
	for(i = 0x138; i < 0x140; i++)
		ioReadable[i] = false;
	for(i = 0x144; i < 0x150; i++)
		ioReadable[i] = false;
	for(i = 0x15c; i < 0x200; i++)
		ioReadable[i] = false;
	for(i = 0x20c; i < 0x300; i++)
		ioReadable[i] = false;
	for(i = 0x304; i < 0x400; i++)
		ioReadable[i] = false;
	
	//if(romSize < 0x1fe2000) {
	//  *((u16 *)&rom[0x1fe209c]) = 0xdffa; // SWI 0xFA
	//  *((u16 *)&rom[0x1fe209e]) = 0x4770; // BX LR
	//} 
	//else {
		//agbPrintEnable(false);
	//}
	
}


int CPULoadRom(const char *szFile, bool extram){
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
	romSize = 0x40000;
	
	#ifndef ROMTEST
	#ifdef BIOSHANDLER
		//bios copy to biosram
		FILE *f = fopen("0:/bios.bin", "r");
		if(!f){ 
			printf("there is no bios.bin in root!"); while(1);
		}
		int fileSize=fread((void*)(u8*)bios, 1, 0x4000,f);
		fclose(f);
		if(fileSize!=0x4000){
			printf("failed bios copy @ %x! so far:%d bytes",(unsigned int)bios,fileSize);
			while(1);
		}
	#else
		int fileSize=0;
		FILE *f;
	#endif
	//gbarom setup
	f = fopen(szFile, "r");
	if(!f) {
		printf("Error opening image %s",szFile);
		return 0;
	}

	//copy first32krom/ because gba4ds's first 32k sector is bugged (returns 0 reads)
	fread((u8*)&first32krom[0],sizeof(first32krom),1,f);
	fseek(f,0,SEEK_END);
	fileSize = ftell(f);
	fseek(f,0,SEEK_SET);
	generatefilemap(f,fileSize);
	romSize=fileSize;	//size readjusted for final alloc'd rom	
	
	//gba rom entrypoint from header
	memcpy((u8*)&gbaheader,(u8*)&first32krom[0], sizeof(gbaheader));
	exRegs[0xe]=exRegs[0xf]=(u8*)(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
	printf("FS:entrypoint @ %x! ",(unsigned int)(u32*)exRegs[0xf]);
	
	ichflyfilestream = f; //pass the filestreampointer and make it global
	ichflyfilestreamsize = fileSize;
	printf("generated filemap! OK:");
#endif

#ifdef ROMTEST
	//filesize
	romSize=rom_pl_size;	//size readjusted for final alloc'd rom
	//gba rom entrypoint from header
	memcpy((u8*)&gbaheader,(u8*)&rom_pl[0], sizeof(gbaheader));
	exRegs[0xe]=exRegs[0xf]=(u8*)(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
	printf("ROMTEST:entrypoint @ %x! ",(unsigned int)(u32*)exRegs[0xf]);
	
#endif
	
	flashInit();
	eepromInit();
	return romSize;
}


//saves data to file
int ram2file_nds(char * fname,u8 * buffer,int size){
    FILE * fhandle = fopen(fname,"w");
    if(fhandle){
        
        int sizewritten=fwrite((u8*)buffer, 1, size, fhandle); //2) perform read (512bytes read (128 reads))
        fclose(fhandle);
        if(size == sizewritten){
            printf("OK exported %d bytes @ fname: %s ",(int)sizewritten,(char*)fname);
            return 0;
        }
        else{
            printf("error, only copied %d bytes @ fname: %s ",(int)sizewritten,(char*)fname);
        }
    }
    else{
        printf("file was not created: %s",fname);
    }
    return 1;
}

void printGBACPU(){
	int cntr=0;
	printf("Hardware Registers: ");
	//Base sp cpu <mode>
	if ((exRegs[0x10]&0x1f) == (0x10) || (exRegs[0x10]&0x1f) == (0x1f))
		printf(" USR/SYS STACK ");
	else if ((exRegs[0x10]&0x1f)==(0x11))
		printf(" FIQ STACK");
	else if ((exRegs[0x10]&0x1f)==(0x12))
		printf(" IRQ STACK");
	else if ((exRegs[0x10]&0x1f)==(0x13))
		printf(" SVC STACK");
	else if ((exRegs[0x10]&0x1f)==(0x17))
		printf(" ABT STACK");
	else if ((exRegs[0x10]&0x1f)==(0x1b))
		printf(" UND STACK");
	else
		printf(" STACK LOAD ERROR CPSR: %x",(unsigned int)exRegs[0x10]);
	
	for(cntr=0;cntr<15;cntr=cntr+3){
		printf(" r%d :[0x%x] r%d :[0x%x] r%d :[0x%x] ", 0 + cntr, (unsigned int)exRegs[0 + cntr], 1 + cntr, (unsigned int)exRegs[1 + cntr], 2 + cntr, (unsigned int)exRegs[2 + cntr]);
	}
	printf(" r%d Addr[0x%x]: [0x%x] ", 0xf, (unsigned int)exRegs[0xf], CPUReadMemory(exRegs[0xf]));
	
	
	//user/sys has no SPSR
	/*
	if ( ((exRegs[0x10]&0x1f) == (0x10)) || ((exRegs[0x10]&0x1f) == (0x1f)) ){
		
	}
	*/
	//fiq
	if((exRegs[0x10]&0x1f)==0x11){
		printf("CPSR[%x] / SPSR_fiq:[%x] ",(unsigned int)exRegs[0x10],(unsigned int)getSPSRFromCPSR(exRegs[0x10]));
	}
	//irq
	else if((exRegs[0x10]&0x1f)==0x12){
		printf("CPSR[%x] / SPSR_irq:[%x] ",(unsigned int)exRegs[0x10],(unsigned int)getSPSRFromCPSR(exRegs[0x10]));
	}
	//svc
	else if((exRegs[0x10]&0x1f)==0x13){
		printf("CPSR[%x] / SPSR_svc:[%x] ",(unsigned int)exRegs[0x10],(unsigned int)getSPSRFromCPSR(exRegs[0x10]));
	}
	//abort
	else if((exRegs[0x10]&0x1f)==0x17){
		printf("CPSR[%x] / SPSR_abt:[%x] ",(unsigned int)exRegs[0x10],(unsigned int)getSPSRFromCPSR(exRegs[0x10]));
	}
	//undef
	else if((exRegs[0x10]&0x1f)==0x1b){
		printf("CPSR[%x] / SPSR_und:[%x] ",(unsigned int)exRegs[0x10],(unsigned int)getSPSRFromCPSR(exRegs[0x10]));
	}
	if(armstate == CPUSTATE_ARM){
		printf("CPU Model: %s - Running: %d - CPUMode: %s", ARMModel, cpuStart, "ARM");
	}
	else{
		printf("CPU Model: %s - Running: %d - CPUMode: %s", ARMModel, cpuStart, "THUMB");
	}
	printf("CpuTotalTicks:(%d) / lcdTicks:(%d) / vcount: (%d)",(int)cpuTotalTicks,(int)lcdTicks,(int)GBAVCOUNT);
}


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


int CPUUpdateTicks(){
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



void vcount_thread(){
	gbacpu_refreshvcount();	
	GBAVCOUNT++;
}

void vblank_thread(){

}

void hblank_thread(){
	cpuTotalTicks++;
}

//(CPUinterrupts)
//software interrupts service (GBA BIOS calls!)
u32 swi_virt(u32 swinum){
    switch(swinum) {
        case 0x00:
            bios_cpureset();
		break;
        case 0x01:
            bios_registerramreset(exRegs[0]);
		break;
        case 0x02:
        {
            #ifdef DEV_VERSION
                printf("Halt: GBAIE %x",(unsigned int)GBAIE);
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
                printf("Stop");
            #endif
			//holdState = true;
			//holdType = -1;
			//stopState = true;
			//cpuNextEvent = cpuTotalTicks;
			
            //ori
            //gba4ds_swiIntrWait(1,(GBAIE & 0x6080));
            
		}
        break;
        case 0x04:
        {
            #ifdef DEV_VERSION
            printf("IntrWait: 0x%08x,0x%08x",(unsigned int)R[0],(unsigned int)R[1]);      
            #endif
            
            //gba4ds_swiIntrWait(R[0],R[1]);
            //CPUSoftwareInterrupt();
        
		}
        break;    
        case 0x05:
        {
            #ifdef DEV_VERSION
                //printf("VBlankIntrWait: 0x%08X 0x%08X",REG_IE,anytimejmpfilter);
                //VblankHandler(); //todo
            #endif
            //if((REG_DISPSTAT & DISP_IN_VBLANK)) while((REG_DISPSTAT & DISP_IN_VBLANK)); //workaround
            //while(!(REG_DISPSTAT & DISP_IN_VBLANK));
            
            //send cmd
            ////execute_arm7_command(0xc0700100,0x1FFFFFFB,0x0);
            //gba4ds_swiWaitForVBlank();
            
            //gba4ds_swiWaitForVBlank();
            ////execute_arm7_command(0xc3730100,0x0,0x0);
            
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
        
        case 0x19:
        {
            //#ifdef DEV_VERSION
            printf("SoundBiasSet: 0x%08x ",(unsigned int)exRegs[0]);
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


u32 debuggeroutput(){
	return 0;
}