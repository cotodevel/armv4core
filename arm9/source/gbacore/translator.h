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

#ifndef __translator_armv4core_h__
#define __translator_armv4core_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "Util.h"
#include "fatfslayerTGDS.h"

#define CPUSTATE_ARM (u8)(0)
#define CPUSTATE_THUMB (u8)(1)


#endif

#ifdef __cplusplus
extern "C"{
#endif

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

//CPU flags will live on the DTCM , so they're accessible
extern u32 cpsrasm;	//gba hardware cpsr from asm opcodes
extern u8 z_flag;
extern u8 n_flag;
extern u8 c_flag;
extern u8 v_flag;
extern u8 i_flag;
extern u8 f_flag;

//inmediate flag set for ARM opcode (5.4 ARM)
extern u8 immop_arm;
	
//set/alter condition codes for ARM opcode (5.4 ARM)
extern u8 setcond_arm;
	
//SPSR == the last mode Interrupt, Fast interrupt, the old CPU flags (old stack [mode] && old CPU [mode]) 

extern u8 armstate;	//0 arm / 1 thumb
extern u8 armirqstate;//0 disabled / 1 enabled
extern u8 armswistate;//0 disabled / 1 enabled
extern u32 updatecpuflags(u8 mode, u32 cpsr, u32 cpumode); //updatecpuflags(mode,cpsr,cpumode); mode: 0 = hardware asm cpsr update / 1 = virtual CPU mode change,  CPSR , change to CPU mode

//disassemblers
extern u32 disthumbcode(u32 thumbinstr);
extern u32 disarmcode(u32 arminstr);

//thumb opcodes that require hardware CPSR bits
//5.1
extern u32 lslasm(u32 x1,u32 y1);
extern u32 lsrasm(u32 x1,u32 y1);
extern int asrasm(int x1, u32 y1);
//5.2
extern u32 addasm(u32 x1,u32 y1);
extern int addsasm(int x1,int y1); //re-uses addasm bit31 signed

extern u32 subasm(u32 x1,u32 y1);
extern int subsasm(int x1,int y1); //re-uses subasm bit31 signed
//5.3
extern u32 movasm(u32 reg);
extern u32 cmpasm(u32 x, u32 y); //r0,r1
//add & sub already added
//5.4
extern u32 andasm(u32 x1,u32 y1);
extern u32 eorasm(u32 x1,u32 y1);
//lsl, lsr, asr already added
extern u32 adcasm(u32 x1,u32 y1);
extern u32 sbcasm(u32 x1,u32 y1);
extern u32 rorasm(u32 x1,u32 y1);
extern u32 tstasm(u32 x1,u32 y1);
extern u32 negasm(u32 x1);
//cmp rd,rs already added
extern u32 cmnasm(u32 x1,u32 y1);
extern u32 orrasm(u32 x1,u32 y1);
extern u32 mulasm(u32 x1,u32 y1); //unsigned multiplier
extern u32 bicasm(u32 x1,u32 y1);
extern u32 mvnasm(u32 x1);
//5.5
//add & cmp (low & high reg) already added

//5.8
//ldsb
extern u32 ldsbasm(u32 x1,u32 y1);
extern u32 ldshasm(u32 x1,u32 y1);

//ARM opcodes
extern u32 rscasm(u32 x1,u32 y1);
extern u32 teqasm(u32 x1,u32 y1);
extern u32 rsbasm(u32 x1,u32 y1);

//5.6
extern u32 mlaasm(u32 x1,u32 y1, u32 y2); // z1 = ((x1 * y1) + y2) 
extern u32 mulsasm(u32 x1,u32 y1); //signed multiplier
extern u32 multtasm(u32 x1,u32 y1);
extern u32 mlavcsasm(u32 x1,u32 y1,u32 y2);

//ldr/str
extern u32 ldru32extasm(u32 x1,u32 y1);
extern u16 ldru16extasm(u32 x1,u16 y1);
extern u8	ldru8extasm(u32 x1,u8 y1);

#ifdef __cplusplus
}
#endif
