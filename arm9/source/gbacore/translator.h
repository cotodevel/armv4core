#include <nds/ndstypes.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>

#include ".\util.h"
#include ".\opcode.h"

#include "..\pu\pu.h"
#include "..\pu\supervisor.h"

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

#ifndef armv4_core
#define armv4_core
#endif

#ifdef __cplusplus
extern "C"{
#endif

//CPU flags will live on the DTCM , so they're accessible
extern u32 cpsrasm;	//gba hardware cpsr from asm opcodes
extern u32 cpsrvirt;	//gba virtualized cpsr for environment

//inmediate flag set for ARM opcode (5.4 ARM)
extern u8 immop_arm;

//SPSR == the last mode Interrupt, Fast interrupt, the old CPU flags (old stack [mode] && old CPU [mode]) 
extern u32 spsr_svc;
extern u32 spsr_irq;
extern u32 spsr_abt;
extern u32 spsr_und;
extern u32 spsr_fiq;
extern u32 spsr_usr;	//well, there's no spsr for user but for compatibility
extern u32 spsr_sys;
extern u32 spsr_last; //this one for any cpu<mode> SPSR handle


//u32  addresslookup(u32 srcaddr, u32 blacklist[], u32 whitelist[]);
u32 updatecpuflags(u8 mode, u32 cpsr, u32 cpumode); //updatecpuflags(mode,cpsr,cpumode); mode: 0 = hardware asm cpsr update / 1 = virtual CPU mode change,  CPSR , change to CPU mode

//disassemblers
u32 disthumbcode(u32 thumbinstr);
u32 disarmcode(u32 arminstr);

//thumb opcodes that require hardware CPSR bits
u32 lslasm(u32 x1,u32 y1);
u32 lsrasm(u32 x1,u32 y1);
int asrasm(int x1, u32 y1);
u32 addasm(u32 x1,u32 y1);
int addsasm(int x1,int y1); //re-uses addasm bit31 signed

u32 subasm(u32 x1,u32 y1);
int subsasm(int x1,int y1); //re-uses subasm bit31 signed

u32 movasm(u32 reg);
u32 cmpasm(u32 x, u32 y); //r0,r1
//add & sub already added
u32 andasm(u32 x1,u32 y1);
u32 eorasm(u32 x1,u32 y1);

u32 adcasm(u32 x1,u32 y1);
u32 sbcasm(u32 x1,u32 y1);
u32 rorasm(u32 x1,u32 y1);
u32 tstasm(u32 x1,u32 y1);
u32 cmnasm(u32 x1,u32 y1);
u32 orrasm(u32 x1,u32 y1);
u32 mulasm(u32 x1,u32 y1); //unsigned multiplier
u32 bicasm(u32 x1,u32 y1);
u32 mvnasm(u32 x1);
//add & cmp (low & high reg) already added

//ldsb
u32 ldsbasm(u32 x1,u32 y1);
u32 ldshasm(u32 x1,u32 y1);

//ARM opcodes
u32 rscasm(u32 x1,u32 y1);
u32 teqasm(u32 x1,u32 y1);
u32 rsbasm(u32 x1,u32 y1);

//multiplier
u32 mulasm(u32 rm,u32 rs);                              //Rd:=Rm(0)*Rs(1). Rn is ignored
u32 mlaasm(u32 rm,u32 rs, u32 rn);                      //Rd:=Rm(0)*Rs(1)+Rn(2)

void umullasm(u32 * rdlo,u32 * rdhi,u32 rm,u32 rs);     //RdLo(0),RdHi(1),Rm(2),Rs(3)
void smullasm(u32 * rdlo,u32 * rdhi,u32 rm,u32 rs);     //RdLo(0),RdHi(1),Rm(2),Rs(3)

void umlalasm(u32 * rdlo_out,u32 * rdhi_out,u32 rm,u32 rs,u32 rdlo_in,u32 rdhi_in);     //RdLo(0), RdHi(1) = Rm(2) * Rs(3) + RdLo(4), RdHi(5)
void smlalasm(u32 * rdlo_out,u32 * rdhi_out,u32 rm,u32 rs,u32 rdlo_in,u32 rdhi_in);     //RdLo(0), RdHi(1) = Rm(2) * Rs(3) + RdLo(4), RdHi(5)

//ldr/str
u32 ldru32extasm(u32 x1,u32 y1);
u16 ldru16extasm(u32 x1,u16 y1);
u8	ldru8extasm(u32 x1,u8 y1);

#ifdef __cplusplus
}
#endif