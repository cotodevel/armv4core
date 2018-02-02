#include <nds.h>
#include <fat.h>
#include <filesystem.h>
#include <dirent.h>
#include <unistd.h>    // for sbrk()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <nds/memory.h>
#include <nds/ndstypes.h>

#include "../zlib/zlib.h"
#include "../zlib/zip/unzip.h"

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


/*
video ram notes:
All VRAM (and Palette, and OAM) can be written to only in 16bit and 32bit units 
(STRH, STR opcodes), 8bit writes are ignored (by STRB opcode). 
The only exception is "Plain <ARM7>-CPU Access" mode: The ARM7 CPU can use STRB to write 
to VRAM (the reason for this special feature is that, in GBA mode, two 128K VRAM blocks 
are used to emulate the GBA's 256K Work RAM).
*/
//#define internalRAM ((u8*)0x03000000)	//iram //allocated to ARM9 and served as Shared WRAM 03000000-03007FFF (IRAM)
//#define workRAM ((u8*)0x02000000)		//wram //02000000-0203FFFF WRAM - On-board Work RAM (256 KBytes) 2 Wait
//#define paletteRAM ((u8*)0x05000000)	//pallette //matches gba 05000000-050003FF   BG/OBJ Palette RAM  (1 Kbyte)
//#define vram ((u8*)0x06000000)			//vram //matches gba VRAM - Video RAM (96 KBytes)
//#define emuloam ((u8*)0x07000000) 		//oam emulated memory 
//gba 04000000-040003FE I/O, address patches occur on the VBAEMU core


#ifndef GBA_H
#define GBA_H

/*
#define UPDATE_REG(address, value){\
	WRITE16LE(((u16 *)&gba->ioMem[address]),value);\
}\
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

#define ARMV4_PSR_BITS  0xf90f03ff

#define PSR_MODE    0x1f
#define PSR_USR 0x10
#define PSR_FIQ 0x11
#define PSR_IRQ 0x12
#define PSR_SVC 0x13
#define PSR_ABT 0x17
#define PSR_UND 0x1b
#define PSR_SYS 0x1f

//0xbb000000
#define base_factor 24

/*//original VBA
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
*/

//add gba here

#endif // GBA_H

//LUT table for fast-seeking 32bit depth unsigned values
extern const  u8 minilut[0x10];

//slot 2 bus 
extern const u32  objtilesaddress [];
extern const u8  gamepakramwaitstate[];
extern const u8  gamepakwaitstate[];
extern const u8  gamepakwaitstate0[];
extern const u8  gamepakwaitstate1[];
extern const u8  gamepakwaitstate2[];

#ifdef __cplusplus
extern "C" {
#endif

//lookup calls
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);

int utilload(const char *file,u8 *data,int size,bool extram);
int CPULoadRom(const char *szFile,bool extram);

int utilReadInt2(FILE *);
int utilReadInt3(FILE *);
void utilGetBaseName(const char *,char *);
void utilGetBasePath(const char *, char *);
int LengthFromString(const char *);
int VolumeFromString(const char *);
int unzip(char *, void *, uLong);
int utilLoad(const char *file,u8 *data,int size,bool extram);
int setregbasesize(u32, u8);
u32 bit(u32 val);

//more GBAVirt cpu opcodes
u16 swap16(u16 value);
void UPDATE_REG(u16 address, u16 value);

//direct memory reads
void u32store(u32 address, u32 value);
void u16store(u32 address, u16 value);
void u8store(u32 address, u8 value);

u32 u32read(u32 address);
u16 u16read(u32 address);
u8 u8read(u32 address);

u32 READ32LE(u8 * addr);
u16 READ16LE(u8 * addr);

void WRITE32LE(u8 * x,u32 v);
void WRITE16LE(u8 * x,u16 v);

u8	clzero(u32);

extern u32 gba_entrypoint;

//debug
extern void isdebugemu_defined();
extern bool setarmstate(u32 psr);  //set arm state and cpsr bits from any psr / ret false:ARM | true:THUMB

extern int executeuntil_pc(u32 target_pc);


extern int ram2file_nds(char * fname,u8 * buffer,int size);

extern void CPUInit(const char *biosFileName, bool useBiosFile,bool extram);
extern int utilLoad(const char *file,u8 *data,int size,bool extram);

#ifdef __cplusplus
}
#endif

