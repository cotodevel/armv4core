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

#ifndef __util_armv4core_h__
#define __util_armv4core_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BIT(n) (1 << (n))
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))	
#define alignw(n)(CHECK_BIT(n,0)==1?n+1:n)

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

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION  SAVE_GAME_VERSION_10

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

#define ARMModel (char*)("ARMv4t")

//struct: map[index].address[(u8*)address]
typedef struct {
  u8 *address;
  u32 mask;
} memoryMap;

typedef union {
  struct {
#ifdef WORDS_BIGENDIAN
    u8 B3;
    u8 B2;
    u8 B1;
    u8 B0;
#else
    u8 B0;
    u8 B1;
    u8 B2;
    u8 B3;
#endif
  } B;
  struct {
#ifdef WORDS_BIGENDIAN
    u16 W1;
    u16 W0;
#else
    u16 W0;
    u16 W1;
#endif
  } W;
#ifdef WORDS_BIGENDIAN
  volatile u32 I;
#else
	u32 I;
#endif
} reg_pair;

#define R13_IRQ  18
#define R14_IRQ  19
#define SPSR_IRQ 20
#define R13_USR  26
#define R14_USR  27
#define R13_SVC  28
#define R14_SVC  29
#define SPSR_SVC 30
#define R13_ABT  31
#define R14_ABT  32
#define SPSR_ABT 33
#define R13_UND  34
#define R14_UND  35
#define SPSR_UND 36
#define R8_FIQ   37
#define R9_FIQ   38
#define R10_FIQ  39
#define R11_FIQ  40
#define R12_FIQ  41
#define R13_FIQ  42
#define R14_FIQ  43
#define SPSR_FIQ 44

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern struct variable_desc savestr;
extern const  u8 minilut[0x10];
extern const u32  objtilesaddress [];
extern const u8  gamepakramwaitstate[];
extern const u8  gamepakwaitstate[];
extern const u8  gamepakwaitstate0[];
extern const u8  gamepakwaitstate1[];
extern const u8  gamepakwaitstate2[];
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);
extern void initemu();
extern int utilload(const char *file,u8 *data,int size,bool extram);
extern int loadrom(const char *filename,bool extram);
extern int i;
extern int utilReadInt2(FILE *);
extern int utilReadInt3(FILE *);
extern void utilGetBaseName(const char *,char *);
extern void utilGetBasePath(const char *, char *);
extern int LengthFromString(const char *);
extern int VolumeFromString(const char *);
extern int unzip(char *, void *, uLong);
extern int utilLoad(const char *file,u8 *data,int size,bool extram);
extern int setregbasesize(u32, u8);
extern u16 swap16(u16 value);
extern void initmemory(); //test memory & init
extern u32 dummycall(u32 arg);
extern void __libnds_mpu_setup();
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);
extern int utilload(const char *file,u8 *data,int size,bool extram);
extern int CPULoadRom(const char *szFile,bool extram);
extern int utilReadInt2(FILE *);
extern int utilReadInt3(FILE *);
extern void utilGetBaseName(const char *,char *);
extern void utilGetBasePath(const char *, char *);
extern int LengthFromString(const char *);
extern int VolumeFromString(const char *);
extern int unzip(char *, void *, uLong);
extern int utilLoad(const char *file,u8 *data,int size,bool extram);
extern int setregbasesize(u32, u8);
extern void u32store(u32 address, u32 value);
extern void u16store(u32 address, u16 value);
extern void u8store(u32 address, u8 value);
extern u32 u32read(u32 address);
extern u16 u16read(u32 address);
extern u8 u8read(u32 address);
extern u32 READ32LE(u8 * addr);
extern u16 READ16LE(u8 * addr);
extern void WRITE32LE(u8 * x,u32 v);
extern void WRITE16LE(u8 * x,u16 v);
extern u8	clzero(u32);
extern u32 gba_entrypoint;
extern void isdebugemu_defined();
extern int executeuntil_pc(u32 target_pc);
extern int ram2file_nds(char * fname,u8 * buffer,int size);
extern void CPUInit(const char *biosFileName, bool useBiosFile,bool extram);
extern int utilLoad(const char *file,u8 *data,int size,bool extram);
extern u32 gba_setup();
extern u32 UPDATE_REG(u32 address, u32 value);
extern struct gbaheader_t gbaheader;
extern void printGBACPU();

#ifdef __cplusplus
}
#endif