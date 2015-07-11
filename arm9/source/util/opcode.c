#include "opcode.h"

#include "gbacore.h"
#include "translator.h"

/*
//ldr/str (inline)
inline __attribute__((always_inline))
u32 ldru32inlasm(u32 x1){
u32 y1;
__asm__ volatile(
				"ldr %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u16 ldru16inlasm(u32 x1){
u16 y1;
__asm__ volatile(
				"ldrh %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u8 ldru8inlasm(u32 x1){
u8 y1;
__asm__ volatile(
				"ldrb %[y1],[%[x1]]""\n\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

inline __attribute__((always_inline))
u8 stru8inlasm(u32 x1,u32 x2,u8 y1){
u8 out;
__asm__ volatile(
				"strb %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u16 stru16inlasm(u32 x1,u32 x2,u16 y1){
u16 out;
__asm__ volatile(
				"strh %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

inline __attribute__((always_inline))
u32 stru32inlasm(u32 x1,u32 x2,u32 y1){
u32 out;
__asm__ volatile(
				"str %[y1],[%[x1],%[x2]]""\n\t"
				"ldr %[out],[%[x1],%[x2]]""\n\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

*/
inline __attribute__((always_inline))
u32 nopinlasm(){
__asm__ volatile(
				"nop""\n\t"
				: 	// = write only (value+=value) / + read/write with output / & output only reg
				:	//1st arg takes rw, 2nd and later ro
				:
				); 
return 0;
}

//works FINE
u32 u32store(u32 address, u32 value){
	*(u32*)(address)=value;
	return 0;
}

u16 u16store(u32 address, u16 value){
	*(u16*)(address)=value;
	return 0;
}

u8 u8store(u32 addr, u8 value){
	*(u8*)(addr)=value;
	return 0;
}

u32 u32read(u32 address){
	return *(u32*)(address);
}

u16 u16read(u32 address){
	return *(u16*)(address);
}

u8 u8read(u32 addr){
	return *(u8*)(addr);
}

//counts leading zeroes :)
u8 clzero(u32 var){
   
    u8 cnt=0;
    u32 var3;
    if (var>0xffffffff) return 0;
   
    var3=var; //copy
    var=0xFFFFFFFF-var;
    while((var>>cnt)&1){
        cnt++;
    }
    if ( (((var3&0xf0000000)>>28) >0x7) && (((var3&0xff000000)>>24)<0xf)){
        var=((var3&0xf0000000)>>28);
        var-=8; //bit 31 can't count to zero up to this point
            while(var&1) {
                cnt++; var=var>>1;
            }
    }
return cnt;
}

//debugging:

//0:ARM | 1:THUMB
u8 getarmstate(){

	//->switch to arm/thumb mode depending on cpsr for virtual env
	if( ((cpsrvirt>>5)&1) == 0x1 )
		armstate=0x1;
	else
		armstate=0x0;
		
	return armstate;
}

//0: no, it wasn't enabled when compiled.
//1: yes, it was enabled when compiled.
u8 was_debugemu_enabled=0;

//toggles if debugemu is specified. Run at startup ONLY.
void isdebugemu_defined(){

#ifdef DEBUGEMU
	was_debugemu_enabled=1;
#endif

}


//loops-executes the emulator until pc specified
int executeuntil_pc(int pc_chosen){

int opcodes_executed=0;
int cur_rom=0;

#ifdef DEBUGEMU
#undef DEBUGEMU
#endif

//execute until desired opcode:
while(pc_chosen != rom){

	cpu_fdexecute();
	
	if(cur_rom == rom){
		while(1){
			iprintf("this has looped\n ");
		}
	}
	/*
	//0:ARM | 1:THUMB
	if(getarmstate()==0){
		cur_rom = rom - 0x4;
	}
	else{
		cur_rom = rom - 0x2;
	}
	*/
	
	cur_rom = rom;
	
	opcodes_executed++;
}


if(was_debugemu_enabled==1){
	#define DEBUGEMU
}

return opcodes_executed;
}