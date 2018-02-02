#ifndef opcodegbaarmcore
#define opcodegbaarmcore

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#define gzFile void*

#endif

#ifdef __cplusplus
extern "C"{
#endif

extern u32 nopinlasm();

//extract_word
extern int extract_word(u32 * buffer,u32 word ,u32 * buffer_out,u32 delimiter,u8 type);

extern void nds_2_gba_copy(u8 * source,u8 * dest,int size);
extern void gba_2_nds_copy(u8 * source,u8 * dest,int size);

//translator opcode helpers
extern u32 BASE_ADDR(u32 addr);
extern s32 arm_do_branch(u32 arm_opcode);
extern void arm_alu_opcode(u32 arm_opcode);

#ifdef __cplusplus
}
#endif