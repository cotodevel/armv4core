//arm9 main libs
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
//#include <dswifi9.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>

//NDS7 clock
//#include <time.h> 

//GBASystem struct declaration
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

//GBAProcess
u32 cpu_fdexecute();
u32 cpu_calculate();

int cpuupdateticks(int lcdtick_hw);
u32 cpuirq(u32 cpumode);
u32 __attribute__ ((hot)) dma_gbavirt(u32 source,u32 dest,u32 wordcnt,u16 control,u8 dma_ch);

//IO GBA (virtual < -- > hardware) handlers
u32 gbacpu_refreshvcount();								//CPUCompareVCOUNT(gba);
u16 cpu_updateregisters(u16 address, u16 value);		//CPUUpdateRegister(0xn, 0xnn);
u32 ndsvcounter();

//GBA chips emulation
#ifndef GPIOVIRT
#define GPIOVIRT
	//GPIO
	struct gpio_map{
		u16 io[3];
		u64 io_cmd[3];
	};
	struct gpio_map GPIOMAP;
#endif

extern struct gpio_map GPIOMAP;


//GBACore
extern u32  __attribute__((section(".dtcm"))) 		rom;			//current rom pointer
extern u32  __attribute__((section(".dtcm")))  	romsize;		//gba romsize loaded 
extern u32  __attribute__((section(".dtcm"))) * 	rom_entrypoint; //entrypoint
extern u32  __attribute__((section(".dtcm"))) 		gbachunk;		//gba read chunk
extern u32  __attribute__((section(".dtcm"))) 		dummyreg;		//any purpose destroyable 4 byte dtcm chunk
extern u32  __attribute__((section(".dtcm"))) 		dummyreg2;		//any purpose destroyable 4 byte dtcm chunk
extern u32  __attribute__((section(".dtcm"))) 		dummyreg3;		//any purpose destroyable 4 byte dtcm chunk
extern u32  __attribute__((section(".dtcm"))) 		dummyreg4;		//any purpose destroyable 4 byte dtcm chunk
extern u32  __attribute__((section(".dtcm"))) 		dummyreg5;		//any purpose destroyable 4 byte dtcm chunk
extern u32  __attribute__((section(".dtcm"))) 		bios_irqhandlerstub_C;	//irq handler word aligned pointer for ARM9

//GBA stack (on dtcm because fast)
extern u8 __attribute__((section(".dtcm"))) gbastck_usr[0x200];
extern u8 __attribute__((section(".dtcm"))) gbastck_fiq[0x200];
extern u8 __attribute__((section(".dtcm"))) gbastck_irq[0x200];
extern u8 __attribute__((section(".dtcm"))) gbastck_svc[0x200];
extern u8 __attribute__((section(".dtcm"))) gbastck_abt[0x200];
extern u8 __attribute__((section(".dtcm"))) gbastck_und[0x200];
//extern u8 __attribute__((section(".dtcm"))) gbastck_sys[0x200]; //stack shared with usr

//GBA stack base addresses
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_usr;
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_fiq;
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_irq;
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_svc;
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_abt;
extern u32 __attribute__((section(".dtcm"))) * gbastckadr_und;
//extern u32 __attribute__((section(".dtcm"))) * gbastckadr_sys;	//stack shared with usr

//GBA stack address frame pointer (or offset pointer)
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_usr;
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_fiq;
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_irq;
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_svc;
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_abt;
extern u32 __attribute__((section(".dtcm"))) * gbastckfp_und;
//extern u32 __attribute__((section(".dtcm"))) * gbastckfp_sys; //shared with usr

//SPSR slot for GBA stack frame pointer (single cpu<mode> to framepointer operations)
extern u32 __attribute__((section(".dtcm"))) gbastckfpadr_spsr;

//current slot for GBA stack <mode> base pointer
extern u32 __attribute__((section(".dtcm"))) * gbastckmodeadr_curr;

//current slot for GBA stack frame pointer
extern u32 __attribute__((section(".dtcm"))) * gbastckfpadr_curr;

//gba virtualized r0-r14 registers
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_cpu[0x10]; //placeholder for actual CPU mode registers

//each sp,lr for cpu<mode>
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13usr[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14usr[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13fiq[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14fiq[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13irq[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14irq[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13svc[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14svc[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13abt[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14abt[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r13und[0x1];
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14und[0x1];

//extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1]; //usr/sys uses same stacks
//extern u32  __attribute__((section(".dtcm"))) gbavirtreg_r14sys[0x1];

//and FIQ(32) which is r8-r12
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_fiq[0x5];

//and cpu<mode> all the other backup registers when restoring from FIQ r8-r12
extern u32  __attribute__((section(".dtcm"))) gbavirtreg_cpubup[0x5];



//branchslot calls (up to 32 groups of * 17 elements * 4 depth size (u32) addresses)
//for cpu save working registers when branch mode enters
//////////////////////////////////////////////////////////////////////////////////////
//this is for storing current CPU mode fully when branching calls from the virtualizer
extern u32  __attribute__((section(".dtcm"))) branch_stack[17*32]; //32 slots

//branch framepointer
extern u32 * __attribute__((section(".dtcm"))) branch_stackfp;
//////////////////////////////////////////////////////////////////////////////////////


////////branch address returning values for when entering//////////
//branch address stack for LR (PC+ (opcodes to be executed * 4)) addresses
extern u32  __attribute__((section(".dtcm"))) branch_adrstack[0x10];

//branch address stack framepointer
extern u32 * __attribute__((section(".dtcm"))) branch_adrstack_fp;
///////////////////////////////////////////////////////////////////////////////////////


//extern'd addresses for virtualizer calls
// [(u32)&disthumbcode	] 
// [(u32)&disasmcode	]
// [					]
// [					]
// [					]
// [					]
// [					]
// [					]
extern u32  __attribute__((section(".dtcm"))) call_adrstack[0x10];

//GBA Interrupts
//4000200h - IE - Interrupt Enable Register (R/W)
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

//virtual environment Interruptable Bits

//CPUtotalticker
extern u32  __attribute__((section(".dtcm"))) cputotalticks;

//IF
extern u32  /*__attribute__((section(".dtcm")))*/ gbavirt_ifmasking;

//IE
extern u32  /*__attribute__((section(".dtcm")))*/ gbavirt_iemasking;

//IME
extern u32  /*__attribute__((section(".dtcm")))*/ gbavirt_imemasking;

//hardware environment Interruptable Bits for ARM9
//IF
extern u32  /*__attribute__((section(".dtcm")))*/ nds_ifmasking;

//IE
extern u32  /*__attribute__((section(".dtcm")))*/ nds_iemasking;

//IME
extern u32  /*__attribute__((section(".dtcm")))*/ nds_imemasking;

extern u32  __attribute__((section(".dtcm"))) hblankdelta;

extern u32  __attribute__((section(".dtcm"))) nds_vcount;

//thread vectors (for ASM irq)

//address whitelist to be patched
extern u32  __attribute__((section(".dtcm"))) addrpatches[0x10];

//new redirected patched addresses (requires init first after initemu() as pointers are generated there)
extern u32  __attribute__((section(".dtcm"))) addrfixes[0x10];

//lookup address tables
extern u32 __attribute__ ((hot)) addresslookup(u32 srcaddr, u32 blacklist[], u32 whitelist[]);

//input
u32 systemreadjoypad(int which);

//fetch
u32 armnextpc(u32 address);
u32 armfetchpc_arm(u32 address);
u16 armfetchpc_thumb(u32 address);


//swi
u32 swi_virt(u32 swinum);

//u32 gbaboot(u32); //main jumper
u32 thumbcode(u32); //jump to thumbmode gba
u32 armcode(u32); //jump to thumbmode gba

//CPU GBA:

//direct GBA CPU reads
u32 virtread_word(u32 address);
u16 virtread_hword(u32 address);
u8 virtread_byte(u32 address);

////direct GBA CPU writes
u32 virtwrite_word(u32 address,u32 data);
u16 virtwrite_hword(u32 address,u16 data);
u8 virtwrite_byte(u32 address,u8 data);

//GBA addressing hamming weight approach fast
u8 	cpuread_bytefast(u8 address); 		//CPUReadByteQuick(gba, addr)
u16 cpuread_hwordfast(u16 address);		//CPUReadHalfWordQuick(gba, addr)
u32 cpuread_wordfast(u32 address);		//CPUReadMemoryQuick(gba, addr)

//process list GBA load
//old:	CPUReadByte(GBASystem *gba, u32 address);
u8 __attribute__ ((hot)) cpuread_byte(u32 address);
//old:	CPUReadHalfWord(GBASystem *gba, u32 address)
u16 cpuread_hword(u32 address);
//old:	CPUReadMemory(GBASystem *gba, u32 address)
u32 cpuread_word(u32 address);	

//process list GBA writes
//old: CPUWriteByte(GBASystem *gba, u32 address, u8 b)
u32 cpuwrite_byte(u32 address,u8 b);
//old: CPUWriteHalfWord(GBASystem *gba, u32 address, u16 value)
u32 cpuwrite_hword(u32 address, u16 value);
//old: CPUWriteMemory(GBASystem *gba, u32 address, u32 value)
u32 cpuwrite_word(u32 address, u32 value);


#ifdef __cplusplus
}
#endif
