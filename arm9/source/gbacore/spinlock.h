#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifndef SPINLOCK_COTO
#define SPINLOCK_COTO
	
#define spinlock_elements (10)
#define spinlock_linearelements (spinlock_elements-1)

#define NULL_VAR 0

//spinlock example

typedef volatile struct{
	u8 spinlock_id;
	u8 status;	//0 unlocked | 1 locked
	u32 func_ptr;	
} spinlock_table;

typedef u32 (*u32cback_ptr)();

#endif

#ifdef __cplusplus
extern "C" {
#endif

extern u32 __attribute__ ((hot)) spinlock_sched();
extern u32 spinlock_perm(u8 process_id,u8 status);
extern u32 spinlock_createproc(u8 process_id,u8 status,u32cback_ptr new_callback);
extern u32 spinlock_modifyproc(u8 process_id,u8 status,u32cback_ptr new_callback);
extern u32 spinlock_deleteproc(u8 process_id);

extern volatile spinlock_table spinlock_handler[spinlock_elements];

#ifdef __cplusplus
}
#endif
