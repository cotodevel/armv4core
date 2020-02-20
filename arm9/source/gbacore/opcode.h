#ifndef opcodegbaarmcore
#define opcodegbaarmcore

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#endif

#ifdef __cplusplus
extern "C"{
#endif

extern u32 nopinlasm();

//extract_word
extern int extract_word(u32 * buffer,u32 word ,u32 * buffer_out,u32 delimiter,u8 type);
extern void nds_2_gba_copy(u8 * source,u8 * dest,int size);
extern void gba_2_nds_copy(u8 * source,u8 * dest,int size);

#ifdef __cplusplus
}
#endif