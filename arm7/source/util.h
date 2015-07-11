#include <nds.h>

/*
//divswi output
extern u32 divswiarr[0x3];
extern u32 bios_irqhandlerstub_C;
*/
#ifdef __cplusplus
extern "C" {
#endif

//lookup calls
u8 lutu16bitcnt(u16 x);
u8 lutu32bitcnt(u32 x);

//LUT table for fast-seeking 32bit depth unsigned values
extern u8 minilut[0x10];

//hardware environment Interruptable Bits for ARM7
//IF
extern u32	 nds_ifmasking;

//IE
extern u32	 nds_iemasking;

//IME
extern u32 	 nds_imemasking;

//CPSR from asm (hardware) cpu NZCV
extern u32 	 cpsrasm;

#ifdef __cplusplus
}
#endif