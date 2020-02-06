#ifndef __gbaemu4dsfatext_h__
#define __gbaemu4dsfatext_h__

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "fatfslayerTGDS.h"

//functions taken from ichflysettings.h from gbaemu4ds
#define chucksizeinsec 1 //1,2,4,8
#define buffslots 255
#define chucksize 0x200*chucksizeinsec

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* ichflyfilestream;
extern int ichflyfilestreamsize;

//part of fatfile.c
extern u32 *sectortabel;
extern void * lastopen;
extern void * lastopenlocked;
extern int PosixStructFDLastLoadFile;	//Coto: a StructFD non-posix file descriptor having the handle for the FILE * opened at the time the sector table map was generated.
										//TGDS uses this to access the fatfs level file attributes
extern u32 current_pointer;
extern u32 allocedfild[buffslots];
extern u8* greatownfilebuffer;

extern void generatefilemap(FILE * f,int size);
extern void getandpatchmap(int offsetgba,int offsetthisfile,FILE * fhandle);

//part of gbaemu4ds_fat_ext.cpp

extern u8 ichfly_readu8(unsigned int pos);
extern u16 ichfly_readu16(unsigned int pos);
extern u32 ichfly_readu32(unsigned int pos);
extern void ichfly_readdma_rom(u32 pos,u8 *ptr,u32 c,int readal);

extern void testGBAEMU4DSFSTGDS(FILE * f,sint32 fileSize);





extern u8 stream_readu8(u32 pos);
extern u16 stream_readu16(u32 pos);
extern u32 stream_readu32(u32 pos);
extern FILE * gbaromfile;
extern u32 isgbaopen(FILE * gbahandler);
extern u32 opengbarom(const char * filename,const char * access_type);
extern u32 closegbarom();
extern u32 readu32gbarom(int offset);
extern u16 writeu16gbarom(int offset,u16 * buf_in,int size_elem);
extern u32 writeu32gbarom(int offset,u32 * buf_in,int size_elem);
extern u32 getfilesizegbarom();

#ifdef __cplusplus
}
#endif


