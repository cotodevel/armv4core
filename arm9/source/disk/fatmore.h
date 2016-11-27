#ifndef GBAEMU4DSSTRM
#define GBAEMU4DSSTRM

#include <nds.h>
#include <fat.h>

//Prototypes for ichfly's extended FAT driver
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fatfile.h" //required for linking FILE_STRUCT and other FAT related initializers

//extra settings for ownfilebuffer
#define sectorscale 1 //1,2,4,8
#define buffslots 255
#define sectorsize 0x200*sectorscale
//#define sectorbinsz clzero(sectorsize)

#define u32size (sizeof(u32))
#define u16size (sizeof(u16))
#define u8size (sizeof(u8))

#define sectorsize_u32units ((sectorsize/u32size)-1) //start from zero
#define sectorsize_u16units ((sectorsize/u16size)-1)
#define sectorsize_u8units ((sectorsize/u8size)-1)

#define sectorsize_int32units (sectorsize/u32size) 	//start from real number 1
#define sectorsize_int16units (sectorsize/u16size) 	//start from real number 1
#define sectorsize_int8units (sectorsize/u8size) 	//start from real number 1

#define strm_buf_size (int)(1024*4)

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern FILE_STRUCT* file_ext;

extern FILE* ichflyfilestream;
extern volatile int ichflyfilestreamsize;

extern __attribute__((section(".itcm"))) 	u8 ichfly_readu8extern(int pos);
extern __attribute__((section(".itcm"))) 	u16 ichfly_readu16extern(int pos);
extern __attribute__((section(".itcm"))) 	u32 ichfly_readu32extern(int pos);

extern __attribute__((section(".itcm")))  	u8 ichfly_readu8(int pos);
extern __attribute__((section(".itcm")))  	u16 ichfly_readu16(int pos);
extern __attribute__((section(".itcm")))  	u32 ichfly_readu32(int pos);
extern __attribute__((section(".itcm")))	void ichfly_readdma_rom(u32 pos,u8 *ptr,u32 c,int readal);

extern void closegbarom();
extern void getandpatchmap(u32 offsetgba,u32 offsetthisfile);

extern volatile  u32 *sectortabel;
extern void * lastopen;
extern void * lastopenlocked;

extern PARTITION* partitionlocked;
extern FN_MEDIUM_READSECTORS	readSectorslocked;
extern u32 current_pointer;
extern u32 allocedfild[buffslots];
extern u8* greatownfilebuffer;


extern void generatefilemap(int size);

//[nds] disk sector ram buffer
extern volatile u32 disk_buf32[(strm_buf_size)]; 	//32 reads //0x80
extern volatile u16 disk_buf16[(strm_buf_size)];	//16 reads
extern volatile u8 disk_buf8[(strm_buf_size)];		//16 reads

extern int startoffset32;	//start offset for disk_buf32 ,from gbabuffer fetch
extern int startoffset16;	//start offset for disk_buf32 ,from gbabuffer fetch
extern int startoffset8;	//start offset for disk_buf32 ,from gbabuffer fetch

extern u8 cache_read8(int offset);
extern u16 cache_read16(int offset);
extern u32 cache_read32(int offset);

extern u8 first32krom[1024*32];

#ifdef __cplusplus
}
#endif
