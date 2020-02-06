#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <dirent.h>
#include <unistd.h>    // for sbrk()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include "gba.arm.core.h"

//PU setup all NDS region addresses
#define debug_vect	(*(intfuncptr *)(0x02FFFD9C)) //ori: #define EXCEPTION_VECTOR	(*(VoidFn *)(0x2FFFD9C))
#define except_vect	(*(void *)(0x00000000)) //0x00000000 or 0xffff0000

//These are for GBA emu

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

//THIS IS EXTREMELY IMPORTANT! TO DEBUG !!
//27FFD9Ch - RAM - NDS9 Debug Stacktop / Debug Vector (0=None)
//380FFDCh - RAM - NDS7 Debug Stacktop / Debug Vector (0=None)
//These addresses contain a 32bit pointer to the Debug Handler, and, memory below of the addresses is used as Debug Stack. 
//The debug handler is called on undefined instruction exceptions, on data/prefetch aborts (caused by the protection unit), 
//on FIQ (possibly caused by hardware debuggers). It is also called by accidental software-jumps to the reset vector, 
//and by unused SWI numbers within range 0..1Fh.


/* EXCEPTION VECTOR TRIGGERS
Exception		/Offset from vector base		/Mode on entry		/A bit on entry		/F bit on entry		/I bit on entry
Reset			0x00							Supervisor			Disabled			Disabled			Disabled

Undefined 
instruction		0x04							Undefined			Unchanged			Unchanged			Disabled

Software 
interrupt		0x08							Supervisor			Unchanged			Unchanged			Disabled

Prefetch 
Abort			0x0C							Abort				Disabled			Unchanged			Disabled

Data Abort		0x10							Abort				Disabled			Unchanged			Disabled

Reserved		0x14							Reserved			-					-					-

IRQ				0x18							IRQ					Disabled			Unchanged			Disabled

FIQ				0x1C							FIQ					Disabled			Disabled			Disabled

*/

//exception vectors:
//:FFFF0000             _reset_vector 
//:FFFF0004             _undefined_vector 
//:FFFF0008             _swi_vector
//:FFFF000C             _prefetch_abort_vector 
//:FFFF0010             _data_abort_vector 
//:FFFF0014             _reserved_vector 
//:FFFF0018             _irq_vector
//:FFFF001C             _fiq_vector

//THESE MEMORY AREAS AREN'T ON PU DEFAULT SETTINGS!
//Any r/w beyond 0x02xxxxxx will cause CPU to change to ABT mode + unresolved PU address 
//#define ioMem caioMem //hack todo
//GBA Slot ROM 08000000h (max. 32MB) parts of gba code to execute is mapped here with various tricks

#define DISPCAPCNT (*(vu32*)0x4000064)
//BIOS_GBA wrapper nds<->   gba bios 00000000-00003FFF BIOS - System ROM (16 KBytes)
//GBA & NDS stacks are assigned to Data TCM (accesable only in ARM9, through CP15 mcr / mrc)

//08000000-09FFFFFF Game Pak ROM/FlashROM (max 32MB) - Wait State 0
//0A000000-0BFFFFFF Game Pak ROM/FlashROM (max 32MB) - Wait State 1
//0C000000-0DFFFFFF Game Pak ROM/FlashROM (max 32MB) - Wait State 2

/*	Region 0 - background	0x00000000 PU_PAGE_128M
	Region 1 - DTCM 0x0b000000 PAGE_16K   
	Region 2 - speedupone 0x02040000 PU_PAGE_256K
	Region 3 - speeduptwo 0x02080000 PU_PAGE_512K
	Region 4 - speedupthree 0x02100000 PU_PAGE_1M
	Region 5 - speedupfour 0x02200000 PU_PAGE_2M
	Region 6 - ITCM protector 0x00000000 PU_PAGE_16M
	Region 7 - IO 0x04000000 PU_PAGE_16M
*/

//Protection Unit DCACHE / ICACHE setup
#define PU_PAGE_4K		(0x0B << 1)
#define PU_PAGE_8K		(0x0C << 1)
#define PU_PAGE_16K		(0x0D << 1)
#define PU_PAGE_32K		(0x0E << 1)
#define PU_PAGE_64K		(0x0F << 1)
#define PU_PAGE_128K		(0x10 << 1)
#define PU_PAGE_256K		(0x11 << 1)
#define PU_PAGE_512K		(0x12 << 1)
#define PU_PAGE_1M		(0x13 << 1)
#define PU_PAGE_2M		(0x14 << 1)
#define PU_PAGE_4M		(0x15 << 1)
#define PU_PAGE_8M		(0x16 << 1)
#define PU_PAGE_16M		(0x17 << 1)
#define PU_PAGE_32M		(0x18 << 1)
#define PU_PAGE_64M		(0x19 << 1)
#define PU_PAGE_128M		(0x1A << 1)
#define PU_PAGE_256M		(0x1B << 1)
#define PU_PAGE_512M		(0x1C << 1)
#define PU_PAGE_1G		(0x1D << 1)
#define PU_PAGE_2G		(0x1E << 1)
#define PU_PAGE_4G		(0x1F << 1)

//debug dump PU format:
//0x3003db4 	80a548a 	153123
//PU address 	value held 	times accessed PU address

#ifdef __cplusplus
extern "C" {
#endif

extern void gbamode();
extern void ndsmode();
extern void emulateedbiosstart();
extern int cpuGetCPSR();

//MPU & other
extern int inter_swi(int);	
extern int cpuGetSPSR();

//C vector exceptions
extern u32 exceptswi(u32); 		//swi vector
extern u32 exceptundef(u32 undef);	//undefined vector
extern u32 exceptirq(u32 nds_iemask,u32 nds_ifmask,u32 sp_ptr);
extern u32 swicaller(u32 arg);
extern void exception_dump();

#ifdef __cplusplus
}
#endif