#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#ifdef __cplusplus
extern "C" {
#endif

u32 flashread();

void flashwrite(u32 address,u8 value);

#ifdef __cplusplus
}
#endif
