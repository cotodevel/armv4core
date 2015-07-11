#include <nds/ndstypes.h>


#ifdef __cplusplus
extern "C"{
#endif
//direct memory reads
u32 u32store(u32 address, u32 value);
u16 u16store(u32 address, u16 value);
u8 u8store(u32 address, u8 value);

u32 u32read(u32 address);
u16 u16read(u32 address);
u8 u8read(u32 address);

u8	clzero(u32);
u32 nopinlasm();


//debug
void isdebugemu_defined();
u8 getarmstate();

int executeuntil_pc(int pc_chosen);

#ifdef __cplusplus
}
#endif