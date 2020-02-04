#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <fcntl.h>
#include <sys/stat.h>

//Protection Unit calls, wrappers, go here.
#include "pu.h"
#include "supervisor.h"
#include "opcode.h"
#include "Util.h"
#include "buffer.h"
#include "translator.h"

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

extern u32  __attribute__((section(".vectors"))) (*exceptrstC)();
extern u32  __attribute__((section(".vectors"))) (*exceptundC)();
extern u32  __attribute__((section(".vectors"))) (*exceptswiC)(u32);
extern u32  __attribute__((section(".vectors"))) (*exceptprefabtC)();
extern u32  __attribute__((section(".vectors"))) (*exceptdatabtC)(u32);
extern u32  __attribute__((section(".vectors"))) (*exceptreservC)();
extern u32  __attribute__((section(".vectors"))) (*exceptirqC)();

void exception_dump(){
	printf("exception_dump() .");
	while(1==1){
		IRQVBlankWait();
	}
}

void gbamode(){
	pu_SetCodePermissions(0x06333333);	//instruction tcm rights: bit 0-3 reg0 - bit 4-7 reg1 - etc...
	pu_SetDataPermissions(0x06333333);	//data tcm rights: 
}

void ndsmode(){
	//ori: pu_SetCodePermissions(0x33330333); //instruction region access permissions to others - C5,C0,3 - access rights: 3 rw , 6 ro
	//ori: pu_SetDataPermissions(0x33330333); //data region access permission to others - c5, c0, 2 <-- could cause freezes if not properly set
	pu_SetCodePermissions(0x33333333); //instruction region access permissions to others - C5,C0,3 - access rights: 3 rw , 6 ro
	pu_SetDataPermissions(0x33333333); //data region access permission to others - c5, c0, 2 <-- could cause freezes if not properly set
}

//ori C install handlers

//branching method
/*
unsigned Install_Handler (unsigned routine, unsigned *vector){

 Updates contents of 'vector' to contain branch instruction 
 to reach 'routine' from 'vector'. Function return value is 
 original contents of 'vector'.
 NB: 'Routine' must be within range of 32Mbytes from 'vector'.

unsigned vec, oldvec;
vec = ((routine - (unsigned)vector - 0x8)>>2);
if (vec & 0xff000000){
	printf ("Installation of Handler failed");
	exit (1);
}
vec = 0xea000000 | vec; //branching method
oldvec = *vector;
*vector = vec;
return (oldvec);
}
*/
//Install_Handler(handler,vectors);

//LDR,PC+adress method
/*
unsigned Install_Handler (unsigned *location, unsigned *vector)
 Updates contents of 'vector' to contain LDR pc, [pc, #offset] 
 instruction to cause long branch to address in ‘location’. 
 Function return value is original contents of 'vector'.
{ unsigned vec, oldvec;
vec = ((unsigned)location - (unsigned)vector-0x8) | 0xe59ff000 //ldr pc,arm register method
oldvec = *vector;
*vector = vec;
return (oldvec);
}
*/

//BIOS debug vector: 0x027FFD9C //ARM9 exception vector: 0xFFFF0000 / <-- only if vectors & 0xffff0000
//exception handler address, exception vectors
unsigned Install_Handler (u32 * location, u32 *vector){
	// Updates contents of 'vector' to contain LDR pc, [pc, #offset] 
	// instruction to cause long branch to address in ‘location’. 
	// Function return value is original contents of 'vector'.

	u32 vec, oldvec;
	vec = ((u32*)location - (u32*)vector- 0x8) | 0xe59ff000; //ldr pc,arm register method
	oldvec = *vector;
	*vector = vec;
	return (oldvec); //return (vec);
}


//vector: reset,undefined,swi,prefetch_abort,data_abort,reserved,irq,fiq
void puhandlersetup(u32funcptr customrsthdl,u32funcptr customundefhdl,u32funcptr customswihdl ,
					u32funcptr customprefabthdl,u32funcptr customdatabthdl,u32funcptr customresvhdl,
					u32funcptr customirqhdl
					){
	exceptrstC = 		customrsthdl; 		//inter_regs.s
	exceptundC = 		customundefhdl;
	exceptswiC = 		customswihdl;
	exceptprefabtC = 	customprefabthdl;
	exceptdatabtC = 	customdatabthdl;
	exceptreservC = 	customresvhdl;
	exceptirqC =		customirqhdl;
	
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
	
	exception_dump();
	printf(" PU exception type: UNDEFINED ");
	while(1);

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
	
	/*
	//detect game filesize otherwise cause freezes
	else if((swiaddress >= 0x08000000) && (swiaddress < (romsize+0x08000000))  ){
		//printf("swigb");
		//return (ichfly_readu32(swiaddress ^ (u32)0x08000000));
	}
	*/
	
	else{
		//printf("swi unknown! ");
		/*
		//cpu_SetCP15Cnt(cpu_GetCP15Cnt() & ~0x1);
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
		printf(" SWI #num: %x",swiaddress);
		printf(" PU exception type: swi  at %p (0x%08X) ", lnk_ptr, *(lnk_ptr));
		//printf(" btw romsize: %x",romsize);
	
		//pu_Enable();
		*/
	}

	exception_dump();
	printf("swi exception within range 0..1Fh.");
	printf(" PU exception type: SWI ");
	while(1);		
	return 0;
}

u32 exceptprefabt(){
	exception_dump();
	printf(" PU exception type: PREFETCH ABORT ");
	while(1);
	return 0;
}

//data_abort handler
u32 exceptdataabt(u32 dabtaddress){
	/*
	//16 bit reads
	if( ((cpsrvirt>>5)&1) == 0x1 ){
	
		if((dabtaddress >= 0x08000000) && (dabtaddress < 0x08000200)  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,gbaheaderbuf[(dabtaddress ^ 0x08000000)/4]);
			return ((gbaheaderbuf[(dabtaddress ^ 0x08000000)/4])&0xffff);
		}
	
		else if((dabtaddress >= 0x08000200) && (dabtaddress < (romsize+0x08000000))  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,ichfly_readu32(dabtaddress ^ 0x08000000));
			return stream_readu16(dabtaddress ^ 0x08000000);
		}
	}
	//32Bit reads
	else{
	
		if((dabtaddress >= 0x08000000) && (dabtaddress < 0x08000200)  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,gbaheaderbuf[(dabtaddress ^ 0x08000000)/4]);
			return gbaheaderbuf[(dabtaddress ^ 0x08000000)/4];
		}
	
		else if((dabtaddress >= 0x08000200) && (dabtaddress < (romsize+0x08000000))  ){
			//printf(" => data abt! (%x):%x ",dabtaddress,ichfly_readu32(dabtaddress ^ 0x08000000));
			return stream_readu32(dabtaddress ^ 0x08000000);
		}
	}
	*/

	printf("data abort exception!");
	exception_dump();
	while(1);	
	return 0;
}

u32 exceptreserv(){	
	return 0;
}

//NDS hardware IRQ process (it is triggered in BIOS NDS9 IRQ) also checks GBA IRQs

//returns: excluded served IF
u32 exceptirq(u32 ndsIE,u32 ndsIF,u32 sp_ptr){
	//process callbacks (IEregister & IFregister)
	switch(ndsIF & ndsIE){
		case(1<<0):	//LCD V-Blank
			vblank_thread();
		break;
		case(1<<1):	//LCD H-Blank
		break;
		case(1<<2)://LCD V-Counter Match
			
		break;
		case(1<<3)://Timer 0 Overflow
			
		break;
		case(1<<4)://Timer 1 Overflow
			
		break;
		case(1<<5)://Timer 2 Overflow
			
		break;
		case(1<<6)://Timer 3 Overflow
		
		break;
		/*//unused
		case(1<<7)://NDS7 only: SIO/RCNT/RTC (Real Time Clock)
		break;
		*/
		case(1<<8):// DMA 0
		break;
		case(1<<9):// DMA 1
			
		break;
		case(1<<10)://DMA 2
			
		break;
		case(1<<11)://DMA 3
			
		break;
		case(1<<12)://Keypad
			
		break;
		case(1<<13)://GBA-Slot (external IRQ source)
			
		break;
		/*
		case(1<<14): //unused
		break;
		
		case(1<<15): //unused
		break;
		*/
		case(1<<16): //IPC Sync
			//recvwordipc();
		break;
		case(1<<17): //IPC Send FIFO Empty
		
		break;
		case(1<<18): //IPC Recv FIFO Not Empty
		break;
		case(1<<19): // NDS-Slot Game Card Data Transfer Completion
			//printf("irq cart!");
		break;
		case(1<<20): //NDS-Slot Game Card IREQ_MC
		
		break;
		case(1<<21): //NDS9 only: Geometry Command FIFO
		
		break;
		/*
		//unused
		case(1<<22): //NDS7 only: Screens unfolding
		break;
		//unused
		case(1<<23): //NDS7 only: SPI bus
		break;
		//unused
		case(1<<24): //NDS7 only: Wifi
		break;
		*/
		default:	//nothing
		break;
	}

	//printf("GBA IRQ request! ");
	//2/2 GBA IRQ
	//process callbacks (IEregister & IFregister)
	switch(ndsIF & ndsIE){
		case(1<<0):	//LCD V-Blank
			ndsIF&=~(1<<0);
		break;
		case(1<<1):	//LCD H-Blank
			ndsIF&=~(1<<1);
		break;
		case(1<<2)://LCD V-Counter Match
			ndsIF&=~(1<<2);
		break;
		case(1<<3)://Timer 0 Overflow
			ndsIF&=~(1<<3);
		break;
		case(1<<4)://Timer 1 Overflow
			ndsIF&=~(1<<4);
		break;
		case(1<<5)://Timer 2 Overflow
			ndsIF&=~(1<<5);
		break;
		case(1<<6)://Timer 3 Overflow
			ndsIF&=~(1<<6);
		break;
		case(1<<7)://Serial IO / SIO Comms
			ndsIF&=~(1<<7);
		break;
		case(1<<8):// DMA 0
			ndsIF&=~(1<<8);
		break;
		case(1<<9):// DMA 1
			ndsIF&=~(1<<9);
		break;
		case(1<<10)://DMA 2
			ndsIF&=~(1<<10);
		break;
		case(1<<11)://DMA 3
			ndsIF&=~(1<<11);
		break;
		case(1<<12)://Keypad
			ndsIF&=~(1<<12);
		break;
		case(1<<13)://GBA-Slot (external IRQ source)
			ndsIF&=~(1<<13);
		break;
		default:	//nothing
		break;	
	}

	return ndsIF;
}

//bios handler, does not work with MPU vectors set to 0x00000000
void gbhandler(){
	//exceptionStack = (u32)0x23EFFFC;
	debug_vect = (intfuncptr)&inter_swi; //&enter_except; //correct way
	//*curr_exception=(u32)exceptdataabt; //add curr_exception later if you ever need it

	//printf("inter_swi(): (%x) @t 0x02FFFD9C ",(u32)&inter_swi);
}

void emulateedbiosstart(){
	cpu_SetCP15Cnt(cpu_GetCP15Cnt() &~BIT(13));
}

//Setup of MPU region & cache settings
void setgbamap(){
	
	//use the TGDS default MPU Map
	/*
	//update DTCM user reserved address
	//dtcm_end_alloced=(unsigned int)(&dtcm_end_alloced+0x2);

	nopinlasm();

	//correct PU setting way:
	//u32 reserved_dtcm = dtcm_end_alloced-getdtcmbase();
	
	//DC_FlushAll(); //coherent cache clean
	//IC_InvalidateAll(); //Instruction Cache Lockdown
	//printf("free DTCM cache: (%d)bytes",(int)( ((dtcm_top_ld-getdtcmbase()) - (0x400*4)) - reserved_dtcm) );
	
	nopinlasm();

	//IC_InvalidateAll(); //Instruction Cache Lockdown
	//DC_InvalidateAll(); //Data Cache lockdown
	
	nopinlasm();

	setdtcmbase(); //set dtcm base (see linker file)
	setitcmbase(); //set itcm base (see linker file)

	nopinlasm();

	cpu_SetCP15Cnt(cpu_GetCP15Cnt() & ~0x1); //Disable pu to configure it
	DC_InvalidateAll(); //Data Cache free access
	
	nopinlasm();

	//vector setup: reset,undefined,swi,prefetch_abort,data_abort,reserved,irq,fiq
	puhandlersetup(exceptreset,exceptundef,exceptswi,exceptprefabt,exceptdataabt,exceptreserv,exceptirq);

	nopinlasm();
	prepcache(); //enable (ON) cache DTCM / ITCM hardware mechanism
	nopinlasm();

	
	pu_SetRegion(0, 0x04000000 		| 	0x1fffff		|1); 	//protect IO 0x41fffff OK
	pu_SetRegion(1, getdtcmbase() 	|	0x00004000 		|1); 	//dtcm OK
	pu_SetRegion(2, 0x01001000 		|	0x00007000 		|1);	//itcm OK
	pu_SetRegion(3, (u32)&gbawram	| sizeof(gbawram)	|1);	//GBAWRAM OK
	pu_SetRegion(4, vector_addr 	|	(vector_end_addr-vector_addr) 		|1);	//vectors
	
	//printf("GBAWRAM size is: %x",(unsigned int)sizeof(gbawram));
	nopinlasm();

	//dtcm & itcm do not use write/cache buffers | IO ports included
	writebufenable(0b00001000); ////C3,C0,0. for dtcm, itcm is ro enable buffer-write on what regions (pu_GetWriteBufferability())
	dcacheenable(0b00001000);	//c2,c0,0 data tcm can access to what (pu_SetDataCachability())
	icacheenable(0b00001000); //c2,c0,1	instruction tcm can access to what (pu_SetCodeCachability())

	pu_SetCodePermissions(0x33363363); //instruction region access permissions to others - C5,C0,3 - access rights: 3 rw , 6 ro
	pu_SetDataPermissions(0x33363633); //data region access permission to others - c5, c0, 2 <-- could cause freezes if not properly set


	//drainwrite(); //update memory banks at cachable + noncachable memory
	nopinlasm();
	nopinlasm();

	//requires: 1) asm handlers 2) they are on itcm 3) call to C handlers for each asm handler
	#ifdef NONDS9HANDLERS
		printf("PU: set exception vec @0x00000000 ");
		emulateedbiosstart();
	#else
		printf("PU: set exception vec @0xffff0000 ");
		*(u32*)(0x027FFD9C)=(u32)&exception_dump;
	#endif

	pu_Enable(); //activate region settings	
	*/
}
