//arm9 main libs
#include <nds.h>
//This is Coto's Spinlock
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>//BRK(); SBRK();
#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
//#include <dswifi9.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>


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
