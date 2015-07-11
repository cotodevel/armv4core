#include "util.h"
#include "opcode.h"
#include "..\arm9.h"
#include "..\disk\stream_disk.h"
#include "..\pu\pu.h"
#include "..\pu\supervisor.h"
#include ".\gbacore.h"
#include ".\buffer.h"
#include ".\translator.h"	//for patches setup
#include "..\disk\file_browse.h"
#include "..\settings.h"

int i=0;

//VA + ( DTCMTOP - (stack_size)*4) - dtcm_reservedcode[end_of_usedDTCMstorage] (we use for the emu)  in a loop of CACHE_LINE size

// Declare the externs
//extern struct gbaheader_t gbaheader;

// returns unary(decimal) ammount of bits using the Hamming Weight approach 

//8 bit depth Lookuptable 
//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
const u8 /* __attribute__((section(".dtcm"))) */ minilut[0x10] = {
	0,	1,	1,	2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,		//0n
};

u8 lutu16bitcnt(u16 x){
	return (minilut[x &0xf] + minilut[(x>>4) &0xf] + minilut[(x>>8) &0xf] + minilut[(x>>12) &0xf]);
}

u8 lutu32bitcnt(u32 x){
	return (lutu16bitcnt(x & 0xffff) + lutu16bitcnt(x >> 16));
}


//count bits set in u32 field and return u32 decimal #imm
//iprintf("\n ammount of register bytes: %x",(unsigned int)lookupu16(thumbinstr&0xff));
//u32 temp=0x00000000;
//iprintf("lut:%d",lutu32bitcnt(temp));	//(thumbinstr&0xff)

const u32  objtilesaddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakramwaitstate[4] = { 4, 3, 2, 8 };
const u8 gamepakwaitstate[4] =  { 4, 3, 2, 8 };
const u8 gamepakwaitstate0[2] = { 2, 1 };
const u8 gamepakwaitstate1[2] = { 4, 1 };
const u8 gamepakwaitstate2[2] = { 8, 1 };
const bool isInRom [16]=
  { false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, false, false };


// swaps a 16-bit value
inline u16 swap16(u16 v){
	return (v<<8)|(v>>8);
}

//this was as macro... prevent any macro "expansions"..
u32 WRITE16LE(u32 x,u16 v){
	//*((u16 *)x) = swap16((v));
	u16store(x, swap16((v)));
return 0;
}

//Updates IO MAP!
u32 UPDATE_REG(u32 address, u32 value){
	//WRITE16LE((u32)&gba.iomem[address],(u16)value);
	u16store( ((u32)gba.iomem) + address, value);
	//iprintf("read value @ (%x):[%x] \n",(unsigned int)((u32)gba.iomem+address),(unsigned int)u32read((u32)gba.iomem,address));
return 0;
}

//Reads from IO map!
u32 READ_IOREG32(u32 address){
	return u32read((u32)gba.iomem + address);
}

u16 READ_IOREG16(u32 address){
	return u16read((u32)gba.iomem + address);
}

u8 READ_IOREG8(u32 address){
	return u8read((u32)gba.iomem + address);
}
//////////////////////////////////////////////////ALL KIND OF STACKING METHODS THAT AREN'T REALLY FOR GBA BUT GLOBAL stacking for testing//////////

u32 * cpubackupmode(u32 * branch_stackfp, u32 cpuregvector[0x16], u32 cpsr){

	//if frame pointer reaches TOP don't increase SP anymore
	// (ptrpos - ptrsrc) * int depth < = ptr + size struct (minus last element bc offset 0 uses an element)
	if( (int)( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4) <  (int)(gba_branch_table_size - gba_branch_elemnt_size)){ 
	
	//iprintf("gba_branch_offst[%x] +",( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4)); //debug
	
		stmiavirt((u8*)(&cpuregvector[0]), (u32)(u32*)branch_stackfp, 0xffff, 32, 0, 0);
		
		//for(i=0;i<16;i++){
		//	iprintf(">>%d:[%x]",i,cpuregvector[i]);
		//}
		//while(1);
		
		//iprintf("\n \n \n \n 	--	\n");
		//move 16 bytes ahead fp and store cpsr
		//debug: stmiavirt((u8*)cpsr, (u32)(u32*)(branch_stackfp+0x10), 0x1 , 32, 0);
		
		//iprintf("curr_used fp:%x / top: %x / block size: %x \n",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
		branch_stackfp=(u32*)addasm((u32)(u32*)branch_stackfp,(u32)0x10*4);
		
		//save cpsr int SPSR_mode slot
		switch(cpsr&0x1f){
			
			case(0x10): //user
				spsr_usr=cpsr;
			break;
			case(0x11): //fiq
				spsr_fiq=cpsr;
			break;
			case(0x12): //irq
				spsr_irq=cpsr;
			break;
			case(0x13): //svc
				spsr_svc=cpsr;
			break;
			case(0x17): //abt
				spsr_abt=cpsr;
			break;
			case(0x1b): //und
				spsr_und=cpsr;
			break;
			case(0x1f): //sys
				spsr_sys=cpsr;
			break;
		}
		//and save spsr this way for fast spsr -> cpsr restore (only for cpusave/restore modes), otherwise save CPSR manually
		gbastckfpadr_spsr=cpsr;
	}
	
	else{ 
		//iprintf("branch stack OVERFLOW! \n"); //debug
	}

return branch_stackfp;

}
//this one restores cpsrvirt by itself (restores CPU mode)
u32 * cpurestoremode(u32 * branch_stackfp, u32 cpuregvector[0x16]){

	//if frame pointer reaches bottom don't decrease SP anymore:
	//for near bottom case
	if ( (u32*)branch_stackfp > (u32*)&branch_stack[0]) { 
	
	//iprintf("gba_branch_block_size[%x] -",gba_branch_block_size); //debug

		//iprintf("curr_used fp:%x / top: %x / block size: %x \n",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
		branch_stackfp=(u32*)subasm((u32)(u32*)branch_stackfp,0x10*4);
		
		ldmiavirt((u8*)(&cpuregvector[0x0]), (u32)(u32*)(branch_stackfp), 0xffff, 32, 0, 0);
		
	}
	
	//if frame pointer reaches bottom don't decrease SP anymore:
	//for bottom case
	else if ( (u32*)branch_stackfp == (u32*)&branch_stack[0]) { 

		//move 4 slots behinh fp (32 bit *) and restore cpsr
		ldmiavirt((u8*)(&cpuregvector[0]), (u32)(u32*)(branch_stackfp), 0xffff, 32, 0, 0);
		
		//don't decrease frame pointer anymore.
		
	}
	
	else{ 
		//iprintf("branch stack has reached bottom! ");
	}
	
return branch_stackfp;
}

//data stack manipulation calls

//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 
// 0 = for CPU swap opcodes (cpurestore/cpubackup) modes
// 1 = for all LDMIAVIRT/STMIAVIRT OPCODES
// 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending

//1) create a call that retrieves data from thumb SP GBA
u32 __attribute__ ((hot)) ldmiavirt(u8 * output_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped,u8 order){ //access 8 16 or 32

int cnt=0;
int offset=0;

//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order

switch(byteswapped){
case(0): 	
	while(cnt<16){
		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
	
			if (access == (u8)8){
			
			}
			else if (access == (u8)16){
			
			}
			else if (access == (u8)32){
				if ((order&1) == 0){
					*((u32*)output_buf+(offset))=*((u32*)stackptr+(cnt)); // *(level_addr[0] + offset) //where offset is index depth of (u32*) / OLD
					//iprintf(" ldmia:%x[%x]",offset,*((u32*)stackptr+(cnt)));
				}
				else{
					*((u32*)output_buf-(offset))=*((u32*)stackptr-(cnt));
				}
			}
		offset++; //linear offset
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			//iprintf("restored %x to PC! ",*((u32*)stackptr+(offset)));
			
			if ((order&1) == 0)
				*((u32*)output_buf+(offset))=rom;
			
			else
				*((u32*)output_buf-(offset))=rom;
		}
	
	cnt++; //non linear register offset
	}
break;

//if you come here again because values aren't r or w, make sure you actually copy them from the register list to be copied
//this can't be modified as breaks ldmia gbavirtreg handles (read/write). SPECIFICALLY DONE FOR gbavirtreg[0] READS. don't use it FOR ANYTHING ELSE.

case(1):
	while(cnt<16) {
		if((regs>>cnt) & 0x1){
		//iprintf("%x ",cnt);
		
		if (access == (u8)8)
			*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset));
		
		else if (access == (u8)16){
			//iprintf("%x ",cnt);
			//iprintf("%x ",(u32*)stackptr);
			//for(i=0;i<16;i++){
			//iprintf("%x ", *((u32*)input_buf+i*4));
			//}
			//iprintf("%x ",cnt);
			//iprintf("ldmia:%d[%x] \n", offset,*((u32*)stackptr+(offset)));
			*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
		}	
		
		else if (access == (u8)32){
			
			if ((order&1) == 0){
				//iprintf("%x ",cnt);
				//iprintf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//iprintf("%x ", *((u32*)stackptr+i*4));
				//}
				//iprintf("%x ",cnt);
				//iprintf("ldmia:%d[%x] \n", offset,*((u32*)stackptr+(cnt)));
				*((u32*)output_buf+(offset))= *((u32*)stackptr+(cnt));
			}
			else{
				*((u32*)output_buf-(offset))= *((u32*)stackptr-(cnt));
			}
		}
			
		offset++;
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (PUSH-POP MANAGEMENT)
case (2):

	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
			//iprintf("ldmia virt: %x ",cnt);
		
			if (access == (u8)8)
				*((u8*)output_buf+(cnt))= *((u8*)stackptr+(offset)) ;
		
			else if (access == (u8)16){
				//iprintf("%x ",cnt);
				//iprintf("%x ",(u32*)stackptr);
				//for(i=0;i<16;i++){
				//iprintf("%x ", *((u32*)input_buf+i*4));
				//}
				//iprintf("%x ",cnt);
					*((u16*)output_buf+(cnt))= *((u16*)stackptr+(offset));
			}	
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//iprintf("%x ",cnt);
					//iprintf("%x ",(u32*)stackptr);
					//for(i=0;i<16;i++){
					//iprintf("%x ", *((u32*)stackptr+i*4));
					//}
					//iprintf("%x ",cnt);
		
					//iprintf("(2)ldmia:%x[%x]",offset,*((u32*)stackptr+(offset)));
					*((u32*)output_buf+(cnt))= *((u32*)stackptr+(offset));
				}
				else{
					*((u32*)output_buf-(cnt))= *((u32*)stackptr-(offset));
				}
			}
			offset++;
		}
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			
			if ((order&1) == 0){
				//iprintf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				rom=*((u32*)stackptr+(offset));
			}
			else{
				rom=*((u32*)stackptr-(offset));
			}
		}
	cnt++;
	}

break;

//EXCLUSIVE FOR STACK (LDMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
case(3): 	
	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15){
		//iprintf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//Ok but direct reads
					//*(u32*) ( (*((u32*)output_buf+ 0)) + (offset*4) )=*((u32*)stackptr+(cnt));
					//iprintf(" ldmia%x:srcvalue[%x]",cnt, *((u32*)stackptr+(cnt)) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
			
					//handled reads is GBA CPU layer specific:
					*((u32*)(output_buf+(cnt*4)))=cpuread_word((u32)(stackptr+(offset*4)));
					//iprintf(" (3)ldmia%x:srcvalue[%x]",cnt, cpuread_word(stackptr+(offset*4)));
				}
				else{
					*((u32*)(output_buf-(cnt*4)))=cpuread_word((u32)(stackptr-(offset*4)));
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				//Ok but direct reads
				//iprintf("restored %x to PC! ",*((u32*)stackptr+(offset)));
				//rom=*((u32*)stackptr+(offset));
			
				//handled reads is GBA CPU layer specific:
				rom=cpuread_word((u32)(stackptr+(cnt*4)));
				break;
			}
			else{
				rom=cpuread_word((u32)(stackptr-(cnt*4)));
				break;
			}
		}
	cnt++;
	}
break;

} //switch

return 0;
}

//2)create a call that stores desired r0-r7 reg (value held) into SP
//input_buffer / stackpointer buffer / regs affected / access 8 16 or 32 / 0 = swapped (ldr/str hardware) / 1 = not swapped (direct rw)
//	/ 2 = STACK MANAGEMENT (from virtual regs into STACK<mode>)
//order: 0 = ascending / 1 = descending
u32 __attribute__ ((hot)) stmiavirt(u8 * input_buf, u32 stackptr, u16 regs, u8 access, u8 byteswapped, u8 order){ 

//bit(regs);
//regs format: 1111 1111 1111 1111 (15<-- 0) in ascending order
int cnt=0;
int offset=0;


switch(byteswapped){
case(0): 	//SPECIFICALLY DONE FOR CPUBACKUP/RESTORE. 
			//don't use it FOR ANYTHING ELSE.
			
	while(cnt<16) {
	
		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
			
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){
				
				if ((order&1) == 0){
					*((u32*)stackptr+(cnt))= (* ((u32*)input_buf + offset)); //linear reads (each register is set on their corresponding offset from the buffer)
					//iprintf(" stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ offset)); );
				}
				else{
					*((u32*)stackptr-(cnt))= (* ((u32*)input_buf - offset)); 
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){		
			if ((order&1) == 0){
				//Ok but direct writes
				//*(((u32*)stackptr)+(offset))=rom;
				//iprintf("saved rom: %x -> stack",rom); //if rom overwrites this is why
			
				rom=(* ((u32*)input_buf + offset));
				break;
			}
			else{
				rom=(* ((u32*)input_buf - offset));
				break;
			}
		}
		
	cnt++;
	}
break;

case(1): //for STMIA/LDMIA virtual opcodes (if you save / restore all working registers full asc/desc this will not work for reg[0xf] PC)
	while(cnt<16) {
	//drainwrite();

		if( ((regs>>cnt) & 0x1) && cnt!=15 ){
			//iprintf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			
				if ((order&1) == 0){
					//iprintf("stmia:%d[%x] \n", offset,*((u32*)input_buf+(offset)));
					*((u16*)stackptr+(cnt))= *((u16*)input_buf+(offset));
				}
				else{
					*((u16*)stackptr-(cnt))= *((u16*)input_buf-(offset));
				}
			}	
		
			else if (access == (u8)32){
				if ((order&1) == 0){
					//iprintf("stmia:%d[%x]", offset,*(((u32*)input_buf)+(cnt)));
					*((u32*)stackptr+(offset))= (* ((u32*)input_buf+ cnt));
				}
				else{
					*((u32*)stackptr-(offset))= (* ((u32*)input_buf- cnt));
				}
			}
			
			offset++;
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				*((u32*)stackptr+offset)=rom;
				//rom=(u32)*((u32*)input_buf+(offset));
				//iprintf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
			}
			else{
				*((u32*)stackptr-offset)=rom;
				//rom=(u32)*((u32*)input_buf+(offset));
				//iprintf("writing to rom: %x",(u32)*((u32*)input_buf+(offset))); //if rom overwrites this is why
			}
		}
	cnt++;
	}
break;

//EXCLUSIVE FOR STACK (PUSH-POP stack<mode>_curr framepointer MANAGEMENT)
case(2): 	
	while(cnt<16) {
	
		if((regs>>cnt) & 0x1){
		//iprintf("%x ",cnt);
		
		if (access == (u8)8){
		}
		
		else if (access == (u8)16){
		}
		
		else if (access == (u8)32){
			
			if ((order&1) == 0){
				*((u32*)stackptr+(offset)) = (* ((u32*)input_buf+ cnt));	//reads from n register offset into full ascending stack
				//iprintf(" (2)stmia%x:srcvalue[%x]",cnt,(* ((u32*)input_buf+ cnt)) ); // stmia:  rn value = *(level_addsrc[0] + offset)
			}
			else{
				*((u32*)stackptr-(offset)) = (* ((u32*)input_buf- cnt));
			}
		}
			
		offset++;
		}
	cnt++;
	}
break;

//EXCLUSIVE FOR STACK (STMIA THUMB opcode) transfers between a RD and GBA virtual regs inter-operability
case(3): 	
	while(cnt<16) {
		if( ((regs>>cnt) & 0x1) && (cnt!=15)){
		//iprintf("%x ",cnt);
		
			if (access == (u8)8){
			}
		
			else if (access == (u8)16){
			}
		
			else if (access == (u8)32){ 
				if ((order&1) == 0){
				
					//OK writes but direct access
					//*((u32*)stackptr+(offset))= *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) );	//reads from register offset[reg n] from [level1] addr into full ascending stack
					//iprintf(" stmia%x:srcvalue[%x]",cnt, *(u32*) ( (*((u32*)input_buf+ 0)) + (cnt*4) ) ); // stmia:  level_addsrc[1] = *((*(level_addsrc[0])) + (offset*4)) 
			
					//handled writes is GBA CPU layer specific:
					cpuwrite_word((u32)(stackptr+(offset*4)), *((u32*)input_buf+cnt) );
					//iprintf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
				}
				else{
					//handled writes is GBA CPU layer specific:
					cpuwrite_word((u32)(stackptr-(offset*4)), *((u32*)input_buf-cnt) );
					//iprintf("(3):stmia(%x):srcvalue[%x]",cnt, cpuread_word((u32)stackptr+(offset*4)) );
				}
			}
		offset++; //offset is for linear memory reads (cnt is cpu register position dependant)	
		}
		
		else if (( ((regs>>cnt) & 0x1) && cnt==15 )){
			if ((order&1) == 0){
				//Ok but direct writes
				//*(((u32*)stackptr)+(offset))=rom;
				//iprintf("saved rom: %x -> stack",rom); //if rom overwrites this is why
		
				//handled writes is GBA CPU layer specific:
				cpuwrite_word((u32)(stackptr+(offset*4)), rom );
				//iprintf("read value:%x",cpuread_word(stackptr+(offset*4)));
				break;
			}
			else{
				//handled writes is GBA CPU layer specific:
				cpuwrite_word((u32)(stackptr-(offset*4)), rom );
				//iprintf("read value:%x",cpuread_word(stackptr+(offset*4)));
				break;
			}
		}
	cnt++;	//offset for non-linear register stacking
	}
break;

} //switch

return 0;
}

//fast single ldr / str opcodes for virtual environment gbaregs

u32 __attribute__ ((hot)) faststr(u8 * input_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
switch(byteswapped){
	//0: gbavirtreg handles.
	case(0):{
		if (access == (u8)8){
			switch(regs){
				case(0xf):
					rom=(* ((u8*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u8*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//iprintf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
		else if (access == (u8)16){
			switch(regs){
				case(0xf):
					rom=(* ((u16*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u16*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//iprintf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
		else if (access == (u8)32){
			
			switch(regs){
				case(0xf):
					rom=(* ((u32*)input_buf+ 0));
				break;
				
				default:
					gbareg[regs] = (* ((u32*)input_buf+ 0));	//this starts from current offset (required by unsorted registers)
					//iprintf(" str: %x:srcvalue[%x]",regs,(* ((u32*)input_buf+ 0)) ); // stmia:  rn value = *(level_addsrc[0] + offset)	
				break;
			} //don't care after this point
		}
	}
	break;
}

return 0;
}
//gbaregs[] arg 2
u32 __attribute__ ((hot)) fastldr(u8 * output_buf, u32 gbareg[0x10], u16 regs, u8 access, u8 byteswapped){
switch(byteswapped){
	//0: gbavirtreg handles.
	case(0):{
		if (access == (u8)8){
			switch(regs){
				case(0xf):
					*((u8*)output_buf+(0))=rom;
				break;
				
				default:
					*((u8*)output_buf+(0))= gbareg[regs];
					//iprintf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
		else if (access == (u8)16){
				switch(regs){
				case(0xf):
					*((u16*)output_buf+(0))=rom;
				break;
				
				default:
					*((u16*)output_buf+(0))= gbareg[regs];
					//iprintf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
		else if (access == (u8)32){
		
			switch(regs){
				case(0xf):
					*((u32*)output_buf+(0))=rom;
				break;
				
				default:
					*((u32*)output_buf+(0))= gbareg[regs] ;
					//iprintf(" ldr:%x[%x]",regs,*((u32*)stackptr+(regs)));
				break;
			}//don't care after this point
		}
	break;
	}
}
return 0;
}

//get physical stack sz (curr stack *) //value: gbastack address base u32 *
int getphystacksz(u32 * curr_stack){

if((u32)(u32*)curr_stack == (u32)&gbastck_usr[0])
	return gba_stack_usr_size; //iprintf("stack usr! \n"); 
	
else if ((u32)(u32*)curr_stack == (u32)&gbastck_fiq[0])
	return gba_stack_fiq_size; //iprintf("stack fiq! \n");
 
else if ((u32)(u32*)curr_stack == (u32)&gbastck_irq[0])
	return gba_stack_irq_size; //iprintf("stack irq! \n"); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_svc[0])
	return gba_stack_svc_size; //iprintf("stack svc! \n"); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_abt[0])
	return gba_stack_abt_size; //iprintf("stack abt! \n"); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_und[0])
	return gba_stack_und_size; //iprintf("stack und! \n"); 

//else if ((u32)(u32*)curr_stack == (u32)&gbastck_sys[0])
	//return gba_stack_sys_size; //iprintf("stack sys! \n"); 

else if ((u32)(u32*)curr_stack == (u32)&branch_stack[0])
	return gba_branch_table_size; //branch table size 
	
else
	return 0xdeaddead; //iprintf("ERROR STACK NOT DETECTED \n ");
	
}

//new stack test , includes branch stack test (do not use GBA CPU addressing) and hardware stack test (that use GBA CPU addressing) 
//returns: framepointer of initial stack (if success)
u32 * stack_test(u32 * stackfp,int size, u8 testmode){

int j=0;		//regs 0-15 offset / 16 bytestep
int ctr=0;
//branchstack opcode test
u32 * stackfpbu=stackfp;

//Gba model cpu addressing stack test
if(testmode==0){
	
	// 1/2 writes
	for(ctr=0;ctr<(size/4);ctr++){

		//fill! (+1 because we want the real integer modulus) 
		if( ((ctr+1) % 0x10) == 0){
			//iprintf("\n <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			//gbavirtreg_cpu[j]=0xc070+(j<<8);
			//iprintf("stack: @ %x for save data\n",(unsigned int)((u32)(u32*)stackfpbu+(ctr*4)));
			cpuwrite_word((unsigned int)(((u32)stackfpbu)+(ctr*4)),0xc070+(j<<8));
			//iprintf("(%x)[%x]\n",(unsigned int)((u32)(u32*)stackfpbu+(ctr*4)), (unsigned int) cpuread_word((u32)((u32)(u32*)stackfpbu+(ctr*4))));
			j++;
		}
	
		//debug
		//if(ctr>15) { iprintf("halt"); while(1);}
	}
	j=0;
	// 2/2 reads and check
	for(ctr=0;ctr<(size/4);ctr++){

		//fill! (+1 because we want the real integer modulus) 
		if( ((ctr+1) % 0x10) == 0){
			//iprintf("\n <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			
			if(cpuread_word(((u32)stackfpbu+(ctr*4))) == (0xc070+(j<<8)) ){
				cpuwrite_word(((u32)(u32*)stackfpbu+(ctr*4)),0x0); //clr block
				j++;
			}
			else{
				iprintf("\n [GBAStack] STUCK AT: base(%x)+(%x)",(unsigned int)(u32*)stackfpbu,(unsigned int)(ctr*4));
				iprintf("\n [GBAStack] value: [%x]",(unsigned int)cpuread_word((((u32)stackfpbu+(ctr*4)))));
				
				while (1);
			}
		}
	
		//debug
		//if(ctr>15) { iprintf("halt"); while(1);}
	}
	
	
	return (u32*)size;
}
else if(testmode==1){

	for(ctr=0;ctr<((int)(size) - (0x4*17) );ctr++){

		//fill!
		if( (ctr % 0x10) == 0){
			//iprintf("\n <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			gbavirtreg_cpu[j]=0xc070+(j<<8);
			j++;
		}

		if ( ((ctr % (gba_branch_block_size)) == 0) && (ctr != 0)) {
			stackfpbu=cpubackupmode((u32*)(stackfpbu),gbavirtreg_cpu,cpsrvirt); //already increases fp
			//iprintf("b.ofset:%x \n",(unsigned int)branchfpbu);
			//ofset+=0x1;
		}
	}

	//iprintf("1/2 stack test fp set to: %x \n",(unsigned int)(u32*)branchfpbu);
	//flush workreg
	for(j=0;j<0x10;j++){
		*((u32*)(u32)&gbavirtreg_cpu+j)=0x0;
	}

	//debug
	//branchfpbu=cpurestoremode((u32*)(u32)branchfpbu, &gbavirtreg[0]);
	//ldmiavirt((u8*)gbavirtreg[0]+(0x0), (u32)(u32*)(branchfpbu), 0xffff, 32, 0);

	//debug check if 16 regs address are recv OK
	//for(i=0;i<16;i++){
	//	iprintf(" REG%d [%x]",i,(unsigned int)*((u32*)gbavirtreg[0]+i));
	//}

	for(ctr=0;ctr<((int)(size) - (0x4*17));ctr++){

		if ( ((ctr % (gba_branch_block_size)) == 0)) {
			stackfpbu=cpurestoremode((u32*)(stackfpbu),gbavirtreg_cpu);
			//iprintf("b.ofset->restore :%x \n",(unsigned int)(u32*)branchfpbu);
			//ofset+=0x4;
		}

		//reset cnt!
		if( (ctr % 0x10) == 0){
			//iprintf(" <r%d:[%x]>",j,(unsigned int)gbavirtreg_cpu[j]);
			j=0;
		}

		else {
			if ( gbavirtreg_cpu[j] == (u32)(0xc070+(j<<8)))
				j++;
			else {
				//check why if 16 regs address are recv OK
				for(i=0;i<16;i++){
					//iprintf(" REG%d[%x]",i,(unsigned int)gbavirtreg_cpu[i]);
				}
				iprintf("\n [branchstack] STUCK AT: %x:%x",(unsigned int)(u32*)(stackfpbu+1),(unsigned int)gbavirtreg_cpu[j]);
				while(1);
			}
		}
	}
	//iprintf("2/2 stack test fp set to: %x- stack tests OK ;) \n",(u32)(u32*)branchfpbu);
	return stackfpbu;
}

else return (u32*)0x1;

}

///////////////////////////////////////////////////////GENERAL STACK OPCODES THAT AREN'T FOR GBA END////////////////////////////////////////////////

u32 bit(u32 val){
int bitcnt=0;
for(bitcnt=31;bitcnt>=0;bitcnt--){
    if((1<<bitcnt)&val) printf("1");
	else printf("0");
    }
    printf("\n");
return 0;
}


int utilReadInt2(FILE *f){
  int res = 0;
  int c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c;
  c = fgetc(f);
  if(c == EOF)
    return -1;
  return c + (res<<8);
}

int utilReadInt3(FILE *f){
  int res = 0;
  int c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c;
  c = fgetc(f);
  if(c == EOF)
    return -1;
  res = c + (res<<8);
  c = fgetc(f);
  if(c == EOF)
    return -1;
  return c + (res<<8);
}

void utilGetBaseName(const char *file, char *buffer){
  strcpy(buffer, file);
}

void utilGetBasePath(const char *file, char *buffer){
	strcpy(buffer,file);

	char *p = strrchr(buffer, '\\');

	if(p)
		*p = 0;
}

int LengthFromString(const char * timestring) {
	int c=0,decimalused=0,multiplier=1;
	int total=0;
	if (strlen(timestring) == 0) return 0;
	for (c=strlen(timestring)-1; c >= 0; c--) {
		if (timestring[c]=='.' || timestring[c]==',') {
			decimalused=1;
			total*=1000/multiplier;
			multiplier=1000;
		} else if (timestring[c]==':') multiplier=multiplier*6/10;
		else {
			total+=(timestring[c]-'0')*multiplier;
			multiplier*=10;
		}
	}
	if (!decimalused) total*=1000;
	return total;
}

int VolumeFromString(const char * volumestring) {
	int c=0,decimalused=0,multiplier=1;
	int total=0;
	if(strlen(volumestring) == 0) return 0;
	for(c=strlen(volumestring)-1; c >= 0; c--) {
		if (volumestring[c]=='.' || volumestring[c]==',') {
			decimalused=1;
			total*=1000/multiplier;
			multiplier=1000;
		} 
		else if ((volumestring[c]>='0')&& (volumestring[c]<='9')) {
			total+=(volumestring[c]-'0')*multiplier;
			multiplier*=10;
		}
		else
			break;
	}
	if (!decimalused) total*=1000;
	//return (float) total / 1000.;
	return total;
}

//unzip //returns size in bytes if succeded.
int unzip(char * filename, void * outbuf, uLong outbufsz){
	int unzoutput=UNZ_ERRNO;
	unzFile unzstat = unzOpen(filename);
	if (unzstat==NULL){
		printf("bad filename[%s]",filename);
		return -1;
	}
	//struct for global info
	unz_global_info global_info;
	if((unzoutput=(unzGetGlobalInfo( unzstat, &global_info ))) != UNZ_OK){
		return unzoutput;
	}
	//struct for file info
	unz_file_info file_info;
	unzGetCurrentFileInfo(unzstat,&file_info,filename,MAX_FILENAME_LENGTH,NULL, 0, NULL, 0);
	if((unzoutput=(unzOpenCurrentFile(unzstat))) != UNZ_OK){
		return unzoutput;
	}
	else {
		unzoutput=unzReadCurrentFile(unzstat,outbuf,outbufsz);
		unzClose( unzstat );
	}
return unzoutput;
}

void initmemory(struct GBASystem *gba){
	//coto
	//void *memset(void *sourcemem, int value, size_t n)
	gba->caioMem = (u8*)&gbacaioMem[0];
	gba->workram=(u8*)&gbawram[0];		//((int)&__ewram_end + 0x1);
	gba->iomem = (u8 *)&iomem[0];  //IO memory         (1 KBytes)
	gba->bios = (u8 *)&gbabios[0]; //BIOS - System ROM         (16 KBytes)
	gba->intram=(u8*)&gbaintram[0]; //On-chip Work RAM - IRAM (32K)
	gba->palram=(u8*)&palram[0]; //BG/OBJ Palette RAM (1K)
	gba->oam=(u8 *)&gbaoam[0]; //OAM - OBJ Attributes      (1 Kbyte)
	
	//4000240h - NDS9 - VRAMCNT_A - 8bit - VRAM-A (128K) Bank Control (W)
	//  0-2   VRAM MST              ;Bit2 not used by VRAM-A,B,H,I
	//  3-4   VRAM Offset (0-3)     ;Offset not used by VRAM-E,H,I
	//  5-6   Not used
	//  7     VRAM Enable (0=Disable, 1=Enable)

	//					Post-setup settings:
	//  VRAM    SIZE  MST  OFS   ARM9, 2D Graphics Engine A, BG-VRAM (max 512K)
	//  A,B,C,D 128K  1    0..3  6000000h+(20000h*OFS)
	//  E       64K   1    -     6000000h
	//  F,G     16K   1    0..3  6000000h+(4000h*OFS.0)+(10000h*OFS.1)
  
	
	u8 vramofs=(0<<3);
	u8 vrammst=(1<<0);
	
	//NDS vram A bank 6000000h+(20000h*OFS)
	gba->vidram=(u8 *)0x06000000+(0x20000*vramofs); //OAM - OBJ Attributes      (1 Kbyte)
	
	u32store(0x04000240 , vrammst|vramofs|(1<<7)); //MST 1 , vram offset 0 (A), VRAM enable
	
	
	if(((u8*)gba->vidram)!=NULL)
		iprintf("\n VRAM: OK :[%x]!",(unsigned int)(u8*)gba->vidram); 
	else{
		iprintf("\n VRAM FAIL! @:%x",(unsigned int)(u8*)gba->vidram);
		while(1);
	}

	int ramtestsz=0;
	
	ramtestsz=wramtstasm((int)gba->workram,256*1024);
	if(ramtestsz==alignw(256*1024))
		iprintf("\n GBAWRAM tst: OK :[%x]!",(unsigned int)(u8*)gba->workram); 
	else{
		iprintf("\n FAILED ALLOCING GBAEWRAM[%x]:@%d (bytes: %d)",(unsigned int)(u8*)gbawram,ramtestsz,0x10000);
		while(1);
	}
	memset((void*)gba->workram,0x0,0x10000);

	//OK
	ramtestsz=wramtstasm((int)gba->iomem,0x400);
	if(ramtestsz==alignw(0x400))
		iprintf("\n IOMEM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->iomem,ramtestsz);
	else 
		iprintf("\n IOMEM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->iomem,ramtestsz);
	memset((void*)gba->iomem,0x0,0x400);


	//this is OK
	ramtestsz=wramtstasm((int)gba->bios,0x4000);
	if(ramtestsz==alignw(0x4000))
		iprintf("\n BIOS tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->bios,ramtestsz);
	else 
		iprintf("\n BIOS tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->bios,ramtestsz);
	memset((void*)gba->bios,0x0,0x4000);

	//this is OK
	ramtestsz=wramtstasm((int)gba->intram,0x8000);
	if(ramtestsz==alignw(0x8000))
		iprintf("\n IRAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->intram,ramtestsz);
	else 
		iprintf("\n IRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->intram,ramtestsz);
	memset((void*)gba->intram,0x0,0x8000);

	//this is OK
	ramtestsz=wramtstasm((int)(u8*)gba->palram,0x400);
	if(ramtestsz==alignw(0x400))
		iprintf("\n PaletteRAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->palram,ramtestsz);
	else 
		iprintf("\n PaletteRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->palram,ramtestsz);
	memset((void*)gba->palram,0x0,0x400);

	//this is OK
	ramtestsz=wramtstasm((int)gba->oam,0x400);
	if(ramtestsz==alignw(0x400))
		iprintf("\n OAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->oam,ramtestsz);
	else 
		iprintf("\n OAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gba->oam,ramtestsz);
	memset((void*)gba->oam,0x0,0x400);

	//this is OK
	ramtestsz=wramtstasm((int)&gba->caioMem[0x0],0x400);
	if(ramtestsz==alignw(0x400))
		iprintf("\n caioMem tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)&gba->caioMem[0x0],ramtestsz);
	else 
		iprintf("\n caioMem tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)&gba->caioMem[0x0],ramtestsz);
	memset((void*)&gba->caioMem[0x0],0x0,0x400);

	//gba stack test
	for(i=0;i<sizeof(gbastck_usr)/2;i++){
		
		u16store((u32)&gbastck_usr[0] + (i*2),0xc070+i);	//gbastack[i] = 0xc070;	
		
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_usr[0] + (i*2) ) != (0xc070+i) ){
			iprintf("stackusr:failed writing @ %x \n",(unsigned int)&gbastck_usr+(i*2));
			iprintf("stackusr:reads: %x",(unsigned int)u16read((u32)&gbastck_usr[0] + (i*2)));
			iprintf("stackusr:base: %x",(unsigned int)&gbastck_usr[0]);
			while(1);
		}
	
		u16store((u32)&gbastck_usr[0] + (i*2),0x0);
	}

	for(i=0;i<sizeof(gbastck_fiq)/2;i++){
		u16store((u32)&gbastck_fiq[0] + (i*2),(0xc070+i)); //gbastack[i] = 0xc070;
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_fiq[0] + (i*2) ) != (0xc070+i) ){
			iprintf("stackfiq:failed writing @ %x \n",(unsigned int)&gbastck_fiq+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_fiq[0] + (i*2),0x0);
	}

	for(i=0;i<sizeof(gbastck_irq)/2;i++){
		u16store((u32)&gbastck_irq[0] + (i*2),0xc070+i); //gbastack[i] = 0xc070;
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_irq[0] + (i*2) ) != (0xc070+i) ){
			iprintf("stackirq:failed writing @ %x \n",(unsigned int)&gbastck_irq+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_irq[0] + (i*2),0x0);
	}

	for(i=0;i<sizeof(gbastck_svc)/2;i++){
		u16store((u32)&gbastck_svc[0] + (i*2),(0xc070+i)); //gbastack[i] = 0xc070;				
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_svc[0] + (i*2) ) != (0xc070+i) ){
			iprintf("stacksvc:failed writing @ %x \n",(unsigned int)&gbastck_svc+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_svc[0] + (i*2),0x0);
	}

	for(i=0;i<sizeof(gbastck_abt)/2;i++){
		u16store((u32)&gbastck_abt[0] + (i*2), (0xc070+i)); //gbastack[i] = 0xc070;				
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_abt[0] + (i*2)) != (0xc070+i)){
			iprintf("stackabt:failed writing @ %x \n",(unsigned int)&gbastck_abt+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_abt[0] + (i*2),0x0);
	}

	for(i=0;i<sizeof(gbastck_und)/2;i++){
		u16store((u32)&gbastck_und[0] + (i*2), (0xc070+i)); //gbastack[i] = 0xc070;				
		//iprintf("%x \n",(unsigned int)gbastack[i]); //iprintf("%x \n",(unsigned int)**(&gbastack+i*2)); //non contiguous memory

		if(u16read((u32)&gbastck_und[0] + (i*2) ) != (0xc070+i)){
			iprintf("stackund:failed writing @ %x \n",(unsigned int)&gbastck_und+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_und[0] + (i*2),0x0);
	}
	
	//usr/sys are same stacks, so removed
}

void initemu(struct GBASystem *gba){

gba->sound_clock_ticks = 167772; // 1/100 second
gba->bios=(u8*)0;//bios
gba->oam=(u8*)0;//oam
gba->vidram=(u8*)0;//vram
gba->iomem=(u8*)0;//ioMem
gba->intram=(u8*)0;//internalRAM
gba->workram=(u8*)0;//workRAM
gba->palram=(u8*)0;//paletteRAM

//refresh jump opcode in biosprotected vector
gba->biosprotected[0] = 0x00;
gba->biosprotected[1] = 0xf0;
gba->biosprotected[2] = 0x29;
gba->biosprotected[3] = 0xe1;
  

/*				 allocate segments
 GBA Memory Map

General Internal Memory
  00000000-00003FFF   BIOS - System ROM         (16 KBytes)
  00004000-01FFFFFF   Not used
  02000000-0203FFFF   WRAM - On-board Work RAM  (256 KBytes) 2 Wait
  02040000-02FFFFFF   Not used
  03000000-03007FFF   WRAM - On-chip Work RAM   (32 KBytes)
  03008000-03FFFFFF   Not used
  04000000-040003FE   I/O Registers
  04000400-04FFFFFF   Not used
Internal Display Memory
  05000000-050003FF   BG/OBJ Palette RAM        (1 Kbyte)
  05000400-05FFFFFF   Not used
  06000000-06017FFF   VRAM - Video RAM          (96 KBytes)
  06018000-06FFFFFF   Not used
  07000000-070003FF   OAM - OBJ Attributes      (1 Kbyte)
  07000400-07FFFFFF   Not used
External Memory (Game Pak)
  08000000-09FFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 0
  0A000000-0BFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 1
  0C000000-0DFFFFFF   Game Pak ROM/FlashROM (max 32MB) - Wait State 2
  0E000000-0E00FFFF   Game Pak SRAM    (max 64 KBytes) - 8bit Bus width
  0E010000-0FFFFFFF   Not used
*/

initmemory(gba); //healthy OK

#ifdef DEBUGEMU
	iprintf("init&teststacks OK");
#endif

//GBA address MAP setup
//gbamap set format-> struct map[index].address[(u8*)address] <- (u8*)(u32)&gba.dummysrc
for(i = 0; i < 256; i++){
	gba->map[i].address = (u8 *)(u32)&gba->dummysrc;
	gba->map[i].mask = 0;
}

for(i = 0; i < 0x400; i++)
	gba->ioreadable[i] = true;
for(i = 0x10; i < 0x48; i++)
    gba->ioreadable[i] = false;
for(i = 0x4c; i < 0x50; i++)
    gba->ioreadable[i] = false;
for(i = 0x54; i < 0x60; i++)
    gba->ioreadable[i] = false;
for(i = 0x8c; i < 0x90; i++)
    gba->ioreadable[i] = false;
for(i = 0xa0; i < 0xb8; i++)
    gba->ioreadable[i] = false;
for(i = 0xbc; i < 0xc4; i++)
    gba->ioreadable[i] = false;
for(i = 0xc8; i < 0xd0; i++)
    gba->ioreadable[i] = false;
for(i = 0xd4; i < 0xdc; i++)
    gba->ioreadable[i] = false;
for(i = 0xe0; i < 0x100; i++)
    gba->ioreadable[i] = false;
for(i = 0x110; i < 0x120; i++)
    gba->ioreadable[i] = false;
for(i = 0x12c; i < 0x130; i++)
    gba->ioreadable[i] = false;
for(i = 0x138; i < 0x140; i++)
    gba->ioreadable[i] = false;
for(i = 0x144; i < 0x150; i++)
    gba->ioreadable[i] = false;
for(i = 0x15c; i < 0x200; i++)
    gba->ioreadable[i] = false;
for(i = 0x20c; i < 0x300; i++)
    gba->ioreadable[i] = false;
for(i = 0x304; i < 0x400; i++)
    gba->ioreadable[i] = false;

//vba core init
gba->DISPCNT  = 0x0080;
gba->DISPSTAT = 0x0000;

#ifdef NOBIOS
	gba->VCOUNT   =  0 ;
#else
	gba->VCOUNT 	=  0x007E;
#endif

gba->BG0CNT   = 0x0000;
gba->BG1CNT   = 0x0000;
gba->BG2CNT   = 0x0000;
gba->BG3CNT   = 0x0000;
gba->BG0HOFS  = 0x0000;
gba->BG0VOFS  = 0x0000;
gba->BG1HOFS  = 0x0000;
gba->BG1VOFS  = 0x0000;
gba->BG2HOFS  = 0x0000;
gba->BG2VOFS  = 0x0000;
gba->BG3HOFS  = 0x0000;
gba->BG3VOFS  = 0x0000;
gba->BG2PA    = 0x0100;
gba->BG2PB    = 0x0000;
gba->BG2PC    = 0x0000;
gba->BG2PD    = 0x0100;
gba->BG2X_L   = 0x0000;
gba->BG2X_H   = 0x0000;
gba->BG2Y_L   = 0x0000;
gba->BG2Y_H   = 0x0000;
gba->BG3PA    = 0x0100;
gba->BG3PB    = 0x0000;
gba->BG3PC    = 0x0000;
gba->BG3PD    = 0x0100;
gba->BG3X_L   = 0x0000;
gba->BG3X_H   = 0x0000;
gba->BG3Y_L   = 0x0000;
gba->BG3Y_H   = 0x0000;
gba->WIN0H    = 0x0000;
gba->WIN1H    = 0x0000;
gba->WIN0V    = 0x0000;
gba->WIN1V    = 0x0000;
gba->WININ    = 0x0000;
gba->WINOUT   = 0x0000;
gba->MOSAIC   = 0x0000;
gba->BLDMOD   = 0x0000;
gba->COLEV    = 0x0000;
gba->COLY     = 0x0000;
gba->DM0SAD_L = 0x0000;
gba->DM0SAD_H = 0x0000;
gba->DM0DAD_L = 0x0000;
gba->DM0DAD_H = 0x0000;
gba->DM0CNT_L = 0x0000;
gba->DM0CNT_H = 0x0000;
gba->DM1SAD_L = 0x0000;
gba->DM1SAD_H = 0x0000;
gba->DM1DAD_L = 0x0000;
gba->DM1DAD_H = 0x0000;
gba->DM1CNT_L = 0x0000;
gba->DM1CNT_H = 0x0000;
gba->DM2SAD_L = 0x0000;
gba->DM2SAD_H = 0x0000;
gba->DM2DAD_L = 0x0000;
gba->DM2DAD_H = 0x0000;
gba->DM2CNT_L = 0x0000;
gba->DM2CNT_H = 0x0000;
gba->DM3SAD_L = 0x0000;
gba->DM3SAD_H = 0x0000;
gba->DM3DAD_L = 0x0000;
gba->DM3DAD_H = 0x0000;
gba->DM3CNT_L = 0x0000;
gba->DM3CNT_H = 0x0000;
gba->TM0D     = 0x0000;
gba->TM0CNT   = 0x0000;
gba->TM1D     = 0x0000;
gba->TM1CNT   = 0x0000;
gba->TM2D     = 0x0000;
gba->TM2CNT   = 0x0000;
gba->TM3D     = 0x0000;
gba->TM3CNT   = 0x0000;
gba->P1       = 0x03FF;
gbavirt_iemasking=0x0000;			//  gba->IE       = 0x0000;
gbavirt_ifmasking=0x0000;			//  gba->IF       = 0x0000;
gbavirt_imemasking=0x0000;			// gba->IME      = 0x0000;

cpu_updateregisters(0x00, gba->DISPCNT);	//UPDATE_REG(0x00, gba->DISPCNT);
cpu_updateregisters(0x06, gba->VCOUNT);	//UPDATE_REG(0x06, gba->VCOUNT);
cpu_updateregisters(0x20, gba->BG2PA);		//UPDATE_REG(0x20, gba->BG2PA);
cpu_updateregisters(0x26, gba->BG2PD);		//UPDATE_REG(0x26, gba->BG2PD);
cpu_updateregisters(0x30, gba->BG3PA);		//UPDATE_REG(0x30, gba->BG3PA);
cpu_updateregisters(0x36, gba->BG3PD);		//UPDATE_REG(0x36, gba->BG3PD);
cpu_updateregisters(0x130, gba->P1);		//UPDATE_REG(0x130, gba->P1);
cpu_updateregisters(0x88, 0x200);			//UPDATE_REG(0x88, 0x200);

#ifndef NOBIOS
	gba->lcdticks = 1008;
#else
	gba->lcdticks = 208;
#endif

gba->timer0on = false;
gba->timer0ticks = 0;
gba->timer0reload = 0;
gba->timer0clockreload  = 0;
gba->timer1on = false;
gba->timer1ticks = 0;
gba->timer1reload = 0;
gba->timer1clockreload  = 0;
gba->timer2on = false;
gba->timer2ticks = 0;
gba->timer2reload = 0;
gba->timer2clockreload  = 0;
gba->timer3on = false;
gba->timer3ticks = 0;
gba->timer3reload = 0;
gba->timer3clockreload  = 0;
gba->dma0source = 0;
gba->dma0dest = 0;
gba->dma1source = 0;
gba->dma1dest = 0;
gba->dma2source = 0;
gba->dma2dest = 0;
gba->dma3source = 0;
gba->dma3dest = 0;
gba->fxon = false;
gba->windowon = false;
gba->framecount = 0;
gba->savetype = 0;
gba->layerenable = gba->DISPCNT & gba->layersettings;

//OK so far
gba->map[0].address = gba->bios;
gba->map[0].mask = 0x3FFF;
gba->map[2].address = gba->workram;
gba->map[2].mask = 0x3FFFF;
gba->map[3].address = gba->intram; 
gba->map[3].mask = 0x7FFF;
gba->map[4].address = gba->iomem;
gba->map[4].mask = 0x3FF;
gba->map[5].address = gba->palram;
gba->map[5].mask = 0x3FF;
gba->map[6].address = gba->vidram;
gba->map[6].mask = 0x1FFFF;
gba->map[7].address = gba->oam;
gba->map[7].mask = 0x3FF;
gba->map[8].address = (u8*)(u32)rom;	// gba->rom; 	 //ROM entrypoint
gba->map[8].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/ //MASK for GBA addressable ROMDATA
gba->map[9].address = (u8*)(u32)rom;	// gba->rom; 	/
gba->map[9].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
gba->map[10].address = (u8*)(u32)rom;	// gba->rom; 	/
gba->map[10].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
gba->map[12].address = (u8*)(u32)rom;	// gba->rom; 	/
gba->map[12].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/

//setup patches for gbaoffsets into emulator offsets 
//for default gbamemread redirect if cpuread_writes call fail
addrfixes[0x0]=(u32)(u8*)gba->bios;
addrfixes[0x1]=(u32)(sizeof(gbabios));
addrfixes[0x2]=(u32)(u8*)gba->workram;
addrfixes[0x3]=(u32)(sizeof(gbawram));
addrfixes[0x4]=(u32)(u8*)gba->intram;
addrfixes[0x5]=(u32)(sizeof(gbaintram));
addrfixes[0x6]=(u32)(u8*)gba->iomem;
addrfixes[0x7]=(u32)(sizeof(iomem));
addrfixes[0x8]=(u32)(u8*)gba->palram;
addrfixes[0x9]=(u32)(sizeof(palram));
addrfixes[0xa]=(u32)(u8*)gba->vidram;
addrfixes[0xb]=(u32)(128*1024); //128K VRAM
addrfixes[0xc]=(u32)(u8*)gba->oam;
addrfixes[0xd]=(u32)(sizeof(gbaoam));
addrfixes[0xe]=(u32)0x08000000; /*(u8*)rom*/ //ROM start
addrfixes[0xf]=(u32)gba->romsize; //ROM TOP IS SIZE OF ROM

//Cpu_Stack_USR EQU 0x03007F00 ; GBA USR stack adress
//Cpu_Stack_IRQ EQU 0x03007FA0 ; GBA IRQ stack adress
//Cpu_Stack_SVC EQU 0x03007FE0 ; GBA SVC stack adress

//gba->reg[13].I = 0x03007F00;
//gba->reg[15].I = 0x08000000;
//gba->reg[16].I = 0x00000000;
//gba->reg[R13_IRQ].I = 0x03007FA0;
//gba->reg[R13_SVC].I = 0x03007FE0;
//gba->armIrqEnable = true;

//new stack setup
//1) set stack base 2) to detect stack top just sizeof(gbastck_mode), framepointer has the current pos (from 0 to n) used so far
gbastckadr_usr=(u32*)0x03007F00; //0x100 size & 0x10 as CPU <mode> start (usr/sys shared stack)
gbavirtreg_cpu[0xd]=gbavirtreg_r13usr[0]=(u32)(u32*)gbastckadr_usr;
gbastckfp_usr=(u32*)0x03007F00;
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_usr,0xff,0x0)==(int)0xff)
		iprintf("USR stack OK!");
	else
		iprintf("USR stack WRONG!");
#endif

gbastckadr_fiq=(u32*)(gbastckadr_usr-GBASTACKSIZE); //custom fiq stack
gbavirtreg_r13fiq[0]=(u32)(u32*)gbastckadr_fiq;
gbastckfp_fiq=(u32*)(gbastckadr_usr-GBASTACKSIZE); //#GBASTACKSIZE size
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_fiq,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
		iprintf("FIQ stack OK!");
	else
		iprintf("FIQ stack WRONG!");
#endif

gbastckadr_irq=(u32*)0x03007FA0;
gbavirtreg_r13irq[0]=(u32)(u32*)gbastckadr_irq;
gbastckfp_irq=(u32*)0x03007FA0;
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_irq,0xff,0x0)==(int)0xff)
		iprintf("IRQ stack OK!");
	else
		iprintf("IRQ stack WRONG!");
#endif

gbastckadr_svc=(u32*)0x03007FE0;
gbavirtreg_r13svc[0]=(u32)(u32*)gbastckadr_svc;
gbastckfp_svc=(u32*)0x03007FE0;
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_svc,0xff,0x0)==(int)0xff)
		iprintf("SVC stack OK!");
	else
		iprintf("SVC stack WRONG!");
#endif

gbastckadr_abt=(u32*)(gbastckadr_fiq-GBASTACKSIZE); //custom abt stack
gbavirtreg_r13abt[0]=(u32)(u32*)gbastckadr_abt;
gbastckfp_abt=(u32*)(gbastckadr_fiq-GBASTACKSIZE);
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_abt,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
		iprintf("ABT stack OK!");
	else
		iprintf("ABT stack WRONG!");
#endif

gbastckadr_und=(u32*)(gbastckadr_abt-GBASTACKSIZE); //custom und stack
gbavirtreg_r13und[0]=(u32)(u32*)gbastckadr_und;
gbastckfp_und=(u32*)(gbastckadr_abt-GBASTACKSIZE);
#ifdef STACKTEST
	if((int)stack_test(gbastckadr_und,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
		iprintf("UND stack OK!");
	else
		iprintf("UND stack WRONG!");
#endif

}


int utilload(const char *file,u8 *data,int size,bool extram){ //*file is filename (.gba)
																//*data is pointer to store rom  / always ~256KB &size at load
#ifndef NOBIOS
//bios copy to biosram
FILE *f = fopen("gba.bios", "r");
if(!f){ 
	iprintf("there is no gba.bios in root!"); while(1);
}

int fileSize=fread((void*)(u8*)gba.bios, 1, 0x4000,f);

fclose(f);
if(fileSize!=0x4000){
	iprintf("failed gba.bios copy @ %x! so far:%d bytes",(unsigned int)gba.bios,fileSize);
	while(1);
	}
else
	//iprintf("bios OK!");
	/*
		// tempbuffer2 
		iprintf("\n /// GBABIOS @ %x //",(unsigned int)(u8*)gba.bios);
			
		for(i=0;i<16;i++){
			iprintf(" %x:[%d] ",i,(unsigned int)*((u32*)gbabios+i));
			
			if (i==15) iprintf("\n");
			
		}
		while(1);
	*/
#else
int fileSize=0;
FILE *f;
#endif

//gbarom setup
f = fopen(file, "rb");
if(!f) {
	iprintf("Error opening image %s",file);
	return 0;
}

fseek(f,0,SEEK_END);
fileSize = ftell(f);
fseek(f,0,SEEK_SET);

/* //header part that is not required anymore
fread((char*)&gbaheader, 1, sizeof(gbaheader),f);

//size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int temp = fread((void*)gbaheaderbuf,sizeof(gbaheaderbuf[0]),0x200,f);
if (temp != 0x200){
	iprintf("\n error ret. gbaheader (size rd: %d)",temp);
	while(1);
}

//iprintf("\n util.c filesize: %d ",fileSize);
//iprintf("\n util.c entrypoint: %x ",(unsigned int)0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);

*/

generate_filemap(fileSize);

if(data == 0){ //null rom destination pointer? allocate space for it	
	/*8K for futur alloc 0x2000 unused*/
	//gba.romSize = (((int)&__ewram_end) - 0x1) - ((int)sbrk(0) + 0x5000 + 0x2000); // availablesize = NDSRAMuncSZ - c_progbrk + (20480 +  8192) : 20480 romsize
	
	//filesize
	gba.romsize=fileSize;	//size readjusted for final alloc'd rom
	romsize=fileSize;
	
	//rom entrypoint
	rom_entrypoint=(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
	
	//set rom address
	rom=(u32)0x08000000;	
	
	//iprintf("entrypoint @ %x! ",(unsigned int)(u32*)rom_entrypoint);
}

fclose(f);

iprintf("generated filemap! OK:\n");

return gba.romsize; //rom buffer size
}

int loadrom(struct GBASystem * gba,const char *filename,bool extram){
	
//#ifdef ROMTEST
//rom = (u8*) &rom_pl_bin;
//gba->romsize = (int) rom_pl_bin_size;
//#endif

gba->romsize = 0x40000; //256KB partial romsize

u8 *whereToLoad;
whereToLoad=(u8*)0;

if(gba->cpuismultiboot) whereToLoad = gba->workram;

gba->romsize = utilload(filename,whereToLoad,gba->romsize,extram);
if(gba->romsize==0){ //set ups u8 * rom to point to allocated buffer and returns *partial* or full romSize
	iprintf("error retrieving romSize \n");
return 0;
}

return gba->romsize;
}
