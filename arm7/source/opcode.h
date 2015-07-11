#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C"{
#endif

//shared variables
extern int var1;


/*
//ldr/str inline
u32 ldru32inlasm(u32 x1);
u32 stru32inlasm(u32 x1,u32 y1);
*/

u8	clzero(u32);

/*
//asm opcodes
int sqrtasm(int);
u32 wramtstasm(u32 address,u32 top);
u32 branchtoaddr(u32 value,u32 address);
*/

#ifdef __cplusplus
}
#endif