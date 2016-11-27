//arm9 main libs
#include <nds.h>
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

#include "..\main.h"

#include "spinlock.h"

u32cback_ptr cb_handler; //global callbacku32handler
int counter = 0; //counter on hblank
volatile spinlock_table spinlock_handler[spinlock_elements];

//spinlock create process | spinlock_createproc(u8 process_id,u8 status,u32cback_ptr new_callback)
//returns 0 if created | returns 1 if already exists (failure)
u32 spinlock_createproc(u8 process_id,u8 status,u32cback_ptr new_callback){
	if(!spinlock_handler[process_id].spinlock_id){
		spinlock_handler[process_id].spinlock_id = process_id;
		spinlock_handler[process_id].status = status;
		spinlock_handler[process_id].func_ptr = (u32)new_callback;
	}
	else
		return 1;
return 0;
}

//Modifies an existing spinlock process
//returns 0 if created | returns 1 if already exists (failure)
u32 spinlock_modifyproc(u8 process_id,u8 status,u32cback_ptr new_callback){
	if(spinlock_handler[process_id].spinlock_id){
		//spinlock_handler[process_id].spinlock_id = process_id; //Modify keeps the actual index/ID
		spinlock_handler[process_id].status = status;
		spinlock_handler[process_id].func_ptr = (u32)new_callback;
	}
	else
		return 1;
return 0;
}

//Deletes an existing spinlock process
//returns 0 if created | returns 1 if already exists (failure)
u32 spinlock_deleteproc(u8 process_id){
	if(spinlock_handler[process_id].spinlock_id){
		spinlock_handler[process_id].spinlock_id = 	(u32)NULL_VAR;
		spinlock_handler[process_id].status = 		(u32)NULL_VAR;
		spinlock_handler[process_id].func_ptr = 	(u32)NULL_VAR;
	}
	else
		return 1;
return 0;
}


//spinlock scheduler //executes all threads set to unlock
u32 __attribute__ ((hot)) spinlock_sched(){
	
	switch(spinlock_handler[counter].status){
		case(0):
			cb_handler = (u32cback_ptr) spinlock_handler[counter].func_ptr;
			//spinlock_handler[counter].status=1;
			cb_handler();
		break;
		default:
			counter++; //if this thread isn't runnable index++
		break;
	}
	
	if (counter>((int)spinlock_linearelements))
		counter=0;
	
return 0;
}

//spinlock permission enable/disable
//returns 0 if set permission | returns 1 if process don't exists (failure)
u32 spinlock_perm(u8 process_id,u8 status){
	if(spinlock_handler[process_id].spinlock_id){
		spinlock_handler[process_id].status=status;
		return 0;
	}
	else
		return 1;

return 1;
}
