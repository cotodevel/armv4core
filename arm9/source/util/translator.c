//coto: this translator is mostly mine work. (except gbareads[ichfly] & cpuread_data from VBA)

#include "translator.h"
#include "opcode.h"
#include "Util.h"
#include "pu.h"
#include "supervisor.h"
#include "ipcfifoTGDSUser.h"
#include "main.h"
#include "fatfslayerTGDS.h"
#include "InterruptsARMCores_h.h"
#include "ff.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "xenofunzip.h"
#include "gba.arm.core.h"

/*
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

u32 * cpubackupmode(u32 * branch_stackfp, u32 cpuregvector[0x16], u32 cpsr){

	//if frame pointer reaches TOP don't increase SP anymore
	// (ptrpos - ptrsrc) * int depth < = ptr + size struct (minus last element bc offset 0 uses an element)
	if( (int)( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4) <  (int)(gba_branch_table_size - gba_branch_elemnt_size)){ 
	
	//printf("gba_branch_offst[%x] +",( ( ((u32*)branch_stackfp-(u32*)&branch_stack[0])) *4)); //debug
	
		stmiavirt((u8*)(&cpuregvector[0]), (u32)(u32*)branch_stackfp, 0xffff, 32, 0, 0);
		
		//for(i=0;i<16;i++){
		//	printf(">>%d:[%x]",i,cpuregvector[i]);
		//}
		//while(1);
		
		//printf("    	--	");
		//move 16 bytes ahead fp and store cpsr
		//debug: stmiavirt((u8*)cpsr, (u32)(u32*)(branch_stackfp+0x10), 0x1 , 32, 0);
		
		//printf("curr_used fp:%x / top: %x / block size: %x ",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
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
		//printf("branch stack OVERFLOW! "); //debug
	}

return branch_stackfp;

}
//this one restores cpsrvirt by itself (restores CPU mode)
u32 * cpurestoremode(u32 * branch_stackfp, u32 cpuregvector[0x16]){

	//if frame pointer reaches bottom don't decrease SP anymore:
	//for near bottom case
	if ( (u32*)branch_stackfp > (u32*)&branch_stack[0]) { 
	
	//printf("gba_branch_block_size[%x] -",gba_branch_block_size); //debug

		//printf("curr_used fp:%x / top: %x / block size: %x ",(int)((((u32*)branch_stackfp-(u32*)&branch_stack[0])*4)),gba_branch_table_size,gba_branch_block_size);
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
		//printf("branch stack has reached bottom! ");
	}
	
return branch_stackfp;
}

//Update CPU status flags (Z,N,C,V, THUMB BIT)
//mode: 0 = hardware asm cpsr update (cpsrvirt & cpu cond flags) / 1 = virtual CPU mode change,  CPSR , change to CPU mode
// / 2 = Writes to IO MAP for GBA environment variable updates
u32 updatecpuflags(u8 mode ,u32 cpsr , u32 cpumode){
	switch(mode){
		case (0):
			//1) if invoked from hardware asm function, update flags to virtual environment
			z_flag=(lsrasm(cpsr,0x1e))&0x1;
			n_flag=(lsrasm(cpsr,0x1f))&0x1;
			c_flag=(lsrasm(cpsr,0x1d))&0x1;
			v_flag=(lsrasm(cpsr,0x1c))&0x1;
			cpsrvirt&=~0xF0000000;
			cpsrvirt|=(n_flag<<31|z_flag<<30|c_flag<<29|v_flag<<28);
			//cpsr = latest cpsrasm from virtual asm opcode
			//printf("(0)CPSR output: %x ",cpsr);
			//printf("(0)cpu flags: Z[%x] N[%x] C[%x] V[%x] ",z_flag,n_flag,c_flag,v_flag);
		
		break;
		
		case (1):{
			//1)check if cpu<mode> swap does not come from the same mode
			if((cpsr&0x1f)!=cpumode){
		
			//2a)save stack frame pointer for current stack 
			//and detect old cpu mode from current loaded stack, then store LR , PC into stack (mode)
			//printf("cpsr:%x / spsr:%x",cpsr&0x1f,spsr_last&0x1f);
			
				if( ((cpsrvirt&0x1f) == (0x10)) || ((cpsrvirt&0x1f) == (0x1f)) ){ //detect usr/sys (0x10 || 0x1f)
					spsr_last=spsr_usr=cpsrvirt;
					
					//gbastckadr_usr=gbastckmodeadr_curr=; //not required this is to know base stack usr/sys
					gbastckfp_usr=gbastckfpadr_curr;
					exRegs_r13usr[0x0]=exRegs[0xd]; //user/sys is the same stacks
					exRegs_r14usr[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						printf("stacks backup usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13usr[0x0]);
					#endif
					//printf("before nuked SP usr:%x",(unsigned int)exRegs_r13usr[0x0]);
					
					/* //deprecated
						//if framepointer does not reach the top:
						if((u32)updatestackfp(gbastckfp_usr,gbastckadr_usr) != (u32)0){
					
						//store LR to actual (pre-SPSR'd) Stack
						gbastckfp_usr[0]=exRegs[0xe];
						
						//store PC to actual (pre-SPSR'd stack)
						gbastckfp_usr[1] = exRegs[0xf];
						
						//increase fp by the ammount of regs added
						gbastckfp_usr=(u32*)addspvirt((u32)(u32*)gbastckfp_usr,2);
					}
					*/
				}
				else if((cpsrvirt&0x1f)==0x11){
					spsr_last=spsr_fiq=cpsrvirt;
					gbastckfp_fiq=gbastckfpadr_curr;
					exRegs_r13fiq[0x0]=exRegs[0xd];
					exRegs_r14fiq[0x0]=exRegs[0xe];
					
					//save
					//5 extra regs r8-r12 for fiq
					exRegs_fiq[0x0] = exRegs[0x0 + 8];
					exRegs_fiq[0x1] = exRegs[0x1 + 8];
					exRegs_fiq[0x2] = exRegs[0x2 + 8];
					exRegs_fiq[0x3] = exRegs[0x3 + 8];
					exRegs_fiq[0x4] = exRegs[0x4 + 8];
					
					//restore 5 extra reg subset for other modes
					exRegs[0x0 + 8]=exRegs_cpubup[0x0];
					exRegs[0x1 + 8]=exRegs_cpubup[0x1];
					exRegs[0x2 + 8]=exRegs_cpubup[0x2];
					exRegs[0x3 + 8]=exRegs_cpubup[0x3];
					exRegs[0x4 + 8]=exRegs_cpubup[0x4];
					#ifdef DEBUGEMU
						printf("stacks backup fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13fiq[0x0]);
					#endif
				}
				else if((cpsrvirt&0x1f)==0x12){
					spsr_last=spsr_irq=cpsrvirt;
					gbastckfp_irq=gbastckfpadr_curr;
					exRegs_r13irq[0x0]=exRegs[0xd];
					exRegs_r14irq[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						printf("stacks backup irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13irq[0x0]);
					#endif
				}
				
				else if((cpsrvirt&0x1f)==0x13){
					spsr_last=spsr_svc=cpsrvirt;
					gbastckfp_svc=gbastckfpadr_curr;
					exRegs_r13svc[0x0]=exRegs[0xd];
					exRegs_r14svc[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						printf("stacks backup svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13svc[0x0]);
					#endif
				}
				else if((cpsrvirt&0x1f)==0x17){
					spsr_last=spsr_abt=cpsrvirt;
					gbastckfp_abt=gbastckfpadr_curr;
					exRegs_r13abt[0x0]=exRegs[0xd];
					exRegs_r14abt[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						printf("stacks backup abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13abt[0x0]);
					#endif
				}
				
				else if((cpsrvirt&0x1f)==0x1b){
					spsr_last=spsr_und=cpsrvirt;
					gbastckfp_und=gbastckfpadr_curr;
					exRegs_r13und[0x0]=exRegs[0xd];
					exRegs_r14und[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						printf("stacks backup und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs_r13und[0x0]);
					#endif
				}
				
				//default (no previous PSR) sets
				else {
				
					// disable FIQ & set SVC
					// reg[16].I |= 0x40;
					cpsr|=0x40;
					cpsr|=0x13;
					
					//#ifdef DEBUGEMU
					//	printf("ERROR CHANGING CPU MODE/STACKS : CPSR: %x ",(unsigned int)cpsrvirt);
					//	while(1);
					//#endif
				}
			
			//update SPSR on CPU change <mode> (this is exactly where CPU change happens)
			spsr_last=cpsr;
			
			//3)setup current CPU mode working set of registers and perform stack swap
			//btw2: gbastckfp_usr can hold up to 0x1ff (511 bytes) of data so pointer must not exceed that value
			
			//case gbastckfp_mode  
			//load LR & PC from regs then decrease #1 for each
			
			//unique cpu registers : exRegs[n];
			
			//user/sys
			if ( ((cpumode&0x1f) == (0x10)) || ((cpumode&0x1f) == (0x1f)) ){	
					
					gbastckmodeadr_curr=gbastckadr_usr;
					gbastckfpadr_curr=gbastckfp_usr;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13usr[0x0]; //user SP/LR registers for cpu<mode> (user/sys is the same stacks)
					exRegs[0xe]=exRegs_r14usr[0x0];
					#ifdef DEBUGEMU
						printf("| stacks swap to usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			
			else if((cpumode&0x1f)==0x11){
					gbastckmodeadr_curr=gbastckadr_fiq;
					gbastckfpadr_curr=gbastckfp_fiq;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13fiq[0x0]; //fiq SP/LR registers for cpu<mode>
					exRegs[0xe]=exRegs_r14fiq[0x0];
					
					//save register r8-r12 subset before entering fiq
					exRegs_cpubup[0x0]=exRegs[0x0 + 8];
					exRegs_cpubup[0x1]=exRegs[0x1 + 8];
					exRegs_cpubup[0x2]=exRegs[0x2 + 8];
					exRegs_cpubup[0x3]=exRegs[0x3 + 8];
					exRegs_cpubup[0x4]=exRegs[0x4 + 8];
					
					//restore: 5 extra regs r8-r12 for fiq restore
					exRegs[0x0 + 8]=exRegs_fiq[0x0];
					exRegs[0x1 + 8]=exRegs_fiq[0x1];
					exRegs[0x2 + 8]=exRegs_fiq[0x2];
					exRegs[0x3 + 8]=exRegs_fiq[0x3];
					exRegs[0x4 + 8]=exRegs_fiq[0x4];
					#ifdef DEBUGEMU
						printf("| stacks swap to fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			//irq
			else if((cpumode&0x1f)==0x12){
					gbastckmodeadr_curr=gbastckadr_irq;
					gbastckfpadr_curr=gbastckfp_irq;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13irq[0x0]; //irq SP/LR registers for cpu<mode>
					exRegs[0xe]=exRegs_r14irq[0x0];
					#ifdef DEBUGEMU
						printf("| stacks swap to irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			//svc
			else if((cpumode&0x1f)==0x13){
					gbastckmodeadr_curr=gbastckadr_svc;
					gbastckfpadr_curr=gbastckfp_svc;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13svc[0x0]; //svc SP/LR registers for cpu<mode> (user/sys is the same stacks)
					exRegs[0xe]=exRegs_r14svc[0x0];
					#ifdef DEBUGEMU
						printf("| stacks swap to svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			//abort
			else if((cpumode&0x1f)==0x17){
					gbastckmodeadr_curr=gbastckadr_abt;
					gbastckfpadr_curr=gbastckfp_abt;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13abt[0x0]; //abt SP/LR registers for cpu<mode>
					exRegs[0xe]=exRegs_r14abt[0x0];
					#ifdef DEBUGEMU
						printf("| stacks swap to abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			//undef
			else if((cpumode&0x1f)==0x1b){
					gbastckmodeadr_curr=gbastckadr_und;
					gbastckfpadr_curr=gbastckfp_und;			//current framepointer address (setup in util.c) and updated here
					exRegs[0xd]=exRegs_r13und[0x0]; //und SP/LR registers for cpu<mode>
					exRegs[0xe]=exRegs_r14und[0x0];
					#ifdef DEBUGEMU
						printf("| stacks swap to und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)exRegs[0xd]);
					#endif
			}
			
			//then update cpsr (mode) <-- cpsr & spsr case dependant
			cpsr&=~0x1f;
			cpsr|=(cpumode&0x1f);
			
			//->switch to arm/thumb mode depending on cpsr for virtual env
			if( ((cpsr>>5)&1) == 0x1 )
				armstate=0x1;
			else
				armstate=0x0;
			
			cpsr&=~(1<<0x5);
			cpsr|=((armstate&1)<<0x5);
			
			//save changes to CPSR
			cpsrvirt=cpsr;
			
		}	//end if current cpumode != new cpu mode
		
		else{
			#ifdef DEBUGEMU
				printf("cpsr(arg1)(%x) == cpsr(arg2)(%x)",(unsigned int)(cpsr&0x1f),(unsigned int)cpumode);
			#endif
			
			//any kind of access case:
			
			//->can't rewrite SAME cpu<mode> slots!!
			//->switch to arm/thumb mode depending on cpsr
			if( ((cpsr>>5)&1) == 0x1 )
				armstate=0x1;
			else
				armstate=0x0;
			
			cpsr&=~(1<<0x5);
			cpsr|=((armstate&1)<<0x5);
			
			//save changes to CPSR
			cpsrvirt=cpsr;
		}
		
		}
		break;
		
		
		default:
		break;
	}

	return 0;
}

///////////////////////////////////////THUMB virt/////////////////////////////////////////

u32 __attribute__ ((hot)) disthumbcode(u32 thumbinstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
//drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//testing gba accesses translation to allocated ones
//printf("output: %x ",addresslookup( 0x070003ff, (u32*)&addrpatches[0],(u32*)&addrfixes[0]));

//debug addrfixes
//int i=0;
//for(i=0;i<16;i++){
//	printf(" patch : %x",*((u32*)&addrfixes+(i)));
//	if (i==15) printf("");
//}

//Low regs
switch(thumbinstr>>11){
	////////////////////////5.1
	//LSL opcode
	case 0x0:
		//extract reg
		u32 destroyableRegister = exRegs[((thumbinstr>>3)&7)];
		u32 destroyableRegister2 = lslasm(destroyableRegister,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("LSL r%d[%x],r%d[%x],#%x (5.1)",(int)(thumbinstr&0x7),(unsigned int)destroyableRegister2,(int)((thumbinstr>>3)&0x7),(unsigned int)destroyableRegister,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		exRegs[(thumbinstr&0x7)] = destroyableRegister2;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//LSR opcode
	case 0x1: 
		//extract reg
		u32 destroyableRegister = exRegs[((thumbinstr>>3)&7)];
		u32 destroyableRegister2 = lsrasm(destroyableRegister,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("LSR r%d[%x],r%d[%x],#%x (5.1)",(int)(thumbinstr&0x7),(unsigned int)destroyableRegister2,(int)((thumbinstr>>3)&0x7),(unsigned int)destroyableRegister,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		exRegs[(thumbinstr&0x7)] = destroyableRegister2;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//ASR opcode
	case 0x2:
		//extract reg
		u32 destroyableRegister = exRegs[((thumbinstr>>3)&7)];
		u32 destroyableRegister2 = (u32)asrasm((int)destroyableRegister,((thumbinstr>>6)&0x1f));
		#ifdef DEBUGEMU
		printf("ASR r%d[%x],r%d[%x],#%x (5.1)",(int)(thumbinstr&0x7),(unsigned int)destroyableRegister2,(int)((thumbinstr>>3)&0x7),(unsigned int)destroyableRegister,(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		exRegs[(thumbinstr&0x7)] = destroyableRegister2;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//5.3
	//mov #imm bit[0-8] / move 8 bit value into rd
	case 0x4:
		//mov
		#ifdef DEBUGEMU
		printf("mov r%d,#0x%x (5.3)",(int)((thumbinstr>>8)&0x7),(unsigned int)thumbinstr&0xff);
		#endif
		u32 destroyableRegister = movasm((u32)(thumbinstr&0xff));
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//cmp / compare contents of rd with #imm bit[0-8] / gba flags affected
	case 0x5:
		//rs
		#ifdef DEBUGEMU
		printf("cmp r%d[%x],#0x%x (5.3)",(int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[((thumbinstr>>8)&0x7)],(unsigned int)(thumbinstr&0xff));
		#endif
		cmpasm(exRegs[((thumbinstr>>8)&0x7)],(u32)thumbinstr&0xff);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//add / add #imm bit[7] value to contents of rd and then place result on rd
	case 0x6:
		//rn
		#ifdef DEBUGEMU
		printf("add r%d[%x], #%x (5.3)", (int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[((thumbinstr>>8)&0x7)],(unsigned int)(thumbinstr&0xff));
		#endif
		u32 destroyableRegister = addasm(exRegs[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		//done? update desired reg content
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;
	
	//sub / sub #imm bit[0-8] value from contents of rd and then place result on rd
	case 0x7:	
		//rn
		#ifdef DEBUGEMU
		printf("sub r%d[%x], #%x (5.3)", (int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[((thumbinstr>>8)&0x7)],(unsigned int)(thumbinstr&0xff));
		#endif
		u32 destroyableRegister = subasm(exRegs[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		//done? update desired reg content
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	break;

	//5.6
	//PC relative load WORD 10-bit Imm
	case 0x9:
		u32 destroyableRegister = cpuread_word(((exRegs[0xf]&0xfffffffe)+0x4)+((thumbinstr&0xff)<<2)); //[PC+0x4,#(8<<2)Imm] / because prefetch and alignment
		#ifdef DEBUGEMU
		printf("(WORD) LDR r%d[%x], [PC:%x,#%x] (5.6) ",(int)((thumbinstr>>8)&0x7),(unsigned int)destroyableRegister,(unsigned int)exRegs[0xf],(unsigned int)(thumbinstr&0xff));
		#endif
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister;
		return 0;
	break;
	
	////////////////////////////////////5.9 LOAD/STORE low reg with #Imm
	
	/* STR RD,[RB,#Imm] */
	case(0xc):{
		//1)read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		//exRegs[(thumbinstr&0x7)]
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf(" STR r%d(%x), [r%d(%x),#0x%x] (5.9)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
		#endif
		//2b) RB = #Imm + RB 
		u32 destroyableRegister = addsasm(exRegs[((thumbinstr>>3)&0x7)],(u32)(((thumbinstr>>6)&0x1f)<<2));
		//store RD into [RB,#Imm]
		cpuwrite_word(destroyableRegister, exRegs[(thumbinstr&0x7)]);
		#ifdef DEBUGEMU
		printf("content @%x:[%x]",(unsigned int)destroyableRegister,(unsigned int)cpuload_word(destroyableRegister));
		#endif
		return 0;
	}
	break;
	
	/* LDR RD,[RB,#Imm] */
	//warning: small error on arm7tdmi docs (this should be LDR, but is listed as STR) as bit 11 set is load, and unset store
	case(0xd):{ //word quantity (#Imm is 7 bits, filled with bit[0] & bit[1] = 0 by shifting >> 2 )
		//RB
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//get #Imm
		u32 destroyableRegister2 = (((thumbinstr>>6)&0x1f)<<2);
		
		//add with #imm
		u32 destroyableRegister = addsasm(exRegs[((thumbinstr>>3)&0x7)],destroyableRegister2);
		destroyableRegister = cpuread_word(destroyableRegister);
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("LDR r%d, [r%d,#0x%x]:[%x] (5.9)",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)destroyableRegister2, (unsigned int)destroyableRegister);
		#endif
		return 0;
	}
	break;
	
	//STRB rd, [rs,#IMM] (5.9)
	case(0xe):{
		//1)read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		//exRegs[(thumbinstr&0x7)]
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//2b) RB = #Imm + RB 
		u32 destroyableRegister = addsasm(exRegs[((thumbinstr>>3)&0x7)],(u32)(((thumbinstr>>6)&0x1f)<<2));
		
		//store RD into [RB,#Imm]
		cpuwrite_byte(destroyableRegister, exRegs[(thumbinstr&0x7)]&0xff);
		
		#ifdef DEBUGEMU
			printf(" STRB r%d(%x), [r%d(%x),#0x%x]:[%x] (5.9)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(unsigned int)((thumbinstr>>6)&0x1f)<<2);
			printf(" Content: @%x:[%x]",(unsigned int)destroyableRegister,(unsigned int)cpuread_byte(destroyableRegister));
		#endif
		return 0;
	}
	break;
	
	//LDRB rd, [Rb,#IMM] (5.9)
	case(0xf):{ //byte quantity (#Imm is 5 bits)
		//RB
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//get #Imm
		//(((thumbinstr>>6)&0x1f)<<2);
		
		//add with #imm
		u32 destroyableRegister = addsasm(exRegs[((thumbinstr>>3)&0x7)], (((thumbinstr>>6)&0x1f)<<2));
		u32 destroyableRegister2 = cpuread_byte(destroyableRegister);
		exRegs[(thumbinstr&0x7)] = destroyableRegister2;
		
		#ifdef DEBUGEMU
			printf(" ldrb Rd(%d)[%x], [Rb(%d)[%x],#(0x%x)] (5.9)",
			(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
			(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
			(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
			printf(" Content: @%x:[%x]",(unsigned int)destroyableRegister,(unsigned int)destroyableRegister2);
		#endif
		
		return 0;
	}
	break;
	
	/////////////////////5.10
	//store halfword from rd to low reg rs
	// STRH rd, [rb,#IMM] (5.10)
	case(0x10):{
		//RB
		//exRegs[((thumbinstr>>3)&0x7)]
		//RD
		//exRegs[(thumbinstr&0x7)]
		u32 destroyableRegister =addsasm(exRegs[((thumbinstr>>3)&0x7)],(((thumbinstr>>6)&0x1f)<<1)); // Rb + #Imm bit[6] depth (adds >>0)
		//store RD into [RB,#Imm]
		cpuwrite_hword(destroyableRegister, exRegs[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
		printf("strh r(%d)[%x] ,[Rb(%d)[%x],#[%x]] (5.7)",
		(int)((thumbinstr)&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(unsigned int)(((thumbinstr>>6)&0x1f)<<1));
		#endif
		return 0;
	}
	break;
	
	//load halfword from rs to low reg rd
	//LDRH Rd, [Rb,#IMM] (5.10)
	case(0x11):{
		//RB
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//get #Imm
		//(((thumbinstr>>6)&0x1f)<<1);
		
		//add with #imm
		u32 destroyableRegister = addsasm(exRegs[((thumbinstr>>3)&0x7)], (((thumbinstr>>6)&0x1f)<<1));
		destroyableRegister = cpuread_hword(destroyableRegister);
		
		//Rd
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
			printf(" ldrh Rd(%d)[%x], [Rb(%d)[%x],#(0x%x)] (5.9)",
			(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
			(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
			(unsigned int)(((thumbinstr>>6)&0x1f)<<1)); //if freeze undo this
		#endif
		return 0;
	}
	break;
	
	/////////////////////5.11
	//STR RD, [SP,#IMM]
	case(0x12):{
		//retrieve SP
		//exRegs[(0xd)]
		
		//RD
		//exRegs[((thumbinstr>>8)&0x7)]
		
		//#imm 
		u32 destroyableRegister = ((thumbinstr&0xff)<<2);
		cpuwrite_word((exRegs[(0xd)]+destroyableRegister), exRegs[((thumbinstr>>8)&0x7)]);
		
		#ifdef DEBUGEMU
			printf("str rd(%d)[%x], [SP:(%x),#[%x]] ",(int)((thumbinstr>>8)&0x7), (unsigned int)exRegs[((thumbinstr>>8)&0x7)],(unsigned int)exRegs[(0xd)],(unsigned int)destroyableRegister);
		#endif
		
		//note: this opcode doesn't increase SP
		return 0;
	}
	break;
	
	//LDR RD, [SP,#IMM]
	case(0x13):{
		//retrieve SP
		//exRegs[(0xd)]
		
		//#imm 
		u32 destroyableRegister = ((thumbinstr&0xff)<<2);
		
		u32 destroyableRegister2 = cpuread_word(exRegs[(0xd)] + destroyableRegister);
		//save Rd
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister2;
		
		#ifdef DEBUGEMU
			printf("ldr rd(%d)[%x], [SP:(%x),#[%x]] ",(int)((thumbinstr>>8)&0x7),(unsigned int)destroyableRegister2,(unsigned int)exRegs[(0xd)],(unsigned int)destroyableRegister);
		#endif
		
		//note: this opcode doesn't increase SP
		return 0;
	}
	break;
	
	
	/////////////////////5.12
	//add #Imm to the current PC value and load the result in rd
	//ADD  Rd, [PC,#IMM] (5.12)
	case(0x14):{
		//PC
		//exRegs[(0xf)]
		
		//get #Imm
		//((thumbinstr&0xff)<<2)
		
		//Rd
		//exRegs[((thumbinstr>>8)&0x7)]
		
		//add with #imm
		u32 destroyableRegister = addsasm(exRegs[(0xf)], ((thumbinstr&0xff)<<2));
		u32 destroyableRegister2 = cpuread_word(destroyableRegister);
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister2;
		
		#ifdef DEBUGEMU
		printf("add rd(%d)[%x], [PC:(%x),#[%x]] (5.12) ",(int)((thumbinstr>>8)&0x7),(unsigned int)destroyableRegister2,(unsigned int)exRegs[(0xf)],(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		return 0;
	}
	break;
	
	//add #Imm to the current SP value and load the result in rd
	//ADD  Rd, [SP,#IMM] (5.12)
	case(0x15):{	
		//SP
		//exRegs[(0xd)]
		
		//get #Imm
		//((thumbinstr&0xff)<<2)
		
		//Rd
		//((thumbinstr>>8)&0x7)
		
		//add with #imm
		u32 destroyableRegister = addsasm(exRegs[(0xd)], ((thumbinstr&0xff)<<2));
		
		u32 destroyableRegister2 = cpuread_word(destroyableRegister);
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister2;
		
		#ifdef DEBUGEMU
		printf("add rd(%d)[%x], [r13:(%x),#[%x]] (5.12) ",(int)((thumbinstr>>8)&0x7),(unsigned int)destroyableRegister2,(unsigned int)exRegs[(0xd)],(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		return 0;
	}	
	break;
	
	/////////////////////5.15 multiple load store
	//STMIA rb!,{Rlist}
	case(0x18):{
		//Rb
		//exRegs[((thumbinstr>>8)&0x7)]
		
		//stack operation STMIA
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
				//stmia reg! is (forcefully for thumb) descendent
				cpuwrite_word(exRegs[((thumbinstr>>8)&0x7)]-(offset*4), exRegs[(1<<cntr)]); //word aligned
				offset++;
			}
			cntr++;
		}
		
		//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
		u32 destroyableRegister = (u32)subsasm((u32)exRegs[((thumbinstr>>8)&0x7)],(lutu16bitcnt(thumbinstr&0xff))*4);		//get decimal value from registers selected
		
		//writeback always the new Rb
		exRegs[((thumbinstr>>8)&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("STMIA r%d![%x], {R: %d %d %d %d %d %d %d %x }:regs op:%x (5.15)",
		(int)(thumbinstr>>8)&0x7,
		(unsigned int)exRegs[((thumbinstr>>8)&0x7)],
		(int)(thumbinstr&0xff)&0x80,
		(int)(thumbinstr&0xff)&0x40,
		(int)(thumbinstr&0xff)&0x20,
		(int)(thumbinstr&0xff)&0x10,
		(int)(thumbinstr&0xff)&0x08,
		(int)(thumbinstr&0xff)&0x04,
		(int)(thumbinstr&0xff)&0x02,
		(int)(thumbinstr&0xff)&0x01,
		(unsigned int)(thumbinstr&0xff)
		);
		#endif
		
		return 0;
	}
	break;
	
	//LDMIA rd!,{Rlist}
	case(0x19):{
		//Rb
		//exRegs[((thumbinstr>>8)&0x7)]
		
		#ifdef DEBUGEMU
		printf("LDMIA r%d![%x], {R: %d %d %d %d %d %d %d %d }:regs op:%x (5.15)",
		(int)((thumbinstr>>8)&0x7),
		(unsigned int)exRegs[((thumbinstr>>8)&0x7)],
		(int)(thumbinstr&0xff)&0x80,
		(int)(thumbinstr&0xff)&0x40,
		(int)(thumbinstr&0xff)&0x20,
		(int)(thumbinstr&0xff)&0x10,
		(int)(thumbinstr&0xff)&0x08,
		(int)(thumbinstr&0xff)&0x04,
		(int)(thumbinstr&0xff)&0x02,
		(int)(thumbinstr&0xff)&0x01,
		(unsigned int)(thumbinstr&0xff)
		);
		#endif
		
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
				//ldmia reg! is (forcefully for thumb) ascendent
				cpuwrite_word(exRegs[((thumbinstr>>8)&0x7)]+(offset*4), exRegs[(1<<cntr)]); //word aligned
				offset++;
			}
			cntr++;
		}
		//update Rb <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
		exRegs[((thumbinstr>>8)&0x7)] = (u32)addsasm((u32)exRegs[((thumbinstr>>8)&0x7)],(lutu16bitcnt(thumbinstr&0xff))*4);		//get decimal value from registers selected
		
		return 0;
	}
	break;
	
	
	//				5.18 BAL (branch always) PC-Address (+/- 2048 bytes)
	//must be half-word aligned (bit 0 set to 0)
	case(0x1c):{
			//address patch is required for virtual environment
			//hword=addresslookup(hword, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (hword & 0x3FFFF);
			
			exRegs[0xf]=(cpuread_word((thumbinstr&0x3ff)<<1)+0x4&0xfffffffe); //bit[11] but word-aligned so assembler puts 0>>1
			#ifdef DEBUGEMU
			printf("[BAL] label[%x] THUMB mode / CPSR:%x (5.18) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
	return 0;	
	}
	break;
	
	
	//	5.19 long branch with link
	
	case(0x1e):{
	
		/* //deprecated branch with link
		//1 / 2
		//H bit[11<0]
		//Instruction 1:
		//In the first instruction the offset field contains the upper 11 bits
		//of the target address. This is shifted left by 12 bits and added to
		//the current PC address. The resulting address is placed in LR.
	
		//offset high part
		exRegs[0xe]=((thumbinstr&0x7ff)<<12)+((exRegs[0xf]&0xfffffffe));
	
		//2 / 2
		//H bit[11<1]
		//Instruction 2:
		//In the second instruction the offset field contains an 11-bit 
		//representation lower half of the target address. This is shifted
		//by 1 bit and added to LR. LR which now contains the full 23-bit
		//address, is placed in PC, the address of the instruction following
		//the BL is placed in LR and bit[0] of LR is set.
	
		//The branch offset must take account of the prefetch operacion, which
		//causes the PC to be 1 word (4 bytes) ahead of the current instruction.
	
		//fetch 2/2
		//(dual u16 reads are broken so we cast a u32 read once again for the sec part)
		#ifndef ROMTEST
			u32 u32read=stream_readu32((exRegs[0xf]&0xfffffffe) ^ 0x08000000);
		#endif
		#ifdef ROMTEST
			u32 u32read=(u32)*(u32*)(&rom_pl_bin+(((exRegs[0xf]&0xfffffffe) ^ 0x08000000)/4 ));
		#endif
		
		u32 temppc=(exRegs[0xf]&0xfffffffe);
	
		//rebuild PC from LR + (low part) + prefetch (0x4-2 because PC will +2 after this)
		exRegs[0xf]=(exRegs[0xe]+(((u32read>>16)&0x7ff)<<1)+(0x2)&0xfffffffe);
	
		//update LR
		exRegs[0xe]=((temppc+(0x4)) | (1<<0));
		#ifdef DEBUGEMU
			printf("LONG BRANCH WITH LINK: PC:[%x],LR[%x] (5.19) ",(unsigned int)((exRegs[0xf]&0xfffffffe)+(0x2)),(unsigned int)exRegs[0xe]);
		#endif
	
		*/
		
		//new branch with link
		
		/* //deprecated in favour of cpuread_word
		#ifndef ROMTEST
			u32 u32read=stream_readu32((exRegs[0xf]&0xfffffffe) ^ 0x08000000);
			u32read=rorasm(u32read,0x10); // :) fix
		#endif
		#ifdef ROMTEST
			u32 u32read=(u32)*(u32*)(&rom_pl_bin+(((exRegs[0xf]&0xfffffffe) ^ 0x08000000)/4 ));
		#endif
		*/
		u32 u32read=0;
		//if only a gbarom area read..
		if( (((uint32)(exRegs[0xf]&0xfffffffe)>>24) == 0x8) || (((uint32)(exRegs[0xf]&0xfffffffe)>>24) == 0x9) ){
			
			//new read method (reads from gba cpu core)
			u32read=cpuread_word((uint32)(exRegs[0xf]&0xfffffffe));
			
			#ifndef ROMTEST //gbareads (stream) are byte swapped, we swap bytes here if streamed data
				printf("byteswap gbaread! ");
				u32read=rorasm(u32read,0x10);
			#endif
		}
		else{
			
			//new read method (reads from gba cpu core)
			u32read=cpuread_word(( ((uint32)(exRegs[0xf]&0xfffffffe) >>2) )<<2);
			
		}
		
		printf("BL rom @(%x):[%x] ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)u32read);
		
		//original PC
		u32 oldpc=(exRegs[0xf]&0xfffffffe);
		
		//H = 0 (high ofst)
		u32 part1=(exRegs[0xf]&0xfffffffe)+((((u32read>>16)&0xffff)&0x7ff)<<12);
		exRegs[0xe]=part1;
		
		//H = 1 (low ofst)
		u32 part2=((((((u32read))&0xffff)&0x7ff) << 1) + exRegs[0xe] );
		exRegs[0xf] = (part2+(0x4)-0x2)&0xfffffffe; //prefetch - emulator alignment
		
		exRegs[0xe]= ((oldpc+(0x4)) | 1) &0xfffffffe; //+0x4 for prefetch
		
		#ifdef DEBUGEMU
			printf("LONG BRANCH WITH LINK: PC:[%x],LR[%x] (5.19) ",
			(unsigned int)((exRegs[0xf]&0xfffffffe)+0x2),(unsigned int)exRegs[0xe]); //+0x2 (pc++ fixup)
		#endif
		
		return 0;
		
	break;
	}
}

switch(thumbinstr>>9){
	
	//5.2
	//add rd, rs , rn
	case(0xc):{
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//rn
		//exRegs[((thumbinstr>>6)&0x7)]
		
		#ifdef DEBUGEMU
		printf("add rd(%d),rs(%d)[%x],rn(%d)[%x] (5.2)", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]);
		#endif
		
		u32 destroyableRegister = addasm(exRegs[((thumbinstr>>3)&0x7)], exRegs[((thumbinstr>>6)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//sub rd, rs, rn
	case(0xd):{
	
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//rn
		//exRegs[((thumbinstr>>6)&0x7)]
		
		#ifdef DEBUGEMU
		printf("sub r%d,r%d[%x],r%d[%x] (5.2)", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]);
		#endif
		u32 destroyableRegister = subasm(exRegs[((thumbinstr>>3)&0x7)], exRegs[((thumbinstr>>6)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//add rd, rs, #imm
	case(0xe):{
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//#imm
		//((thumbinstr>>6)&0x7)
		
		#ifdef DEBUGEMU
		printf("add r%d,r%d[%x],#0x%x (5.2)", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		u32 destroyableRegister = addasm(exRegs[((thumbinstr>>3)&0x7)],(thumbinstr>>6)&0x7);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//sub rd, rs, #imm
	case(0xf):{
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("sub r(%d),r(%d)[%x],#0x%x (5.2)", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		u32 destroyableRegister = subasm(exRegs[((thumbinstr>>3)&0x7)], (thumbinstr>>6)&0x7);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	
	/////////////////////5.7
	
	//STR RD, [Rb,Ro]
	case(0x28):{ //40dec
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		//Rd
		//exRegs[(thumbinstr&0x7)]
		
		#ifdef DEBUGEMU
		printf("str rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		
		//store RD into [RB,#Imm]
		cpuwrite_word((exRegs[((thumbinstr>>3)&0x7)] + exRegs[((thumbinstr>>6)&0x7)]), exRegs[(thumbinstr&0x7)]);
		return 0;
	}	
	break;
	
	//STRB RD ,[Rb,Ro] (5.7) (little endian lsb <-)
	case(0x2a):{ //42dec
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		//Rd
		//exRegs[(thumbinstr&0x7)]
		
		//store RD into [RB,#Imm]
		cpuwrite_byte((exRegs[((thumbinstr>>3)&0x7)] + exRegs[((thumbinstr>>6)&0x7)]), (exRegs[(thumbinstr&0x7)]&0xff));
		
		#ifdef DEBUGEMU
		printf("strb rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		return 0;
	}	
	break;
	
	
	//LDR rd,[rb,ro] (correct method for reads)
	case(0x2c):{ //44dec
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		//Rd
		//exRegs[(thumbinstr&0x7)]
		u32 destroyableRegister = cpuread_word(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
		#ifdef DEBUGEMU
		printf("LDR rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		return 0;
	}
	break;
	
	//ldrb rd,[rb,ro]
	case(0x2e):{ //46dec
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		u32 destroyableRegister = cpuread_byte((exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]));
		
		//Rd
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("LDRB rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)destroyableRegister,
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		
		return 0;
	}
	break;
	
	//////////////////////5.8
	//halfword
	//printf("STRH RD ,[Rb,Ro] (5.8) "); //thumbinstr
	case(0x29):{ //41dec strh
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		//Rd
		//exRegs[(thumbinstr&0x7)]
		
		//store RD into [RB,#Imm]
		cpuwrite_hword((exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]), (exRegs[(thumbinstr&0x7)]&0xffff));	
		
		#ifdef DEBUGEMU
		printf("strh rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		return 0;
	}	
	break;
	
	// LDRH RD ,[Rb,Ro] (5.8)
	case(0x2b):{ //43dec ldrh
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		u32 destroyableRegister = cpuread_hword((exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]));
		
		//Rd
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("LDRB rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)",
		(int)(thumbinstr&0x7),(unsigned int)destroyableRegister,
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]
		);
		#endif
		
		return 0;
	}
	break;
	
	//LDSB RD ,[Rb,Ro] (5.8)
	
	case(0x2d):{ //45dec ldsb	
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		s8 sbyte=cpuread_byte(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
		
		//Rd
		exRegs[(thumbinstr&0x7)] = (u32)sbyte;
		
		#ifdef DEBUGEMU
		printf("ldsb rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)",
		(int)(thumbinstr&0x7),(signed int)exRegs[(thumbinstr&0x7)],
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]);
		#endif
		
		return 0;
	}
	break;
	
	//LDSH RD ,[RS0,RS1] (5.8) //kept to use hardware opcode
	case(0x2f):{ //47dec ldsh
		//Rb
		//exRegs[((thumbinstr>>3)&0x7)]
		
		//Ro
		//exRegs[((thumbinstr>>6)&0x7)]
		
		s16 shword=cpuread_hword(exRegs[((thumbinstr>>3)&0x7)] + exRegs[((thumbinstr>>6)&0x7)]);
		
		//Rd
		exRegs[(thumbinstr&0x7)] = (u32)shword;
		
		#ifdef DEBUGEMU
		printf("ldsh rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)",
		(int)(thumbinstr&0x7),(signed int)shword,
		(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)],
		(int)((thumbinstr>>6)&0x7),(unsigned int)exRegs[((thumbinstr>>6)&0x7)]);
		#endif
		
		return 0;
	}
	break;
}

switch(thumbinstr>>8){
	///////////////////////////5.14
	//b: 10110100 = PUSH {Rlist} low regs (0-7)
	case(0xB4):{
		
		//gba stack method (stack pointer) / requires descending pointer
		//exRegs[(0xd)]
		
		#ifdef DEBUGEMU
		printf("[THUMB] PUSH {R: %d %d %d %d %d %d %d %x }:regout:%x (5.14)",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff))
		); 
		#endif
		//new
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//ldmia reg! is (forcefully for thumb) descendent
					cpuwrite_word(exRegs[(0xd)]-(offset*4), exRegs[(1<<cntr)]); //word aligned
					offset++;
				}
			cntr++;
		}
			
		//full descending stack
		u32 destroyableRegister = subsasm(exRegs[(0xd)],(lutu16bitcnt(thumbinstr&0xff))*4); 
		exRegs[(0xd)] = destroyableRegister;
		return 0;
	}
	break;
	
	//b: 10110101 = PUSH {Rlist,LR}  low regs (0-7) & LR
	case(0xB5):{
		//gba r13 descending stack operation
		//exRegs[(0xd)]
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x9){ //8 working low regs for thumb cpu 
			if(cntr!=0x8){
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//push is descending stack
					cpuwrite_word(exRegs[(0xd)]-(offset*4), exRegs[(1<<cntr)]); //word aligned
					offset++;
				}
			}
			else{ //our lr operator
				cpuwrite_word(exRegs[(0xd)]-(offset*4), exRegs[0xe]); //word aligned
				//#ifdef DEBUGEMU
				//	printf("offset(%x):LR! ",(int)cntr);
				//#endif
			}
			cntr++;
		}
		
		u32 destroyableRegister = subsasm(exRegs[(0xd)],(lutu16bitcnt(thumbinstr&0xff)+1)*4); //+1 because LR push
		exRegs[(0xd)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("[THUMB] PUSH {R: %x %x %x %x %x %x %x %x },LR :regout:%x (5.14)",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff)+0x1) //because LR
		);
		#endif
		return 0;
	}
	break;
	
	//b: 10111100 = POP  {Rlist} low regs (0-7)
	case(0xBC):{
		//gba r13 ascending stack operation
		//exRegs[(0xd)]
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		u32 destroyableRegister = addsasm(exRegs[(0xd)],(lutu16bitcnt(thumbinstr&0xff))*4);
		
		int cntr=0;
		int offset=0;
		while(cntr<0x8){ //8 working low regs for thumb cpu
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
				//pop is ascending
				exRegs[(1<<cntr)]=cpuread_word(destroyableRegister+(offset*4)); //word aligned
				offset++;
			}
			cntr++;
		}
		
		exRegs[(0xd)] = destroyableRegister;
		return 0;
	}
	break;
	
	//b: 10111101 = POP  {Rlist,PC} low regs (0-7) & PC
	case(0xBD):{
		//gba r13 ascending stack operation
		//exRegs[(0xd)]
		
		//restore is ascending (so fixup offset to restore n registers)
		u32 destroyableRegister = addsasm(exRegs[(0xd)],(lutu16bitcnt(thumbinstr&0xff))*4);
		
		int cntr=0;
		int offset=0;
		while(cntr<0x9){ //8 working low regs for thumb cpu
			if(cntr!=0x8){
				if(((1<<cntr) & (thumbinstr&0xff)) > 0){
					//restore is ascending (so Fixup stack offset address, to restore n registers)
					exRegs[(1<<cntr)]=cpuread_word(destroyableRegister+(offset*4)); //word aligned
					offset++;
				}
			}
			else{//our pc operator
				exRegs[0xf]=(cpuread_word(destroyableRegister+(offset*4))&0xfffffffe); //word aligned
			}
			cntr++;
		}
		return 0;
	}
	break;
	
	
	///////////////////5.16 Conditional branch
	//b: 1101 0000 / BEQ / Branch if Z set (equal)
	//BEQ label (5.16)
	
	//these read NZCV VIRT FLAGS (this means opcodes like this must be called post updatecpuregs(0);
	
	case(0xd0):{
		if (z_flag==1){
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BEQ] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BEQ not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0001 / BNE / Branch if Z clear (not equal)
	//BNE label (5.16)
	case(0xd1):{
		if (z_flag==0){ 
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BNE] label[%x] THUMB mode (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe)); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BNE not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0010 / BCS / Branch if C set (unsigned higher or same)
	//BCS label (5.16)
	case(0xd2):{
		if (c_flag==1){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BCS] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BCS not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0011 / BCC / Branch if C unset (lower)
	case(0xd3):{
		if (c_flag==0){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
	
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BCC] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BCC not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0100 / BMI / Branch if N set (negative)
	case(0xd4):{
		if (n_flag==1){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BMI] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
	
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BMI not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0101 / BPL / Branch if N clear (positive or zero)
	case(0xd5):{
		if (n_flag==0){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
		
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BPL] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BPL not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0110 / BVS / Branch if V set (overflow)
	case(0xd6):{
		if (v_flag==1){ 
			
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BVS] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BVS not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0111 / BVC / Branch if V unset (no overflow)
	case(0xd7):{
		if (v_flag==0){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BVC] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BVC not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1000 / BHI / Branch if C set and Z clear (unsigned higher)
	case(0xd8):{
		if ((c_flag==1)&&(z_flag==0)){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BHI] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else{
			#ifdef DEBUGEMU
			printf("THUMB: BHI not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1001 / BLS / Branch if C clr or Z Set (lower or same [zero included])
	case(0xd9):{
		if ((c_flag==0)||(z_flag==1)){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);	
			
			#ifdef DEBUGEMU
			printf("[BLS] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLS not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1010 / BGE / Branch if N set and V set, or N clear and V clear
	case(0xda):{
		if ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ){
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BGE] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BGE not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1011 / BLT / Branch if N set and V clear, or N clear and V set
	case(0xdb):{
		if ( ((n_flag==1)&&(v_flag==0)) || ((n_flag==0)&&(v_flag==1)) ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);	
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BLT] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLT not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1100 / BGT / Branch if Z clear, and either N set and V set or N clear and V clear
	case(0xdc):{
		if ( (z_flag==0) && ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ) ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BGT] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BGT not met! ");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1101 / BLE / Branch if Z set, or N set and V clear, or N clear and V set (less than or equal)
	case(0xdd):{	
		if ( ((z_flag==1) || (n_flag==1)) && ((v_flag==0) || ((n_flag==0) && (v_flag==1)) )  ){ 
		
			//address patch is required for virtual environment
			//dbyte_tmp=addresslookup(dbyte_tmp, (u32*)&addrpatches[0],(u32*)&addrfixes[0]) | (dbyte_tmp & 0x3FFFF);
			
			exRegs[0xf]=((cpuread_word((thumbinstr&0xff)<<1)+0x4)&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("[BLE] label[%x] THUMB mode / CPSR:%x (5.16) ",(unsigned int)(exRegs[0xf]&0xfffffffe),(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			printf("THUMB: BLE not met! ");
			#endif
		}
	return 0;	
	}
	break;
	
	
	//5.17 SWI software interrupt changes into ARM mode and uses SVC mode/stack (SWI 14)
	//SWI #Value8 (5.17)
	case(0xDF):{
		
		//printf("[thumb 1/2] SWI #(%x)",(unsigned int)cpsrvirt);
		armIrqEnable=false;
		
		u32 stack2svc=exRegs[0xe];	//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//ori: updatecpuflags(1,temp_arm_psr,0x13);
		updatecpuflags(1,cpsrvirt,0x13);
		
		exRegs[0xe]=stack2svc;		//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//we force ARM mode directly regardless cpsr
		armstate=0x0; //1 thmb / 0 ARM
		
		//printf("[thumb] SWI #0x%x / CPSR: %x(5.17)",(thumbinstr&0xff),cpsrvirt);
		swi_virt((thumbinstr&0xff));
		
		//if we don't use the BIOS handling, restore CPU mode inmediately
		#ifndef BIOSHANDLER
			//Restore CPU<mode> / SPSR (spsr_last) keeps SVC && restore SPSR T bit (THUMB/ARM mode)
				//note cpsrvirt is required because we validate always if come from same PSR mode or a different. (so stack swaps make sense)
			updatecpuflags(1,cpsrvirt | (((spsr_last>>5)&1)),spsr_last&0x1F);
		#endif
		
		//-0x2 because PC THUMB (exRegs[0xf]&0xfffffffe) alignment / -0x2 because prefetch
		#ifdef BIOSHANDLER
			exRegs[0xf]  = (u32)(0x08-0x2-0x2)&0xfffffffe;
		#else
			//otherwise executes a possibly BX LR (callback ret addr) -> PC increases correctly later
			//exRegs[0xf] = (u32)((exRegs[0xe]&0xfffffffe)-0x2-0x2);
		#endif
		
		armIrqEnable=true;
		
		//restore correct SPSR (deprecated because we need the SPSR to remember SVC state)
		//spsr_last=spsr_old;
		
		//printf("[thumb 2/2] SWI #(%x)",(unsigned int)cpsrvirt);
		
		//swi 0x13 (ARM docs)
		
	return 0;	
	}
	break;
}

switch(thumbinstr>>7){
	////////////////////////////5.13
	case(0x160):{ //dec 352 : add bit[9] depth #IMM to the SP, positive offset 	
		//gba stack method
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		u32 destroyableRegister = addsasm(exRegs[(0xd)], dbyte_tmp);
		exRegs[(0xd)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		printf("ADD SP:%x, +#%d (5.13) ",(unsigned int)exRegs[(0xd)],(signed int)dbyte_tmp);
		#endif
		
		return 0;
	}	
	break;
	
	case(0x161):{ //dec 353 : add bit[9] depth #IMM to the SP, negative offset
		//gba stack method
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		u32 destroyableRegister = subsasm(exRegs[(0xd)],dbyte_tmp);
		exRegs[(0xd)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		printf("ADD SP:%x, -#%d (5.13) ",(unsigned int)exRegs[(0xd)], (signed int) dbyte_tmp);
		#endif
		return 0;
	}
	break;
	
}

switch(thumbinstr>>6){
	//5.4
	//ALU OP: AND rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x100):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: AND r%d[%x], r%d[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = andasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)]=destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}	
	break;
	
	//ALU OP: EOR rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x101):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: EOR rd(%d)[%x], rs(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = eorasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: LSL rd(thumbinstr&0x7),rs((thumbinstr>>3)&0x7)
	case(0x102):{
		//rd:
		//exRegs[(thumbinstr&0x7)]
		
		//rs:
		//exRegs[((thumbinstr>>3)&0x7)]
		
		u32 destroyableRegister = lslasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("ALU OP: LSL r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: LSR rd, rs
	case(0x103):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: LSR r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = lsrasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: ASR rd, rs (5.4)
	case(0x104):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		u32 destroyableRegister = asrasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		#ifdef DEBUGEMU
		printf("ALU OP: ASR r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: ADC rd, rs (5.4)
	case(0x105):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: ADC r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = adcasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: SBC rd, rs (5.4)
	case(0x106):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: SBC r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = sbcasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: ROR rd, rs (5.4)
	case(0x107):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: ROR r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = rorasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: TST rd, rs (5.4) (and with cpuflag output only)
	case(0x108):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: TST rd(%d)[%x], rs(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = tstasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}	
	break;
	
	//ALU OP: NEG(pseudo)/RSB rd, rs (5.4)
	case(0x109):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		u32 destroyableRegister = negasm(exRegs[((thumbinstr>>3)&0x7)]);
		#ifdef DEBUGEMU
		printf("ALU OP: NEG rd(%d)[%x], rs%d[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: CMP rd, rs (5.4)
	case(0x10a):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: CMP rd(%d)[%x], rs(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = cmpasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: CMN rd, rs (5.4)
	case(0x10b):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: CMN rd(%d)[%x], rs(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = cmnasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: ORR rd, rs (5.4)
	case(0x10c):{	
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: ORR r%d[%x], r%d[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = orrasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: MUL rd, rs (5.4)
	case(0x10d):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: MUL r%d[%x], r%d[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		u32 destroyableRegister = mulasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		return 0;
	}
	break;
	
	//ALU OP: BIC rd, rs (5.4)
	case(0x10e):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("ALU OP: BIC r(%d)[%x], r(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		u32 destroyableRegister = bicasm(exRegs[(thumbinstr&0x7)], exRegs[((thumbinstr>>3)&0x7)]);
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//ALU OP: MVN rd, rs (5.4)
	case(0x10f):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		u32 destroyableRegister = mvnasm(exRegs[((thumbinstr>>3)&0x7)]);
		
		#ifdef DEBUGEMU
		printf("ALU OP: MVN rd(%d)[%x], rs(%d)[%x] (5.4)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);
		
		return 0;
	}
	break;
	
	
	//high regs <-> low regs
	////////////////////////////5.5
	//ADD rd,hs (5.5)
	case(0x111):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+8)]
		
		u32 destroyableRegister = addasm(exRegs[(thumbinstr&0x7)], exRegs[(((thumbinstr>>3)&0x7)+8)]);
		
		//these don't update CPSR flags
		#ifdef DEBUGEMU
		printf("HI reg ADD rd(%d)[%x], hs(%d)[%x] (5.5)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)(((thumbinstr>>3)&0x7)+8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+8)]);
		#endif
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)] = destroyableRegister;
		return 0;
	}
	break;

	//ADD hd,rs (5.5)	
	case(0x112):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		u32 destroyableRegister = addasm(exRegs[((thumbinstr&0x7)+0x8)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//these don't update CPSR flags
		
		#ifdef DEBUGEMU
		printf("HI reg op ADD hd%d[%x], rs%d[%x] (5.5)",(int)((thumbinstr&0x7)+0x8),(unsigned int)exRegs[(thumbinstr&0x7)+0x8],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		
		//done? update desired reg content
		exRegs[(thumbinstr&0x7)+0x8] = destroyableRegister;
		return 0;
	}
	break;
	
	//ADD hd,hs (5.5) 
	case(0x113):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+0x8)]
		
		#ifdef DEBUGEMU
		printf("HI reg op ADD hd%d[%x], hs%d[%x] (5.5)",(int)((thumbinstr&0x7)+0x8),(unsigned int)exRegs[((thumbinstr&0x7)+0x8)],(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		#endif
		u32 destroyableRegister = addasm(exRegs[((thumbinstr&0x7)+0x8)], exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//these don't update CPSR flags
		
		//done? update desired reg content
		exRegs[((thumbinstr&0x7)+0x8)] = destroyableRegister;
		return 0;
	}
	break;
	
	//CMP rd,hs (5.5)
	case(0x115):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+0x8)]
		
		#ifdef DEBUGEMU
		printf("HI reg op CMP rd%d[%x], hs%d[%x] (5.5)",(int)(thumbinstr&0x7),(unsigned int)exRegs[(thumbinstr&0x7)],(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		#endif
		u32 destroyableRegister = cmpasm(exRegs[(thumbinstr&0x7)], exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		//printf("CPSR:%x ",cpsrvirt);	
		return 0;
	}
	break;
	
	//CMP hd,rs (5.5)
	case(0x116):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("HI reg op CMP hd(%d)[%x], rs(%d)[%x] (5.5)",(int)((thumbinstr&0x7)+0x8),(unsigned int)exRegs[((thumbinstr&0x7)+0x8)],(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif
		u32 destroyableRegister = cmpasm(exRegs[((thumbinstr&0x7)+0x8)], exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//CMP hd,hs (5.5)  /* only CMP opcodes set CPSR flags */
	case(0x117):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+0x8)]
		
		#ifdef DEBUGEMU
		printf("HI reg op CMP hd%d[%x], hd%d[%x] (5.5)",(int)((thumbinstr&0x7)+0x8),(unsigned int)exRegs[((thumbinstr&0x7)+0x8)],(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		#endif
		u32 destroyableRegister = cmpasm(exRegs[((thumbinstr&0x7)+0x8)], exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//printf("CPSR:%x ",cpsrvirt);
		return 0;
	}
	break;
	
	//MOV
	//MOV rd,hs (5.5)	
	case(0x119):{
		//rd
		//exRegs[(thumbinstr&0x7)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+0x8)]
		
		#ifdef DEBUGEMU
		printf("mov rd(%d),hs(%d)[%x] ",(int)(thumbinstr&0x7),(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		#endif
		
		exRegs[(thumbinstr&0x7)] = movasm(exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//MOV Hd,Rs (5.5) 
	case(0x11a):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//rs
		//exRegs[((thumbinstr>>3)&0x7)]
		
		#ifdef DEBUGEMU
		printf("mov hd%d,rs%d[%x] ",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>3)&0x7),(unsigned int)exRegs[((thumbinstr>>3)&0x7)]);
		#endif

		u32 destroyableRegister = movasm(exRegs[((thumbinstr>>3)&0x7)]);
		exRegs[((thumbinstr&0x7)+0x8)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//MOV hd,hs (5.5)
	case(0x11b):{
		//hd
		//exRegs[((thumbinstr&0x7)+0x8)]
		
		//hs
		//exRegs[(((thumbinstr>>3)&0x7)+0x8)]
		
		#ifdef DEBUGEMU
		printf("mov hd(%d),hs(%d)[%x] ",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		#endif
		
		u32 destroyableRegister = movasm(exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		exRegs[((thumbinstr&0x7)+0x8)] = destroyableRegister;
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		return 0;
	}
	break;
	
	//						thumb BX branch exchange (rs)
	case(0x11c):{
		//rs
		//unlikely (r0-r7) will never be r15
		if(((thumbinstr>>0x3)&0x7)==0xf){
			//this shouldnt happen!
			#ifdef DEBUGEMU
				printf("thumb BX tried to be PC! (from RS) this is not supposed to HAPPEN!");
			#endif
			while(1);
		}
		
		u32 temppsr = cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((exRegs[((thumbinstr>>0x3)&0x7)]&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1, temppsr, temppsr&0x1f);
	
		exRegs[0xf]=(u32)(exRegs[((thumbinstr>>0x3)&0x7)]&0xfffffffe);
	
		#ifdef DEBUGEMU
			printf("BX rs(%d)[%x] -> PC[%x]! cpsr:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)exRegs[((thumbinstr>>0x3)&0x7)], exRegs[0xf], (unsigned int)temppsr);
		#endif
	
		return 0;
	}
	break;
	
	//						thumb BX (hs)
	case(0x11D):{
		//hs
		//exRegs[((thumbinstr>>0x3)&0x7)+0x8]
		
		//BX PC sets bit[0] = 0 and adds 4
		if((((thumbinstr>>0x3)&0x7)+0x8)==0xf){
			exRegs[((thumbinstr>>0x3)&0x7)+0x8]+=0x2; //+2 now because thumb prefetch will later add 2 more
		}
		
		u32 temppsr = cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((exRegs[((thumbinstr>>0x3)&0x7)+0x8]&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&0x1f);
		
		exRegs[0xf]=(u32)((exRegs[((thumbinstr>>0x3)&0x7)+0x8]&0xfffffffe)-0x2); //prefetch & align 2-boundary
	
		#ifdef DEBUGEMU
		printf("BX hs(%d)[%x]! cpsr:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)(exRegs[((thumbinstr>>0x3)&0x7)+0x8]&0xfffffffe), (unsigned int)temppsr);
		#endif
		return 0;
	}
	break;
}

//default:
//printf("unknown OP! %x",thumbinstr>>9); //debug
//break;


return thumbinstr;
}

/////////////////////////////////////ARM virt/////////////////////////////////////////
//5.1
u32 __attribute__ ((hot)) disarmcode(u32 arminstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
//drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//validate conditional execution flags:
switch((arminstr>>28)&0xf){
case(0):
	//z set EQ (equ)
	if(z_flag!=1){ //already cond_mode == negate current status (wrong)
		#ifdef DEBUGEMU
		printf("EQ not met! ");
		#endif
		return 0;
	}
	
break;

case(1):
//z clear NE (not equ)
	if(z_flag!=0){
		#ifdef DEBUGEMU
		printf("NE not met!");
		#endif
		return 0;
	}
break;

case(2):
//c set CS (unsigned higher)
	if(c_flag!=1) {
		#ifdef DEBUGEMU
		printf("CS not met!");
		#endif
		return 0;
	}
break;

case(3):
//c clear CC (unsigned lower)
	if(c_flag!=0){
		#ifdef DEBUGEMU
		printf("CC not met!");
		#endif
		return 0;
	}
break;

case(4):
//n set MI (negative)
	if(n_flag!=1){
		#ifdef DEBUGEMU
		printf("MI not met!");
		#endif
		return 0;
	}
break;

case(5):
//n clear PL (positive or zero)
	if(n_flag!=0) {
		#ifdef DEBUGEMU
		printf("PL not met!");
		#endif
		return 0;
	}
break;

case(6):
//v set VS (overflow)
	if(v_flag!=1) {
		#ifdef DEBUGEMU
		printf("VS not met!");
		#endif
		return 0;
	}
break;

case(7):
//v clear VC (no overflow)
	if(v_flag!=0){
		#ifdef DEBUGEMU
		printf("VC not met!");
		#endif
		return 0;
	}
break;

case(8):
//c set and z clear HI (unsigned higher)
	if((c_flag!=1)&&(z_flag!=0)){
		#ifdef DEBUGEMU
		printf("HI not met!");
		#endif
		return 0;
	}
break;

case(9):
//c clear or z set LS (unsigned lower or same)
	if((c_flag!=0)||(z_flag!=1)){
		#ifdef DEBUGEMU
		printf("LS not met!");
		#endif
		return 0;
	}
break;

case(0xa):
//(n set && v set) || (n clr && v clr) GE (greater or equal)
	if( ((n_flag!=1) && (v_flag!=1)) || ((n_flag!=0) && (v_flag!=0)) ) {
		#ifdef DEBUGEMU
		printf("GE not met!");
		#endif
		return 0;
	}
break;

case(0xb):
//(n set && v clr) || (n clr && v set) LT (less than)
	if( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ){
		#ifdef DEBUGEMU
		printf("LT not met!");
		#endif
		return 0;
	}
break;

case(0xc):
// (z clr) && ((n set && v set) || (n clr && v clr)) GT (greater than)
if( (z_flag!=0) && ( ((n_flag!=1) && (v_flag!=1))  || ((n_flag!=0) && (v_flag!=0)) ) ) {
	#ifdef DEBUGEMU
	printf("CS not met!");
	#endif
	return 0;
}
break;

case(0xd):
//(z set) || ((n set && v clear) || (n clr && v set)) LT (less than or equ)
if( (z_flag!=1) || ( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ) ) {
	#ifdef DEBUGEMU
	printf("CS not met!");
	#endif
	return 0;
}
break;

case(0xe): 
//always AL
break;

case(0xf):
//never NV
	return 0;
break;

default:
	return 0;
break;

}

//5.3 Branch & Branch with Link
switch(arminstr & 0xff000000){

	//EQ equal
	case(0x0a000000):{
		if (z_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BEQ ");
			#endif
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	}
	break;
	
	//NE not equal
	case(0x1a000000):
		if (z_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BNE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CS unsigned higher or same
	case(0x2a000000):
		if (c_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BCS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CC unsigned lower
	case(0x3a000000):
		if (c_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BCC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//MI negative
	case(0x4a000000):
	if (n_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BMI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//PL Positive or Zero
	case(0x5a000000):
		if (n_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BPL ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VS overflow
	case(0x6a000000):
		if (v_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BVS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VC no overflow
	case(0x7a000000):
		if (v_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BVC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//HI usigned higher
	case(0x8a000000):
		if ( (c_flag==1) && (z_flag==0) ){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BHI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LS unsigned lower or same
	case(0x9a000000):
		if ( (c_flag==0) || (z_flag==1) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BLS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GE greater or equal
	case(0xaa000000):
		if ( ((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BGE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LT less than
	case(0xba000000):
		if ( ((n_flag==1) && (v_flag==0)) || ((n_flag==0) && (v_flag==1)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BLT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GT greather than
	case(0xca000000):
		if ( (z_flag==0) && (((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0))) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BGT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LE less than or equal
	case(0xda000000):
		if ( 	((z_flag==0) || ( (n_flag==1) && (v_flag==0))) 
				||
				((n_flag==0) && (v_flag==1) )
			){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			exRegs[0xf]=(u32)(s_word&0xfffffffe);
			
			#ifdef DEBUGEMU
			printf("(5.3) BLE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//AL always
	case(0xea000000):{
		
		//new
		s32 s_word=(((arminstr&0xffffff)<<2)+(int)(exRegs[0xf]&0xfffffffe)+(0x8)); //+/- 32MB of addressing for branch offset / prefetch is considered here
		//after that LDR is required (requires to be loaded on a register).
		//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
		exRegs[0xf]=(u32)((s_word-0x4)&0xfffffffe); //fixup next IP handled by the emulator
		
		
		#ifdef DEBUGEMU
		printf("(5.3) BAL ");
		#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
	}
	return 0;
	break;
	
	//NV never
	case(0xf):
	
	#ifdef DEBUGEMU
	printf("(5.3) BNV ");
	#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				exRegs[(0xe)] = exRegs[(0xf)] + 0x8;
				#ifdef DEBUGEMU
				printf("link bit!");
				#endif
			}
	return 0;
	break;
	
}

//ARM 5.3 b (Branch exchange) BX
switch(((arminstr) & 0x012fff10)){
	case(0x012fff10):
	//Rn
	//exRegs[((arminstr)&0xf)]
	
	if(((arminstr)&0xf)==0xf){
		printf("BX rn%d[%x]! PC is undefined behaviour!",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)]);
		while(1);
	}
	u32 temppsr;
	
	exRegs[0xf]=(u32)(exRegs[((arminstr)&0xf)]&0xfffffffe);
	
	//(BX ARM-> THUMB value will always add +4)
	if((exRegs[((arminstr)&0xf)]&0x1)==1){
		//bit[0] RAM -> bit[5] PSR
		temppsr=((exRegs[((arminstr)&0xf)]&0x1)<<5)	| cpsrvirt;		//set bit[0] rn -> PSR bit[5]
		exRegs[0xf] = (((uint32)(exRegs[0xf]&0xfffffffe)) & ~(1<<0))&0xfffffffe; //align to log2 (because memory access/struct are always 4 byte)
		exRegs[0xf]+=0x2;   //align for next THUMB opcode
	}
	else{
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5]
		//alignment for ARM case is done below
	}
	
	//due ARM's ARM mode post +0x4 opcode execute, instruction pointer is reset
	exRegs[0xf]-=(0x4);
	
	//set CPU <mode> (included bit[5])
	updatecpuflags(1,temppsr,temppsr&0x1f);
	
	#ifdef DEBUGEMU
	printf("BX rn(%d)[%x]! set_psr:%x",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)temppsr);
	#endif
	
	return 0;
	break;
}

//5.4 ARM
u8 isalu=0;
//1 / 2 step for executing ARM opcode
//extract bit sequence of bit[20] && bit[25] and perform count
if (((arminstr>>26)&3)==0) {
	
	switch((arminstr>>20)&0x21){
		
		case(0x0):{
			
			//prevent MSR/MRS opcodes mistakenly call 5.4
			if( 
			((arminstr&0x3f0000) == 0xf0000) ||
			((arminstr&0x3ff000) == 0x29f000) ||
			((arminstr&0x3ff000) == 0x28f000 )
			){
				break;
			}
			//it is opcode such..
			//MOV register!
			if(((arminstr>>21)&0xf)==0xd){
				isalu=1;
			}
		}
		
		case(0x1):{
			setcond_arm=1;
			immop_arm=0;
			isalu=1;
		}
		break;
		case(0x20):{
			setcond_arm=0;
			immop_arm=1;
			isalu=1;
		}
		break;
		case(0x21):{
			setcond_arm=1;
			immop_arm=1;
			isalu=1;
		}
		break;
	}
	//printf("ARM opcode output %x ",((arminstr>>20)&0x21));
}
//printf("s:%d,i:%d ",setcond_arm,immop_arm);


//2 / 2 process ARM opcode by set bits earlier
if(isalu==1){

//printf("ARM opcode output %x ",((arminstr>>21)&0xf));
	
	switch((arminstr>>21)&0xf){
	
		//AND rd,rs / AND rd,#Imm
		case(0x0):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister2 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				u32 DestroyableRegister3=andasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
			
				//rd destination reg	 bit[15]---bit[12]
				faststr((u8*)&DestroyableRegister3, exRegs, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
				printf("AND rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) ",
				(int)(arminstr>>12)&0xf,(unsigned int)DestroyableRegister3,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)),(unsigned int)DestroyableRegister2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister2 = 0;
				u32 DestroyableRegister3 = 0;
				
				if( ((DestroyableRegister2=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("DestroyableRegister2:%x",DestroyableRegister2);
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				exRegs[((arminstr>>12)&0xf)] = andasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister3);
				
				#ifdef DEBUGEMU
				printf("AND rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)",
				(int)(arminstr>>12)&0xf,(unsigned int)DestroyableRegister2,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)(arminstr)&0xf,(unsigned int)DestroyableRegister3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
			
			return 0;
		}
		break;
	
		//EOR rd,rs
		case(0x1):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister2 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				u32 DestroyableRegister3 = eorasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister3;
				#ifdef DEBUGEMU
				printf("EOR rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) ",
				(int)(arminstr>>12)&0xf,(unsigned int)DestroyableRegister3,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)),(unsigned int)DestroyableRegister2,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				DestroyableRegister3 = 0; 
				DestroyableRegister4 = 0; 
				if( ((DestroyableRegister3=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister3&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister3>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister3>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister4=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister3>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister3&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister3>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister4=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister3>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister3&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister3>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister4=asrasm(exRegs[((arminstr)&0xf)], (exRegs[((DestroyableRegister3>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister3&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister3>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister4=rorasm(exRegs[((arminstr)&0xf)], (exRegs[((DestroyableRegister3>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("DestroyableRegister3:%x",DestroyableRegister3);
					//lsl
					if((DestroyableRegister3&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister4=lslasm(exRegs[((arminstr)&0xf)], (DestroyableRegister3>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister3&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister4=lsrasm(exRegs[((arminstr)&0xf)], (DestroyableRegister3>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister3&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister4=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister3>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister3&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister4=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister3>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister3>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				u32 DestroyableRegister5 = eorasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister4);
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister5;
				#ifdef DEBUGEMU
				printf("EOR rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)",
				(int)(arminstr>>12)&0xf,(unsigned int)DestroyableRegister5,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister4
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
		return 0;
		}
		break;
	
		//sub rd,rs
		case(0x2):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				u32 DestroyableRegister2 = subasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister2;
				#ifdef DEBUGEMU
				printf("SUB rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister2,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
				
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//printf("DestroyableRegister1:%x",DestroyableRegister1);
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%x),#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				u32 DestroyableRegister3 = subasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister3;
				#ifdef DEBUGEMU
				printf("SUB rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister3,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
			return 0;
		}
		break;
	
		//rsb rd,rs
		case(0x3):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				u32 DestroyableRegister2=rsbasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister2;
				
				#ifdef DEBUGEMU
				printf("RSB rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister2,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1,
				(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x", DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = rsbasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				
				#ifdef DEBUGEMU
				printf("RSB rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x] (5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
			return 0;
		}
		break;
	
		//add rd,rs (addarm)
		case(0x4):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//PC directive (+0x8 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					//printf("[imm]PC fetch!");
					DestroyableRegister1+=0x8; //+0x8 for prefetch
				}
				
				//rd destination reg	 bit[15]---bit[12] ((arminstr>>12)&0xf)
				exRegs[(arminstr>>12)&0xf]=addasm(exRegs[((arminstr>>16)&0xf)],DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("ADD rd%d[%x]",(int)((arminstr>>12)&0xf),(unsigned int)exRegs[(arminstr>>12)&0xf]);
				#endif
				return 0;
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
				
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d),rs(%x)[%x] ",(int)((arminstr)&0xf),(int)((DestroyableRegister1>>4)&0xf),(int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x", DestroyableRegister1);
					
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
		
				//PC directive (+0x12 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					#ifdef DEBUGEMU
					printf("[reg]PC fetch!");
					#endif
					
					DestroyableRegister2=addasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2+(0x12)); //+0x12 for prefetch
			
					//check for S bit here and update (virt<-asm) processor flags
					if(setcond_arm==1)
						updatecpuflags(0,cpsrasm,0x0);
				}
				else{
					DestroyableRegister2=addasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
					//check for S bit here and update (virt<-asm) processor flags
					if(setcond_arm==1)
						updatecpuflags(0,cpsrasm,0x0);
				}
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[(arminstr>>12)&0xf]=DestroyableRegister2;
				
				#ifdef DEBUGEMU
				printf("ADD rd(%d)<-rn(%d)[%x],rm(%d)[%x] (5.4)",
				(int)((arminstr>>12)&0xf),
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
			return 0;
		}
		break;
	
		//adc rd,rs
		case(0x5):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)]=adcasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("ADC rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
				
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = adcasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				#ifdef DEBUGEMU
				printf("ADC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//sbc rd,rs
		case(0x6):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)]=sbcasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("SBC rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
				
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister2 = 0;
				u32 DestroyableRegister3 = 0;
				if( ((DestroyableRegister2=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister2:%x", DestroyableRegister2);
			
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = sbcasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister3);
				
				#ifdef DEBUGEMU
				printf("SBC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)((arminstr)&0xf),(unsigned int)DestroyableRegister3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//rsc rd,rs
		case(0x7):{
			
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister2 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)]=rscasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				
				#ifdef DEBUGEMU
				printf("RSC rd%d[%x]<-rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)),(unsigned int)DestroyableRegister2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16]
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister2 = 0;
				u32 DestroyableRegister3 = 0;
				
				if( ((DestroyableRegister2=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%x),rs(%x)[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister2:%x", DestroyableRegister2);
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm[%x],#imm[%x] ",(unsigned int)((arminstr)&0xf),(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = rscasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister3);
			
				#ifdef DEBUGEMU
				printf("RSC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//tst rd,rs //set CPSR
		case(0x8):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16]
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				tstasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("TST [and] rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
				
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2= 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x", DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				u32 DestroyableRegister3 = tstasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
					
				#ifdef DEBUGEMU
				printf("TST [%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(unsigned int)DestroyableRegister3,
				(int)((arminstr>>16)&0xf),(unsigned)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//teq rd,rs //set CPSR
		case(0x9):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister2 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				DestroyableRegister2 = teqasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				#ifdef DEBUGEMU
				printf("TEQ rn%d[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)),(unsigned int)DestroyableRegister2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister2 = 0;
				u32 DestroyableRegister3 = 0;
				if( ((DestroyableRegister2=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister2>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister2:%x",DestroyableRegister2);
			
					//lsl
					if((DestroyableRegister2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister3=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister2>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				teqasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister3);
				#ifdef DEBUGEMU
				printf("TEQ rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister3
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//cmp rd,rs //set CPSR
		case(0xa):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				cmpasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				#ifdef DEBUGEMU
				printf("CMP rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				cmpasm(exRegs[((arminstr>>16)&0xf)], exRegs[((arminstr)&0xf)]);
				#ifdef DEBUGEMU
				printf("CMP rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)]
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//cmn rd,rs //set CPSR
		case(0xb):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				cmnasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				#ifdef DEBUGEMU
				printf("CMN rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)),(unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				cmnasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				#ifdef DEBUGEMU
				printf("CMN rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//orr rd,rs	//set CPSR
		case(0xc):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)]=orrasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("ORR rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				DestroyableRegister1=orrasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
			
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister1;
				
				#ifdef DEBUGEMU
				printf("ORR rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister1,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//mov rd,rs
		case(0xd):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1=rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				u32 DestroyableRegister4=movasm(DestroyableRegister1);
				#ifdef DEBUGEMU
				printf("MOV rn(%d)[%x],#Imm[%x] (ror:%x[%x])(5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister4,
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
				//rd (1st op reg) 		 bit[19]---bit[16] 
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister4;
			}
			else{	//for Register (operand 2) operator / shift included
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//opcode rd,rm
				u32 DestroyableRegister4=movasm(DestroyableRegister2);
				
				//if PC align for prefetch
				if(((arminstr)&0xf)==0xf)
					DestroyableRegister4+=0x8;
				
				//rd (1st op reg) 		 bit[19]---bit[16]
				exRegs[((arminstr>>12)&0xf)]=DestroyableRegister4;
				
				#ifdef DEBUGEMU
				printf("MOV rd(%x)[%x],rm(%d)[%x] (5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister4,
				(int)((arminstr&0xfff) << 0x14) >> ((arminstr&0xfff) * 2),(unsigned int)DestroyableRegister2
				);
				#endif
				//check for S bit here and update (virt<-asm) processor flags
				if(setcond_arm==1)
					updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			}
			return 0;
		}
		break;
	
		//bic rd,rs
		case(0xe):{
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				
				//rd destination reg	 bit[15]---bit[12]
				exRegs[((arminstr>>12)&0xf)]=bicasm(exRegs[((arminstr>>16)&0xf)],DestroyableRegister1);
				
				#ifdef DEBUGEMU
				printf("BIC rd(%d)[%x]<-rn(%d)[%x],#Imm[%x](ror:%x[%x]) (5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)), (unsigned int)DestroyableRegister1
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x", DestroyableRegister1);
					
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//rd destination reg	 bit[15]---bit[12]
				DestroyableRegister1=bicasm(exRegs[((arminstr>>16)&0xf)], DestroyableRegister2);
				exRegs[((arminstr>>12)&0xf)] = DestroyableRegister1;
				
				#ifdef DEBUGEMU
				printf("BIC rd(%d)[%x]<-rn(%d)[%x],rm(%d)[%x](5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister1,
				(int)((arminstr>>16)&0xf),(unsigned int)exRegs[((arminstr>>16)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	
		//mvn rd,rs
		case(0xf):{
			
			if(immop_arm==1){	//for #Inmediate OP operate
				//rn (1st op reg) 		 bit[19]---bit[16] / unused because of #Imm 
				//exRegs[((arminstr>>16)&0xf)]
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				u32 DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				exRegs[((arminstr>>12)&0xf)]=mvnasm(DestroyableRegister1);
				#ifdef DEBUGEMU
				printf("MVN rn(%d)[%x],#Imm[%x] (ror:%x)(5.4) ",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(unsigned int)(arminstr&0xff),
				(unsigned int)DestroyableRegister1
				);
				#endif
			
				//rd (1st op reg) 		 bit[19]---bit[16] 
				//there isn't one
			}
			else{	//for Register (operand 2) operator / shift included	
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//exRegs[((arminstr>>16)&0xf)]
				
				//rm (operand 2 )		 bit[11]---bit[0]
				//exRegs[((arminstr)&0xf)]
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//printf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				u32 DestroyableRegister1 = 0;
				u32 DestroyableRegister2 = 0;
				if( ((DestroyableRegister1=((arminstr>>4)&0xfff)) &0x1) == 1){
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//rs loaded into dr4
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],rs(%d)[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(int)((DestroyableRegister1>>4)&0xf),(unsigned int)exRegs[((DestroyableRegister1>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(exRegs[((DestroyableRegister1>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//printf("DestroyableRegister1:%x",DestroyableRegister1);
			
					//lsl
					if((DestroyableRegister1&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lslasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((DestroyableRegister1&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=lsrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//asr
					else if ((DestroyableRegister1&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=asrasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//ror
					else if ((DestroyableRegister1&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						DestroyableRegister2=rorasm(exRegs[((arminstr)&0xf)],(DestroyableRegister1>>3)&0x1f);
						#ifdef DEBUGEMU
						printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((DestroyableRegister1>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//opcode rd,rm
				//rd (1st op reg) 		 bit[19]---bit[16] 
				exRegs[((arminstr>>12)&0xf)]=mvnasm(DestroyableRegister2);
				
				#ifdef DEBUGEMU
				printf("MVN rd(%d)[%x],rm(%d)[%x] (5.4)",
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],
				(int)((arminstr)&0xf),(unsigned int)DestroyableRegister2
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//printf(" CPSR:%x",cpsrvirt);
			return 0;
		}
		break;
	} //end switch conditional 5.4 flags

}//end if 5.4

//5.5 MRS / MSR (use TEQ,TST,CMN and CMP without S bit set)
//xxxxxx - xxxx / bit[21]--bit[12]
//printf("PSR:%x",((arminstr>>16)&0x3f));

switch((arminstr>>16)&0x3f){

	//only 11 bit (N,Z,C,V,I,F & M[4:0]) are defined for ARM processor
	//bits PSR(27:8:5) are reserved and must not be modified.
	//1) so reserved bits must be preserved when changing values in PSR
	//2) programs will never check the reserved status bits while PSR check.
	//so read, modify , write should be employed
	
	case(0xf):{ 		//MRS (transfer PSR to register)
		//printf("MRS (transf PSR to reg!) ");
		
		//source PSR is: CPSR & save cond flags
		if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
			#ifdef DEBUGEMU
			printf("CPSR save!:%x",(unsigned int)cpsrvirt);
			#endif
			exRegs[((arminstr>>12)&0xf)] = cpsrvirt;
		}
		//source PSR is: SPSR<mode> & save cond flags
		else{
			//printf("SPSR save!:%x",spsr_last);
			exRegs[((arminstr>>12)&0xf)]=spsr_last;
		}
		return 0;
	}
	break;
	
	case(0x29):{ 	//MSR (transfer reg content to PSR)
		//printf("MSR (transf reg to PSR!) ");
		//CPSR
		if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
			//dummy PSR
			u32 DestroyableRegister2 = (exRegs[(arminstr&0xf)]&0xf90f03ff); //important PSR bits
			#ifdef DEBUGEMU
			printf("CPSR restore!:%x",(unsigned int)DestroyableRegister2);
			#endif
			//cpsrvirt=DestroyableRegister2;
			
			//modified (cpu state id updated)
			updatecpuflags(1,cpsrvirt,DestroyableRegister2&0x1f);
		}
		//SPSR
		else{
			u32 DestroyableRegister2 = (exRegs[(arminstr&0xf)] & 0xf90f03ff);	//important PSR bits
			//dummy PSR
			#ifdef DEBUGEMU
			printf("SPSR restore!:%x",(unsigned int)DestroyableRegister2);
			#endif
			spsr_last=DestroyableRegister2;
		}
		return 0;
	}
	break;
	
	case(0x28):{ 	//MSR (transfer reg content or #imm to PSR flag bits)
		//printf("MRS (transf reg or #imm to PSR flag bits!) ");
		
		//CPSR
		u32 DestroyableRegister1 = 0;
		if( ((DestroyableRegister1=((arminstr>>22)&0x3ff)) &0x1) == 0){
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				//dummy PSR
				u32 DestroyableRegister2 = (exRegs[(arminstr&0xf)]&0xf90f03ff); //important PSR bits
				#ifdef DEBUGEMU
				printf("CPSR restore from rd(%d)!:%x ",(int)(arminstr&0xf),(unsigned int)DestroyableRegister2);
				#endif
				cpsrvirt=DestroyableRegister2;
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject	//so far
				//to rotate right by twice the value in rotate field:
				DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2));
				//printf("CPSR restore from #imm!:%x ", DestroyableRegister1);
				cpsrvirt=DestroyableRegister1;
			}
		
		}
		//SPSR
		else{
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				u32 DestroyableRegister2 = (exRegs[(arminstr&0xf)] &0xf90f03ff);	//important PSR bits
				//dummy PSR
				#ifdef DEBUGEMU
				printf("SPSR restore from rd(%d)!:%x  ",(int)(arminstr&0xf),(unsigned int)DestroyableRegister2);
				#endif
				spsr_last=DestroyableRegister2;
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				DestroyableRegister1 = rorasm(((arminstr&0xfff) << 0x14), ((arminstr&0xfff) * 2)); 
				#ifdef DEBUGEMU
				printf("SPSR restore from #imm!:%x ",(unsigned int)DestroyableRegister1);
				#endif
				spsr_last=DestroyableRegister1;
			}
		}
	return 0;
	}
	break;

}

//5.6 multiply and accumulate

//Condition flags
//If S is specified, these instructions:
//update the N and Z flags according to the result
//do not affect the V flag
//corrupt the C flag in ARMv4 and earlier
//do not affect the C flag in ARMv5 and later.

//MUL     r10,r2,r5
//MLA     r10,r2,r1,r5
//MULS    r0,r2,r2
//MULLT   r2,r3,r2
//MLAVCS  r8,r6,r3,r8

//take bit[27] ... bit[22] & 0 and bit[4] ... bit[0] = 9 for MUL opc 
switch( ((arminstr>>22)&0x3f) + ((arminstr>>4)&0xf) ){
	case(0x9):{
		//printf("MUL/MLA opcode! (5.6)");
		switch((arminstr>>20)&0x3){
			//btw: rn is ignored as whole
			//multiply only & dont alter CPSR cpu flags
			case(0x0):
				//rm
				//exRegs[(arminstr&0xf)]
				
				//rs
				//exRegs[((arminstr>>8)&0xf)]
				
				#ifdef DEBUGEMU
				printf("mul rd(%d),rm(%d)[%x],rs(%d)[%x]",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)exRegs[(arminstr&0xf)],
				(int)((arminstr>>8)&0xf),(unsigned int)exRegs[((arminstr>>8)&0xf)]
				);
				#endif
				
				//rd
				exRegs[((arminstr>>16)&0xf)]=mulasm(exRegs[(arminstr&0xf)], exRegs[((arminstr>>8)&0xf)]);
			break;
			
			//multiply only & set CPSR cpu flags
			case(0x1):
				//rm
				//exRegs[(arminstr&0xf)]
				
				//rs
				//exRegs[((arminstr>>8)&0xf)]
				
				#ifdef DEBUGEMU
				printf("mul rd(%d),rm(%d)[%x],rs(%d)[%x] (PSR s)",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)exRegs[(arminstr&0xf)],
				(int)((arminstr>>8)&0xf),(unsigned int)exRegs[((arminstr>>8)&0xf)]
				);
				#endif
				
				//rd
				exRegs[((arminstr>>16)&0xf)] = mulasm(exRegs[(arminstr&0xf)], exRegs[((arminstr>>8)&0xf)]);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
				
			break;
			
			//mult and accum & dont alter CPSR cpu flags
			case(0x2):
				//rm
				//exRegs[(arminstr&0xf)]
				
				//rs
				//exRegs[((arminstr>>8)&0xf)]
				
				//rn
				//exRegs[((arminstr>>12)&0xf)]
				
				#ifdef DEBUGEMU
				printf("mla rd(%d),rm(%d)[%x],rs(%d)[%x],rn(%d)[%x] ",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)exRegs[(arminstr&0xf)],
				(int)((arminstr>>8)&0xf),(unsigned int)exRegs[((arminstr>>8)&0xf)],
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)]
				);
				#endif
				
				//rd
				exRegs[((arminstr>>16)&0xf)]=mlaasm(exRegs[(arminstr&0xf)], exRegs[((arminstr>>8)&0xf)], exRegs[((arminstr>>12)&0xf)]);
				
			break;
			
			//mult and accum & set CPSR cpu flags
			case(0x3):
				//rm
				//exRegs[(arminstr&0xf)]
				
				//rs
				//exRegs[((arminstr>>8)&0xf)]
				
				//rn
				//exRegs[((arminstr>>12)&0xf)]
				
				#ifdef DEBUGEMU
				printf("mla rd(%d),rm(%d)[%x],rs(%d)[%x],rn(%d)[%x] (PSR s)",
				(int)((arminstr>>16)&0xf),
				(int)(arminstr&0xf),(unsigned int)exRegs[(arminstr&0xf)],
				(int)((arminstr>>8)&0xf),(unsigned int)exRegs[((arminstr>>8)&0xf)],
				(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)]
				);
				#endif
				
				//rd
				exRegs[((arminstr>>16)&0xf)]=mlaasm(exRegs[(arminstr&0xf)], exRegs[((arminstr>>8)&0xf)], exRegs[((arminstr>>12)&0xf)]);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
			break;
		}
	return 0;
	}
	break;
}

//5.7 LDR / STR
//take bit[26] & 0x40 (if set) and save contents if found 
u32 DestroyableRegister6 = 0;
switch( ((DestroyableRegister6=((arminstr>>20)&0xff)) &0x40) ){
	//it is indeed a LDR/STR opcode
	case(0x40):{
		//rd
		//exRegs[((arminstr>>12)&0xf)]
		
		//rn
		//exRegs[((arminstr>>16)&0xf), 32,0)]
		
		u32 DestroyableRegister3 = exRegs[((arminstr>>16)&0xf), 32,0)];
		
		//IF NOT INMEDIATE (i=1)
		//decode = DestroyableRegister6 / rd = exRegs[((arminstr>>12)&0xf)] / rn = DestroyableRegister3 / #imm|rm index /
		//post shifted OR #imm shifted = exRegs[((arminstr)&0xf)]
		
		u32 DestroyableRegister5 = 0;
		//DestroyableRegister5 = calculated address/value (inside #imm/reg)
		//If shifted register? (offset is register Rm)
		if((DestroyableRegister6&0x20)==0x20){
			//R15/PC must not be chosen at Rm!
			if (((arminstr)&0xf)==0xf) return 0;
			
			//rm (operand 2 )		 bit[11]---bit[0]
			//exRegs[((arminstr)&0xf)]
			
			//(currently at: shift field) rs shift opcode to Rm
			u32 destroyableRegister4 = 0;
			if( ((destroyableRegister4=((arminstr>>4)&0xff)) &0x1) == 1){
				//lsl
				if((destroyableRegister4&0x6)==0x0){
					//rs loaded
					u32 destroyableRegister = 0;
					fastldr((u8*)&destroyableRegister, exRegs, ((destroyableRegister4>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("LSL rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((destroyableRegister4>>4)&0xf),(unsigned int)destroyableRegister);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					DestroyableRegister5=lslasm(exRegs[((arminstr)&0xf)],(destroyableRegister&0xff));
				}
				//lsr
				else if ((destroyableRegister4&0x6)==0x2){
					//rs loaded
					u32 destroyableRegister = 0;
					fastldr((u8*)&destroyableRegister, exRegs, ((destroyableRegister4>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("LSR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((destroyableRegister4>>4)&0xf),(unsigned int)destroyableRegister);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					DestroyableRegister5=lsrasm(exRegs[((arminstr)&0xf)],(destroyableRegister&0xff));
				}
				//asr
				else if ((destroyableRegister4&0x6)==0x4){
					//rs loaded
					u32 destroyableRegister = 0;
					fastldr((u8*)&destroyableRegister, exRegs, ((destroyableRegister4>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("ASR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((destroyableRegister4>>4)&0xf),(unsigned int)destroyableRegister);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					DestroyableRegister5=asrasm(exRegs[((arminstr)&0xf)],(destroyableRegister&0xff));
				}
				//ror
				else if ((destroyableRegister4&0x6)==0x6){
					//rs loaded
					u32 destroyableRegister = 0;
					fastldr((u8*)&destroyableRegister, exRegs, ((destroyableRegister4>>4)&0xf), 32,0); 
					#ifdef DEBUGEMU
					printf("ROR rm(%d),rs(%d)[%x] ",(int)((arminstr)&0xf),(int)((destroyableRegister4>>4)&0xf),(unsigned int)destroyableRegister);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					DestroyableRegister5=rorasm(exRegs[((arminstr)&0xf)],(destroyableRegister&0xff));
				}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
			}
			//#Imm ammount shift & opcode to Rm
			else{
				//show arminstr>>4
				//printf("exRegs[((arminstr>>12)&0xf)]:%x",exRegs[((arminstr>>12)&0xf)]);
				
				//lsl
				if((destroyableRegister4&0x6)==0x0){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					DestroyableRegister5=lslasm(exRegs[((arminstr)&0xf)],((destroyableRegister4>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("LSL rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((destroyableRegister4>>3)&0x1f));
					#endif
				}
				//lsr
				else if ((destroyableRegister4&0x6)==0x2){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					DestroyableRegister5=lsrasm(exRegs[((arminstr)&0xf)],((destroyableRegister4>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("LSR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((destroyableRegister4>>3)&0x1f));
					#endif
				}
				//asr
				else if ((destroyableRegister4&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					DestroyableRegister5=asrasm(exRegs[((arminstr)&0xf)],((destroyableRegister4>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("ASR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((destroyableRegister4>>3)&0x1f));
					#endif
				}
				//ror
				else if ((destroyableRegister4&0x6)==0x6){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					DestroyableRegister5=rorasm(exRegs[((arminstr)&0xf)],((destroyableRegister4>>3)&0x1f));
					#ifdef DEBUGEMU
					printf("ROR rm(%d)[%x],#imm[%x] ",(int)((arminstr)&0xf),(unsigned int)exRegs[((arminstr)&0xf)],(unsigned int)((destroyableRegister4>>3)&0x1f));
					#endif
				}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
			}
		}
		
		//ELSE IF INMEDIATE (i=0)(absolute #Imm value)
		else{
			//#Imm value (operand 2)		 bit[11]---bit[0]
			DestroyableRegister5=(arminstr&0xfff);
			#ifdef DEBUGEMU
			printf(" #Imm(0x%x) ",(unsigned int)DestroyableRegister5);	
			#endif
		}
		
		//pre indexing bit 	(add offset before transfer)
		if((DestroyableRegister6&0x10)==0x10){
			
			//up / increase  (Rn+=Rm)
			if((DestroyableRegister6&0x8)==0x8){
				DestroyableRegister3+=DestroyableRegister5;
			}
		
			//down / decrease bit (Rn-=Rm)
			else{
				DestroyableRegister3-=DestroyableRegister5;
			}
			
			//pre indexed says base is updated [!] (writeback = 1)
			DestroyableRegister6 |= (1<<1);
			
			#ifdef DEBUGEMU
			printf("pre-indexed bit! ");
			#endif
		}
		
		//keep original rn and copy
		
		//1)LDR/STR exRegs[((arminstr>>12)&0xf)] as rd 
		
		//decode = DestroyableRegister6 / rd = exRegs[((arminstr>>12)&0xf)] / rn = DestroyableRegister3 / 
		//DestroyableRegister5 = either #Imm OR register offset
		
		//BEGIN MODIFIED
		
		//2a) If register opcode?
		if((DestroyableRegister6&0x20)==0x20){
		
			//bit[20]
			//STR
			if((DestroyableRegister6&0x1)==0x0){
				//transfer byte quantity
				if((DestroyableRegister6&0x4)==0x4){
					//dereference Rn+offset
					//store RD into [Rn,#Imm]
					cpuwrite_byte(DestroyableRegister3,(exRegs[((arminstr>>12)&0xf)]&0xff));
					#ifdef DEBUGEMU
						printf("ARM:5.7 trying STRB rd(%d), [b:rn(%d)[%x],xxx] (5.9) ",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister3);
					#endif
				}
				//transfer word quantity
				else{
					//store RD into [RB,#Imm]
					cpuwrite_word(DestroyableRegister3,exRegs[((arminstr>>12)&0xf)]);
					#ifdef DEBUGEMU
						printf("ARM:5.7 trying to STR rd(%d), [b:rn(%d)[%x],xxx] (5.9)",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister3);
					#endif
				}
			}
			
			//LDR is taken care on cpuxxx_xxx opcodes
			//LDR
			else{
				//transfer byte quantity
				if((DestroyableRegister6&0x4)==0x4){
					//dereference Rn+offset
					u32 DestroyableRegister1 = cpuread_byte(DestroyableRegister3);
					#ifdef DEBUGEMU
						printf(" GBA LDRB rd(%d)[%x], [#0x%x] (5.9)",
						(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister1,(unsigned int)(DestroyableRegister3));
					#endif
					exRegs[((arminstr>>12)&0xf)] = DestroyableRegister1;
				}
			
				//transfer word quantity
				else{
					//dereference Rn+offset
					u32 DestroyableRegister1=cpuread_word(DestroyableRegister3);
					
					#ifdef DEBUGEMU
						printf(" GBA LDR rd(%d)[%x], [#0x%x] (5.9)",
						(int)((arminstr>>12)&0xf),(unsigned int)DestroyableRegister1, (unsigned int)(DestroyableRegister3));
					#endif
					exRegs[((arminstr>>12)&0xf)] = DestroyableRegister1;
				}
			}
		}//end if register/shift opcode
		
		// END MODIFIED
		
		//2b) it's #Imm, (label) Rn or other inmediate value for STR/LDR
		else{
			//bit[20]
			//STR
			if((DestroyableRegister6&0x1)==0x0){
				//transfer byte quantity
				if((DestroyableRegister6&0x4)==0x4){
					#ifdef DEBUGEMU
					printf("STRB rd(%d)[%x],[rn(%d)](%x)",
					(int)((arminstr>>12)&0xf),(unsigned int)(exRegs[((arminstr>>12)&0xf)]&0xff),(int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister3);
					#endif
					//str rd,[rn]
					cpuwrite_byte(DestroyableRegister3, exRegs[((arminstr>>12)&0xf)]&0xff);
				}
				//word quantity
				else{
					#ifdef DEBUGEMU
					printf("STR rd(%d)[%x],[rn(%d)](%x)",
					(int)((arminstr>>12)&0xf),(unsigned int)exRegs[((arminstr>>12)&0xf)],(int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister3);
					#endif
					//str rd,[rn]
					cpuwrite_word(DestroyableRegister3, exRegs[((arminstr>>12)&0xf)]); //broken?
				}
			}
			
			//LDR : 
			else{
				u32 destroyableRegister = 0;
				//Transfer byte quantity
				if((DestroyableRegister6&0x4)==0x4){
					u32 DestroyableRegister1 = 0;
					
					//printf(" LDRB #imm");
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						DestroyableRegister1=(DestroyableRegister3+(0x8)); //align +8 for prefetching
						destroyableRegister=cpuread_byte(DestroyableRegister1);
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						DestroyableRegister1=DestroyableRegister3; //rd is destroyableRegister now, old rd is rewritten
						destroyableRegister=cpuread_byte(DestroyableRegister1);
					}
					
					#ifdef DEBUGEMU
						printf("LDRB rd(%d)[%x]<-LOADED [Rn(%d),#IMM]:(%x)",(int)((arminstr>>12)&0xf),(unsigned int)destroyableRegister,(unsigned int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister1);
					#endif
				}
				//Transfer word quantity
				else{
					u32 DestroyableRegister2=0;
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						DestroyableRegister2=(DestroyableRegister3+(0x8)); //align +8 for prefetching
						destroyableRegister=cpuread_word(DestroyableRegister2);
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						DestroyableRegister2=DestroyableRegister3; //rd is destroyableRegister now, old rd is rewritten
						destroyableRegister=cpuread_word(DestroyableRegister2);
					}
					
					#ifdef DEBUGEMU
						printf("LDR rd(%d)[%x]<-LOADED [Rn(%d),#IMM]:(%x)",(int)((arminstr>>12)&0xf),(unsigned int)destroyableRegister,(unsigned int)((arminstr>>16)&0xf),(unsigned int)DestroyableRegister2);
					#endif
				}
				faststr((u8*)&destroyableRegister, exRegs, ((arminstr>>12)&0xf), 32,0);
			}
		}
		
		
		//3)post indexing bit (add offset after transfer)
		if((DestroyableRegister6&0x10)==0x0){
			#ifdef DEBUGEMU
			printf("post-indexed bit!");
			#endif
			DestroyableRegister6&= ~(1<<1); //forces the writeback post indexed base to be zero (base address isn't updated, basically)
		}
		
		//4)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		// (ALWAYS (if specified) except when R15)
		if( ((DestroyableRegister6&0x2)==0x2) && (((arminstr>>16)&0xf)!=0xf) ){
			
			//up / increase  (Rn+=Rm)
			if((DestroyableRegister6&0x8)==0x8){
				DestroyableRegister3+=DestroyableRegister5;
			}
		
			//down / decrease bit (Rn-=Rm)
			else{
				DestroyableRegister3-=DestroyableRegister5;
			}
			
			#ifdef DEBUGEMU
			printf("(new) rn writeback base addr! [%x]",(unsigned int)DestroyableRegister3);
			#endif
			
			exRegs[((arminstr>>16)&0xf)] = DestroyableRegister3;
		}
		//else: don't write-back address into base
		#ifdef DEBUGEMU
		printf("/******************************/");
		#endif
	}
	break;
} //end 5.7

	
//5.8 STM/LDM
switch( ( (DestroyableRegister6=((arminstr>>20)&0xff)) & 0x80)  ){
	case(0x80):{
		u32 savedcpsr=0;
		u8 writeback=0;
		
		//rn
		//exRegs[((arminstr>>16)&0xf)]
		
		//1a)force 0x10 usr mode
		if( ((DestroyableRegister6&0x4)==0x4) && ((cpsrvirt&0x1f)!=0x10)){
			savedcpsr=cpsrvirt;
			updatecpuflags(1,cpsrvirt,0x10);
			#ifdef DEBUGEMU
			printf("FORCED TO USERMODE!CPSR: %x",(unsigned int)cpsrvirt);
			#endif
			writeback=1;
		}
		//else do not load PSR or force usr(0x10) mode
		
		int cntr=0; 	//CPU register enum
		int offset=0;
		
		//4)STM/LDM
		//bit[20]
		//STM
		if((DestroyableRegister6&0x1)==0x0){
			#ifdef DEBUGEMU
			printf("STMIA r(%d)[%x], {R: %d %d %d %d %d %d %d %x  %d %d %d %d %d %d %d %d } ",
			(int)((arminstr>>16)&0xf),
			(unsigned int)DestroyableRegister6,
			(int)((arminstr&0xffff)&0x8000),
			(int)((arminstr&0xffff)&0x4000),
			(int)((arminstr&0xffff)&0x2000),
			(int)((arminstr&0xffff)&0x1000),
			(int)((arminstr&0xffff)&0x800),
			(int)((arminstr&0xffff)&0x400),
			(int)((arminstr&0xffff)&0x200),
			(int)((arminstr&0xffff)&0x100),
			
			(int)((arminstr&0xffff)&0x80),
			(int)((arminstr&0xffff)&0x40),
			(int)((arminstr&0xffff)&0x20),
			(int)((arminstr&0xffff)&0x10),
			(int)((arminstr&0xffff)&0x08),
			(int)((arminstr&0xffff)&0x04),
			(int)((arminstr&0xffff)&0x02),
			(int)((arminstr&0xffff)&0x01));
			
			printf(":pushd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation STMIA
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((DestroyableRegister6&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((DestroyableRegister6&0x8)==0x8){
					#ifdef DEBUGEMU
						printf("asc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu 
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							//ascending stack
							cpuwrite_word(exRegs[((arminstr>>16)&0xf)]+(offset*4), exRegs[(1<<cntr)]); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						printf("desc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							//descending stack
							cpuwrite_word(exRegs[((arminstr>>16)&0xf)]-(offset*4), exRegs[(1<<cntr)]); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			}
		}
		
		//LDM
		else{
			#ifdef DEBUGEMU
			printf("LDMIA rd(%d)[%x], {R: %d %d %d %d %d %d %d %x  %d %d %d %d %d %d %d %d } ",
			(int)((arminstr>>16)&0xf),
			(unsigned int)DestroyableRegister6,
			(int)((arminstr&0xffff)&0x8000),
			(int)((arminstr&0xffff)&0x4000),
			(int)((arminstr&0xffff)&0x2000),
			(int)((arminstr&0xffff)&0x1000),
			(int)((arminstr&0xffff)&0x800),
			(int)((arminstr&0xffff)&0x400),
			(int)((arminstr&0xffff)&0x200),
			(int)((arminstr&0xffff)&0x100),
			(int)((arminstr&0xffff)&0x80),
			(int)((arminstr&0xffff)&0x40),
			(int)((arminstr&0xffff)&0x20),
			(int)((arminstr&0xffff)&0x10),
			(int)((arminstr&0xffff)&0x08),
			(int)((arminstr&0xffff)&0x04),
			(int)((arminstr&0xffff)&0x02),
			(int)((arminstr&0xffff)&0x01));
			printf(":pop'd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation LDMIA
			int cntr=0;
			int offset=0;
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((DestroyableRegister6&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((DestroyableRegister6&0x8)==0x8){
					#ifdef DEBUGEMU
						printf("asc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							exRegs[(1<<cntr)]=cpuread_word(exRegs[((arminstr>>16)&0xf)]+(offset*4)); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						printf("desc stack! ");
					#endif
					
					while(cntr<0x10){ //16 working registers for ARM cpu
						if(((1<<cntr) & (arminstr&0xffff)) > 0){
							exRegs[(1<<cntr)]=cpuread_word(exRegs[((arminstr>>16)&0xf)]-(offset*4)); //word aligned
							offset++;
						}
						cntr++;
					}
				}
			}
		}
		
		//4)post indexing bit (add offset after transfer) (by default on stmia/ldmia opcodes)
		if((DestroyableRegister6&0x10)==0x0){
			DestroyableRegister6|=0x2; //forces the writeback post indexed base
			#ifdef DEBUGEMU
			printf("post indexed (default)! ");
			#endif
		}
		
		//5)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		if((DestroyableRegister6&0x2)==0x2){
			u32 DestroyableRegister2 = 0;
			//if asc/ up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
			if((DestroyableRegister6&0x8)==0x8){
				DestroyableRegister2=(u32)addsasm((u32)exRegs[((arminstr>>16)&0xf)],(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			
			//else descending stack 1 bit
			else{
				DestroyableRegister2=(u32)subsasm((u32)exRegs[((arminstr>>16)&0xf)],(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			exRegs[((arminstr>>16)&0xf)]=DestroyableRegister2;
			
			#ifdef DEBUGEMU
			printf(" updated addr: %x / Bytes workd onto stack: %x ", (unsigned int)DestroyableRegister2, ((lutu32bitcnt(arminstr&0xffff))*4));
			#endif
		}
		//else: don't write-back address into base
		
		//1b)forced 0x10 usr mode go back to SPSR mode
		if(writeback==1){
			#ifdef DEBUGEMU
			printf("RESTORED MODE:CPSR %x",(unsigned int)savedcpsr);
			#endif
			updatecpuflags(1,cpsrvirt,savedcpsr&0x1f);
			writeback=0;
		}
		
	break;
	}
}

//5.9 SWP Single Data Swap (swp rd,rm,[rn])
switch( ( (DestroyableRegister6=(arminstr)) & 0x1000090)  ){
	case(0x1000090):{
		//printf("SWP opcode!");
		//rn (address)
		//exRegs[((DestroyableRegister6>>16)&0xf)]
		
		//rd is writeonly
		//rm
		//exRegs[((DestroyableRegister6)&0xf)]
		
		//swap byte
		if(DestroyableRegister6 & (1<<22)){
			//printf("byte quantity!");
			//[rn]->rd
			u32 DestroyableRegister2=cpuread_byte(exRegs[((DestroyableRegister6>>16)&0xf)]);
			exRegs[((DestroyableRegister6>>12)&0xf)]=DestroyableRegister2;
			#ifdef DEBUGEMU
			printf("SWPB 1/2 [rn(%d):%x]->rd(%d)[%x] ",(int)((DestroyableRegister6>>16)&0xf),(unsigned int)exRegs[((DestroyableRegister6>>16)&0xf)],(int)((DestroyableRegister6>>12)&0xf),(unsigned int)DestroyableRegister2);
			#endif
			//rm->[rn]
			cpuwrite_byte(exRegs[((DestroyableRegister6>>16)&0xf)],exRegs[((DestroyableRegister6)&0xf)]&0xff);
			#ifdef DEBUGEMU
			printf("SWPB 2/2 rm(%d):[%x]->[rn(%d):[%x]] ",(int)((DestroyableRegister6)&0xf),(unsigned int)(exRegs[((DestroyableRegister6)&0xf)]&0xff),(int)((DestroyableRegister6>>16)&0xf),(unsigned int)exRegs[((DestroyableRegister6>>16)&0xf)]);
			#endif
		}
		else{
			//printf("word quantity!");
			//[rn]->rd
			exRegs[((DestroyableRegister6>>12)&0xf)]=cpuread_word(exRegs[((DestroyableRegister6>>16)&0xf)]);
			#ifdef DEBUGEMU
			printf("SWP 1/2 rm(%d):[%x]->[rn(%d):[%x]] ",(int)((DestroyableRegister6)&0xf),(unsigned int)(exRegs[((DestroyableRegister6)&0xf)]&0xff),(int)((DestroyableRegister6>>16)&0xf),(unsigned int)exRegs[((DestroyableRegister6>>16)&0xf)]);
			#endif
			//rm->[rn]
			cpuwrite_word(exRegs[((DestroyableRegister6>>16)&0xf)],exRegs[((DestroyableRegister6)&0xf)]);
			#ifdef DEBUGEMU
			printf("SWP 2/2 rm(%d):[%x]->[rn(%d):[%x]] ",(int)((DestroyableRegister6)&0xf),(unsigned int)(exRegs[((DestroyableRegister6)&0xf)]&0xff),(int)((DestroyableRegister6>>16)&0xf),(unsigned int)exRegs[((DestroyableRegister6>>16)&0xf)]);
			#endif
		}
		
	}
}

//5.10 software interrupt
switch( (arminstr) & 0xf000000 ){
	case(0xf000000):{
		/*
		//required because SPSR saved is not SVC neccesarily
		//u32 spsr_old=spsr_last;
		
		//Enter SVC<mode>
		//updatecpuflags(1,cpsrvirt,0x13);
		
		//printf("CPSR(entrymode):%x ",cpsrvirt&0x1f);
		//#ifdef DEBUGEMU
		//printf("[ARM] swi call #0x%x! (5.10)",arminstr&0xffffff);
		//#endif
		//swi_virt(arminstr&0xffffff);
		
		//Restore CPU<mode>
		//updatecpuflags(1,cpsrvirt,spsr_last&0x1F);
		
		//printf("CPSR(restoremode):%x ",cpsrvirt&0x1f);
		
		//restore correct SPSR
		//spsr_last=spsr_old;
		*/
		
		#ifdef DEBUGEMU
		printf("[ARM] swi call #0x%x! (5.10)",(unsigned int)(arminstr&0xffffff));
		#endif
		
		armstate = 0;
		armIrqEnable=false;
		
		/* //deprecated (because we need the SPSR to remember SVC state)
		//required because SPSR saved is not SVC neccesarily
		u32 spsr_old=spsr_last; //(see below SWI bios case)
		*/
		
		updatecpuflags(1,cpsrvirt,0x13);
		
		//printf("CPSR(entrymode):%x ",cpsrvirt&0x1f);
		
		//printf("SWI #0x%x / CPSR: %x(5.17)",(thumbinstr&0xff),cpsrvirt);
		swi_virt(arminstr&0xffffff);
		
		exRegs[0xe] = (exRegs[0xf]&0xfffffffe) - (armstate ? 4 : 2);
		
		#ifdef BIOSHANDLER
			exRegs[0xf] = (u32)(0x08-0x4);
		#else
			//exRegs[0xf] = (u32)(exRegs[0xe]-0x4);
			//continue til BX LR (ret address cback)
		#endif
		
		//printf("swi%x",(unsigned int)(arminstr&0xff));
		
		//we let SWI bios decide when to go back from SWI mode
		//Restore CPU<mode>
		updatecpuflags(1,cpsrvirt,spsr_last&0x1F);
		
		/* //deprecated	(because we need the SPSR to remember SVC state)
		//restore correct SPSR
		spsr_last=spsr_old;
		*/
	}
}

//5.11 (Co- Processor calls) gba has not CP

//5.12 CDP not implemented on gba

//5.13 STC / LDC (copy / restore memory to cause undefined exception) not emulated

//5.14 MRC / MCR not implemented / used on GBA ( Co - processor)

//5.15 UNDEFINED INSTRUCTION (conditionally executed). If true it will be executed
switch(arminstr & 0x06000010){
	case(0x06000010):
	//printf("undefined instruction!");
	exceptundef(arminstr);
	break;
}

return arminstr;
}