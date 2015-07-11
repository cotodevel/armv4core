//coto: this translator is my work.

#include "translator.h"

#include "opcode.h"
#include "util.h"

#include "..\pu\pu.h"
#include "..\util\gbacore.h"

#include "..\settings.h"
#include "../disk/stream_disk.h"

#include "..\arm9.h"

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
		//iprintf("(0)CPSR output: %x \n",cpsr);
		//iprintf("(0)cpu flags: Z[%x] N[%x] C[%x] V[%x] \n",z_flag,n_flag,c_flag,v_flag);
	
	break;
	
  	case (1):{
		//1)check if cpu<mode> swap does not come from the same mode
		if((cpsr&0x1f)!=cpumode){
	
		//2a)save stack frame pointer for current stack 
		//and detect old cpu mode from current loaded stack, then store LR , PC into stack (mode)
		//iprintf("cpsr:%x / spsr:%x",cpsr&0x1f,spsr_last&0x1f);
			
			//backup cpumode begin
			
			switch(cpsrvirt&0x1f){
				case(0x10): case(0x1f):
					spsr_last=spsr_usr=cpsrvirt;
					//gbastckadr_usr=gbastckmodeadr_curr=; //not required this is to know base stack usr/sys
					gbastckfp_usr=gbastckfpadr_curr;
					gbavirtreg_r13usr[0x0]=gbavirtreg_cpu[0xd]; //user/sys is the same stacks
					gbavirtreg_r14usr[0x0]=gbavirtreg_cpu[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13usr[0x0]);
					#endif
					//iprintf("before nuked SP usr:%x",(unsigned int)gbavirtreg_r13usr[0x0]);
				break;
				
				case(0x11):
					spsr_last=spsr_fiq=cpsrvirt;
					gbastckfp_fiq=gbastckfpadr_curr;
					gbavirtreg_r13fiq[0x0]=gbavirtreg_cpu[0xd];
					gbavirtreg_r14fiq[0x0]=gbavirtreg_cpu[0xe];
					
					//save
					//5 extra regs r8-r12 for fiq
					gbavirtreg_fiq[0x0] = gbavirtreg_cpu[0x0 + 8];
					gbavirtreg_fiq[0x1] = gbavirtreg_cpu[0x1 + 8];
					gbavirtreg_fiq[0x2] = gbavirtreg_cpu[0x2 + 8];
					gbavirtreg_fiq[0x3] = gbavirtreg_cpu[0x3 + 8];
					gbavirtreg_fiq[0x4] = gbavirtreg_cpu[0x4 + 8];
					
					//restore 5 extra reg subset for other modes
					gbavirtreg_cpu[0x0 + 8]=gbavirtreg_cpubup[0x0];
					gbavirtreg_cpu[0x1 + 8]=gbavirtreg_cpubup[0x1];
					gbavirtreg_cpu[0x2 + 8]=gbavirtreg_cpubup[0x2];
					gbavirtreg_cpu[0x3 + 8]=gbavirtreg_cpubup[0x3];
					gbavirtreg_cpu[0x4 + 8]=gbavirtreg_cpubup[0x4];
					#ifdef DEBUGEMU
						iprintf("stacks backup fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13fiq[0x0]);
					#endif
				break;
				
				case(0x12):
					spsr_last=spsr_irq=cpsrvirt;
					gbastckfp_irq=gbastckfpadr_curr;
					gbavirtreg_r13irq[0x0]=gbavirtreg_cpu[0xd];
					gbavirtreg_r14irq[0x0]=gbavirtreg_cpu[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13irq[0x0]);
					#endif
				break;
				
				case(0x13):
					spsr_last=spsr_svc=cpsrvirt;
					gbastckfp_svc=gbastckfpadr_curr;
					gbavirtreg_r13svc[0x0]=gbavirtreg_cpu[0xd];
					gbavirtreg_r14svc[0x0]=gbavirtreg_cpu[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13svc[0x0]);
					#endif
				break;
				
				case(0x17):
					spsr_last=spsr_abt=cpsrvirt;
					gbastckfp_abt=gbastckfpadr_curr;
					gbavirtreg_r13abt[0x0]=gbavirtreg_cpu[0xd];
					gbavirtreg_r14abt[0x0]=gbavirtreg_cpu[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13abt[0x0]);
					#endif
				break;
				
				case(0x1b):
					spsr_last=spsr_und=cpsrvirt;
					gbastckfp_und=gbastckfpadr_curr;
					gbavirtreg_r13und[0x0]=gbavirtreg_cpu[0xd];
					gbavirtreg_r14und[0x0]=gbavirtreg_cpu[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_r13und[0x0]);
					#endif
				break;
				
				default:
					// disable FIQ & set SVC
					//gba->reg[16].I |= 0x40;
					cpsr|=0x40;
					cpsr|=0x13;
					
					//#ifdef DEBUGEMU
					//	iprintf("ERROR CHANGING CPU MODE/STACKS : CPSR: %x \n",(unsigned int)cpsrvirt);
					//	while(1);
					//#endif
				break;
			}
			
			//backup cpumode end
			
		//update SPSR on CPU change <mode> (this is exactly where CPU change happens)
		spsr_last=cpsr;
		
		//3)setup current CPU mode working set of registers and perform volatile stack/registers per mode swap
		switch(cpumode&0x1f){
			//user/sys mode
			case(0x10): case(0x1f):
				gbastckmodeadr_curr=gbastckadr_usr;
				gbastckfpadr_curr=gbastckfp_usr;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13usr[0x0]; //user SP/LR registers for cpu<mode> (user/sys is the same stacks)
				gbavirtreg_cpu[0xe]=gbavirtreg_r14usr[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to usr_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			//fiq mode
			case(0x11):
				gbastckmodeadr_curr=gbastckadr_fiq;
				gbastckfpadr_curr=gbastckfp_fiq;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13fiq[0x0]; //fiq SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14fiq[0x0];
				
				//save register r8-r12 subset before entering fiq
				gbavirtreg_cpubup[0x0]=gbavirtreg_cpu[0x0 + 8];
				gbavirtreg_cpubup[0x1]=gbavirtreg_cpu[0x1 + 8];
				gbavirtreg_cpubup[0x2]=gbavirtreg_cpu[0x2 + 8];
				gbavirtreg_cpubup[0x3]=gbavirtreg_cpu[0x3 + 8];
				gbavirtreg_cpubup[0x4]=gbavirtreg_cpu[0x4 + 8];
				
				//restore: 5 extra regs r8-r12 for fiq restore
				gbavirtreg_cpu[0x0 + 8]=gbavirtreg_fiq[0x0];
				gbavirtreg_cpu[0x1 + 8]=gbavirtreg_fiq[0x1];
				gbavirtreg_cpu[0x2 + 8]=gbavirtreg_fiq[0x2];
				gbavirtreg_cpu[0x3 + 8]=gbavirtreg_fiq[0x3];
				gbavirtreg_cpu[0x4 + 8]=gbavirtreg_fiq[0x4];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to fiq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			//irq mode
			case(0x12):
				gbastckmodeadr_curr=gbastckadr_irq;
				gbastckfpadr_curr=gbastckfp_irq;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13irq[0x0]; //irq SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14irq[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to irq_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			//svc mode
			case(0x13):
				gbastckmodeadr_curr=gbastckadr_svc;
				gbastckfpadr_curr=gbastckfp_svc;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13svc[0x0]; //svc SP/LR registers for cpu<mode> (user/sys is the same stacks)
				gbavirtreg_cpu[0xe]=gbavirtreg_r14svc[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to svc_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			//abort mode
			case(0x17):
				gbastckmodeadr_curr=gbastckadr_abt;
				gbastckfpadr_curr=gbastckfp_abt;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13abt[0x0]; //abt SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14abt[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to abt_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			//undefined mode
			case(0x1b):
				gbastckmodeadr_curr=gbastckadr_und;
				gbastckfpadr_curr=gbastckfp_und;			//current framepointer address (setup in util.c) and updated here
				gbavirtreg_cpu[0xd]=gbavirtreg_r13und[0x0]; //und SP/LR registers for cpu<mode>
				gbavirtreg_cpu[0xe]=gbavirtreg_r14und[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to und_psr:%x:(%x)",(unsigned int)cpumode&0x1f,(unsigned int)gbavirtreg_cpu[0xd]);
				#endif
			break;
			
		}
		//end if cpumode
		
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
			iprintf("cpsr(arg1)(%x) == cpsr(arg2)(%x)",(unsigned int)(cpsr&0x1f),(unsigned int)cpumode);
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
drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//testing gba accesses translation to allocated ones
//iprintf("output: %x \n",addresslookup( 0x070003ff, (u32*)&addrpatches[0],(u32*)&addrfixes[0]));

//debug addrfixes
//int i=0;
//for(i=0;i<16;i++){
//	iprintf("\n patch : %x",*((u32*)&addrfixes+(i)));
//	if (i==15) iprintf("\n");
//}

//Low regs
switch(thumbinstr>>11){
	////////////////////////5.1
	//LSL opcode
	case 0x0:
		gbavirtreg_cpu[(thumbinstr&0x7)]=lslasm(gbavirtreg_cpu[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] LSL r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//LSR opcode
	case 0x1: 
		gbavirtreg_cpu[(thumbinstr&0x7)]=lsrasm(gbavirtreg_cpu[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] LSR r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//ASR opcode
	case 0x2:
		gbavirtreg_cpu[(thumbinstr&0x7)]=asrasm(gbavirtreg_cpu[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] ASR r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//5.3 (this sets CPSR bits)
	//mov #imm bit[0-8] / move 8 bit value into rd
	case 0x4:
		//mov
		#ifdef DEBUGEMU
		iprintf("[THUMB] MOV r(%d),(#0x%x) (5.3)\n",(int)((thumbinstr>>8)&0x7),(unsigned int)thumbinstr&0xff);
		#endif
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=movasm((u32)(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//cmp / compare contents of rd with #imm bit[0-8] / (this sets CPSR bits)
	case 0x5:
		cmpasm(gbavirtreg_cpu[((thumbinstr>>8)&0x7)],(u32)(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] CMP (PSR: %x) <= r(%d),(#0x%x) (5.3)\n",(unsigned int)cpsrasm,(int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
	return 0;
	break;
	
	//add / add #imm bit[7] value to contents of rd and then place result on rd (this sets CPSR bits)
	case 0x6:
		//rn
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=addasm(gbavirtreg_cpu[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] ADD r(%d), (#%x) (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
	return 0;
	break;
	
	//sub / sub #imm bit[0-8] value from contents of rd and then place result on rd (this sets CPSR bits)
	case 0x7:	
		//rn
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=subasm(gbavirtreg_cpu[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] SUB r(%d), (#%x) (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
	return 0;
	break;

	//5.6
	//PC relative load WORD 10-bit Imm
	case 0x9:
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=cpuread_word(((rom+0x4)+((thumbinstr&0xff)<<2))&0xfffffffd); //[PC+0x4,#(8<<2)Imm] / because prefetch and alignment
		#ifdef DEBUGEMU
		iprintf("[THUMB](WORD) LDR r(%d)[%x], [PC:%x] (5.6) \n",(int)((thumbinstr>>8)&0x7),(unsigned int)gbavirtreg_cpu[((thumbinstr>>8)&0x7)],(unsigned int)(((rom+0x4)+((thumbinstr&0xff)<<2))&0xfffffffd));
		#endif
	return 0;
	break;
	
	////////////////////////////////////5.9 LOAD/STORE low reg with #Imm
	
	/* STR RD,[RB,#Imm] */
	case(0xc):{
		/*
		//read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//#Imm (7-bit alignment because it is WORD ammount)
		dummyreg3=(((thumbinstr>>6)&0x1f)<<2);
		*/
		
		//STR RD , [RB + #Imm]
		//cpuwrite_word(addsasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],(((thumbinstr>>6)&0x1f)<<2)), gbavirtreg_cpu[(thumbinstr&0x7)]);
		cpuwrite_word(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2), gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB](WORD) STR rd(%d), [rb(%d),#0x%x] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
			iprintf("content @%x:[%x]",(unsigned int)(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2)),(unsigned int)u32read(gbavirtreg_cpu[((thumbinstr>>3)&0x7)] + (((thumbinstr>>6)&0x1f)<<2)));
		#endif
		
	return 0;
	}
	break;
	
	/* LDR RD,[RB,#Imm] */
	//warning: small error on arm7tdmi docs (this should be LDR, but is listed as STR) as bit 11 set is load, and unset store
	case(0xd):{ //word quantity (#Imm is 7 bits, filled with bit[0] & bit[1] = 0 by shifting >> 2 )
	
		//LDR RD = [RB + #Imm] (7-bit alignment because it is WORD ammount)
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_word(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2));
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB](WORD) LDR Rd(%d):[Rb(%d),#%x] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
		#endif
	return 0;
	}
	break;
	
	//STRB Rd, [Rb,#IMM] (5.9)
	case(0xe):{
		/*
		//1)read address (from reg) into dummy reg (RD) 
		//--> BTW chunk data must not be modified (if ever it's an address it will be patched at ldr/str opcodes)
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//2a)read address (from reg) into dummy reg (RB) <-- this NEEDS to be checked for address patch as it's the destination physical address
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//#Imm (5-bit because it is not WORD ammount)
		dummyreg3=((thumbinstr>>6)&0x1f);
		*/
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD]STRB Rd(%d), [Rb(%d),(#0x%x)] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
		//2b) STR RD , [RB + #Imm] 
		cpuwrite_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f),gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("content @%x:[%x]",(unsigned int)(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f)),
			(unsigned int)u8read(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f))
			);
		#endif
	return 0;
	}
	break;
	
	//LDRB Rd, [Rb,#Imm] (5.9)
	case(0xf):{ //byte quantity (#Imm is 5 bits)
		//LDR RD = [RB + #Imm] / (5-bit because it is not WORD ammount)
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f));
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][WORD] LDRB Rd(%d), [Rb(%d),#(0x%x)] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
	return 0;
	}
	break;
	
	/////////////////////5.10
	//store halfword from rd to low reg rs
	// STRH Rd, [Rb,#IMM] (5.10)
	case(0x10):{
		/*
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//RD
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		dummyreg3=addsasm(dummyreg,); // Rb + #Imm bit[6] depth (adds >>0)
		*/
		
		//STR RD [RB,#Imm] //bit[6] , and bit[0] unset
		cpuwrite_hword(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<1),gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][HWORD] STRH Rd(%d) ,[Rb(%d),(#0x%x)] (5.10)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<1)
			);
		#endif
		
	return 0;
	}
	break;
	
	//load halfword from rs to low reg rd
	//LDRH Rd, [Rb,#IMM] (5.10)
	case(0x11):{
		/*
		//RB
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//get #Imm
		dummyreg2=(((thumbinstr>>6)&0x1f)<<1);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_hword(dummyreg3);
		*/
		
		//
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_hword(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<1));
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD] LDRH Rd(%d), [Rb(%d),#(0x%x)] (5.9)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<1)
			);
		#endif
		//Rd
		//faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	
	return 0;
	}
	break;
	
	/////////////////////5.11
	//STR RD, [SP,#IMM]
	case(0x12):{
		//retrieve SP
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		//RD
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0); 
		
		//#imm 
		//dummyreg3=((thumbinstr&0xff)<<2);
		
		cpuwrite_word(gbavirtreg_cpu[0xd]+((thumbinstr&0xff)<<2),gbavirtreg_cpu[((thumbinstr>>8)&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD] STR Rd(%d), [SP:(%x),(#0x%x)] \n",
			(int)((thumbinstr>>8)&0x7),(unsigned int)gbavirtreg_cpu[0xd],(unsigned int)((thumbinstr&0xff)<<2)
			);
		#endif
		
		//note: this opcode doesn't increase SP
	return 0;
	}
	break;
	
	//LDR RD, [SP,#IMM]
	case(0x13):{
		/*
		//retrieve SP
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		
		//#imm 
		dummyreg2=((thumbinstr&0xff)<<2);
		
		dummyreg3=cpuread_word((dummyreg+dummyreg2));
		*/
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=cpuread_word(gbavirtreg_cpu[0xd]+((thumbinstr&0xff)<<2));
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] LDR Rd(%d), [SP:(%x),#[%x]] \n",(int)((thumbinstr>>8)&0x7),(unsigned int)gbavirtreg_cpu[0xd],(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		
		//save Rd
		//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		//note: this opcode doesn't increase SP
	return 0;
	}
	break;
	
	
	/////////////////////5.12 (CPSR codes are unaffected)
	//add #Imm to the current PC value and load the result in rd
	//ADD  Rd, [PC,#IMM] (5.12)	/*VERY HEALTHY AND TESTED GBAREAD CODE*/
	case(0x14):{
		/*
		//PC
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xf), 32,0);
		
		//get #Imm
		dummyreg2=((thumbinstr&0xff)<<2);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_word(dummyreg3);
		*/
		
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=cpuread_word(rom+((thumbinstr&0xff)<<2));
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d), [PC:(%x),#[0x%x]] (5.12) \n",
			(int)((thumbinstr>>8)&0x7),(unsigned int)rom,(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		
		//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}
	break;
	
	//add #Imm to the current SP value and load the result in rd
	//ADD  Rd, [SP,#IMM] (5.12)
	case(0x15):{	
		/*
		//SP
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		
		//get #Imm
		dummyreg2=((thumbinstr&0xff)<<2);
		
		//add with #imm
		dummyreg3=addsasm(dummyreg,dummyreg2);
		
		dummyreg3=cpuread_word(dummyreg3);
		*/
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=cpuread_word(gbavirtreg_cpu[0xd]+((thumbinstr&0xff)<<2));
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d), [SP:(%x),#[%x]] (5.12) \n",(unsigned int)((thumbinstr>>8)&0x7),(unsigned int)gbavirtreg_cpu[0xd],(unsigned int)((thumbinstr&0xff)<<2)
			);
		#endif
		
		//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}	
	break;
	
	/////////////////////5.15 multiple load store
	//STMIA rb!,{Rlist}
	case(0x18):{
		
		//Rb //direct gbavirtcpureg
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		
		u32 gbavirt_cpustack=gbavirtreg_cpu[((thumbinstr>>8)&0x7)];
		#ifdef DEBUGEMU
			iprintf("STMIA Rd(%d)![%x], {R: %d %d %d %d %d %d %d %x }:regs op:%x (5.15)\n",
			(int)(thumbinstr>>8)&0x7,
			(unsigned int)gbavirt_cpustack,
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
			
		//new
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//stmia reg! is (forcefully for thumb) descendent
					cpuwrite_word(gbavirt_cpustack+(offset*4), gbavirtreg_cpu[cntr]); //word aligned
					offset++;
				}
			cntr++;
		}
			
		//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
		// & writeback always the new Rb
		
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=(gbavirt_cpustack+((lutu16bitcnt(thumbinstr&0xff))*4));		//get decimal value from registers selected
		//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
	return 0;
	}
	break;
	
	//LDMIA rd!,{Rlist}
	case(0x19):{
		//Rb
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		
		u32 gbavirt_cpustack=gbavirtreg_cpu[((thumbinstr>>8)&0x7)];
		
		#ifdef DEBUGEMU
			iprintf("LDMIA Rd(%d)![%x], {R: %d %d %d %d %d %d %d %d }:regs op:%x (5.15)\n",
			(int)((thumbinstr>>8)&0x7),
			(unsigned int)gbavirt_cpustack,
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
			
		//new
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//ldmia reg! is (forcefully for thumb) ascendent
					//cpuwrite_word(dummyreg+(offset*4), gbavirtreg_cpu[cntr]); //word aligned
					/*temp=*/gbavirtreg_cpu[cntr]=cpuread_word(gbavirt_cpustack+(offset*4));
					offset++;
				}
			cntr++;
		}
			
		//iprintf("last WORD read into cpureg: %x",(unsigned int)temp);
		
		/*
		//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
		dummyreg=(u32)addsasm((u32)dummyreg,(lutu16bitcnt(thumbinstr&0xff))*4);		//get decimal value from registers selected
		
		//update Rb
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>8)&0x7), 32,0);
		*/
		
		//update Rb
		gbavirtreg_cpu[((thumbinstr>>8)&0x7)]=gbavirt_cpustack+((lutu16bitcnt(thumbinstr&0xff))*4);
	return 0;
	}
	break;
	
	
	//				5.18 BAL (branch always) PC-Address (+/- 2048 bytes)
	//must be half-word aligned (bit 0 set to 0)
	case(0x1c):{
			
			/* //Broken
			s16 temp=((thumbinstr)&0x7ff);
			
			rom = (rom+0x4) + (negasm((2048-temp)) * 2) - 0x2 ; //# of halfwords is 2 bytes each (offset11) / -0x2 postadd PC
			*/
			
			s16 temp=(((thumbinstr)&0x7ff)<<1)&0x7ff;
			
			rom = (rom+0x4) + (negasm((2048-temp)) * 2) - 0x2 ; //# of halfwords is 2 bytes each (offset11) / -0x2 postadd PC
			
			#ifdef DEBUGEMU
				iprintf("[reconstructing rom:(%x)] \n",(unsigned int)rom+0x2);
				iprintf("[BAL][THUMB] label[#%d bytes] \n",(signed int)(temp)); 
			#endif
			
	return 0;	
	}
	break;
	
	//	5.19 long branch with link
	case(0x1e):{
		
		u32 part1=cpuread_hword(rom); //0xnnnn0000
		u32 part2=cpuread_hword(rom+0x2); //0x0000nnnn
		
		#ifdef DEBUGEMU
			iprintf("part1: %x | part2: %x \n",(unsigned int)part1,(unsigned int)part2);
		#endif
		
		s32 srom=0;
		
		u32 temppc = rom+(0x4); //prefetch
		
		part1 = ((part1&0x7ff)<<12);
		part2 = ((part2&0x7ff)<<1);
		
		srom = (rom) + 2 + (s32)((part1+part2)&0x007FFFFF);
		
		//patch bad BL addresses
		//The destination address range is (PC+4)-400000h..+3FFFFEh, ie. PC+/-4M.
		if( ((s32)(srom-rom) > (s32)0x003ffffe)
			|| 
			((s32)(srom-rom) < (s32)0xffc00000)
		){
			rom=(rom&0xffff0000) | (srom&0xffff);
		}
		else{
			rom=srom;
		}
		
		gbavirtreg_cpu[0xe] = (temppc)|1; // next instruction @ ret address
		
		
		#ifdef DEBUGEMU
			iprintf("LONG BRANCH WITH LINK [1/2]: PC:[%x],LR[%x] (5.19) \n",(unsigned int)(rom+2),(unsigned int)gbavirtreg_cpu[0xe]);
		#endif
		
		return 0;
		
	break;
	}
	
	/*
	case(0x1f):{
		
		//u16 part2=cpuread_hword(rom); //0xnnnn0000
		
		int offset2 = (thumbinstr & 0x7FF);
		gbavirtreg_cpu[0xe] = rom + (offset2 << 12);
		
		iprintf("BL[2/2]: %x \n",(unsigned int)thumbinstr);
		
		#ifdef DEBUGEMU
			iprintf("LONG BRANCH WITH LINK [2/2]: PC:[%x],LR[%x] (5.19) \n",(unsigned int)(rom+2),(unsigned int)gbavirtreg_cpu[0xe]);
		#endif
		
	break;
	}
	*/
}

switch(thumbinstr>>9){
	//5.2 (this sets CPSR bits)
	//ADD Rd, Rs, Rn
	case(0xc):{
		
		/*
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0); 

		//rn
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("add rd(%d),rs(%d)[%x],rn(%d)[%x] (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg);
		#endif
		
		dummyreg2=addasm(dummyreg2,dummyreg);
		*/
		
		//ADD Rd, Rs, Rn
		gbavirtreg_cpu[(thumbinstr&0x7)]=addasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d),Rs(%d),Rn(%d) (5.2)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
		
		//done? update desired reg content
		//faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
	return 0;
	}
	break;
	
	
	//SUB Rd, Rs, Rn (this sets CPSR bits)
	case(0xd):{
		//SUB Rd, Rs, Rn
		gbavirtreg_cpu[(thumbinstr&0x7)]=subasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] SUB Rd(%d),Rs(%d),Rn(%d) (5.2)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}
	break;
	
	//ADD Rd, Rs, #Imm (this sets CPSR bits)
	case(0xe):{
		/*
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("add r%d,r%d[%x],#0x%x (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		dummyreg2=addasm(dummyreg2,(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		//ADD Rd,[Rs,#Imm]
		gbavirtreg_cpu[(thumbinstr&0x7)]=addasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d),Rs(%x),(#0x%x) (5.2) \n",(unsigned int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}
	break;
	
	//SUB Rd, Rs, #imm (this sets CPSR bits)
	case(0xf):{
		/*
		//stored regs have already checked values / address translated, they don't need to be re-checked when retrieved
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("sub r(%d),r(%d)[%x],#0x%x (5.2)\n", (int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2,(unsigned int)((thumbinstr>>6)&0x7));
		#endif
		dummyreg2=subasm(dummyreg2,(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		//SUB Rd,[Rs,#Imm]
		gbavirtreg_cpu[(thumbinstr&0x7)]=subasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] SUB Rd(%d),Rs(%x),(#0x%x) (5.2) \n",(unsigned int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}
	break;
	
	
	/////////////////////5.7
	
	//STR RD, [Rb,Ro]
	case(0x28):{ //40dec
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//dummyreg4=addsasm(dummyreg,dummyreg2);
		
		#ifdef DEBUGEMU
			iprintf("str rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)\n",
			(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
			(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
			(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//store RD into [RB,Ro]
		cpuwrite_word((dummyreg+dummyreg2),dummyreg3);
		*/
		
		//STR Rd, [Rb,Ro]
		cpuwrite_word(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)],gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD]STR Rd(%d) [Rb(%d),Ro(%d)] (5.7)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7)
			);
		#endif
		
	return 0;
	}	
	break;
	
	//STRB RD ,[Rb,Ro] (5.7) (little endian lsb <-)
	case(0x2a):{ //42dec
	/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//dummyreg4=addsasm(dummyreg,dummyreg2);
		
		#ifdef DEBUGEMU
		iprintf("strb rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
	
		//store RD into [RB,#Imm]
		cpuwrite_byte((dummyreg+dummyreg2),(dummyreg3&0xff));
	*/
	
		//STRB Rd, [Rb,Ro]
		cpuwrite_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)],gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][BYTE] STRB Rd(%d) [Rb(%d),Ro(%d)] (5.7)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7)
			);
		#endif
	
	return 0;
	}	
	break;
	
	
	//LDR rd,[rb,ro] (correct method for reads)
	case(0x2c):{ //44dec
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_word((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		iprintf("LDR rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_word(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] LDR Rd(%d) ,[Rb(%d),Ro(%d)] (5.7)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
		
	return 0;
	}
	break;
	
	//LDRB Rd,[Rb,Ro]
	case(0x2e):{ //46dec
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_byte((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		iprintf("LDRB rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][BYTE] LDRB Rd(%d) ,[Rb(%d),Ro(%d)] (5.7)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
		
	return 0;
	}
	break;
	
	
	//////////////////////5.8
	//halfword
	//iprintf("STRH RD ,[Rb,Ro] (5.8) \n"); //thumbinstr
	case(0x29):{ //41dec strh
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		//Rd
		fastldr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		#ifdef DEBUGEMU
		iprintf("strh rd(%d)[%x] ,rb(%d)[%x],ro(%d)[%x] (5.8)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		cpuwrite_hword((dummyreg+dummyreg2),(dummyreg3&0xffff));	
		*/
		//STRH Rd,[Rb,Ro]
		cpuwrite_hword(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)],gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][HWORD] STRH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}	
	break;
	
	// LDRH RD ,[Rb,Ro] (5.8)\n
	case(0x2b):{ //43dec ldrh
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		dummyreg3=cpuread_hword((dummyreg+dummyreg2));
		
		#ifdef DEBUGEMU
		iprintf("LDRH rd(%d)[%x] ,[rb(%d)[%x],ro(%d)[%x]] (5.7)\n",
		(int)(thumbinstr&0x7),(unsigned int)dummyreg3,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2
		);
		#endif
		
		//Rd
		faststr((u8*)&dummyreg3, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		//LDRH Rd,[Rb,Ro]
		gbavirtreg_cpu[(thumbinstr&0x7)]=cpuread_hword(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][HWORD] LDRH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}
	break;
	
	//LDSB RD ,[Rb,Ro] (5.8)
	
	case(0x2d):{ //45dec ldsb
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		s8 sbyte=cpuread_byte(dummyreg+dummyreg2);
		
		#ifdef DEBUGEMU
			iprintf("ldsb rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)\n",
			(int)(thumbinstr&0x7),(signed int)sbyte,
			(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
			(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2);
		#endif
		
		//Rd
		faststr((u8*)&sbyte, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		//LDSB Rd,[Rb,Ro]
		s8 sbyte=cpuread_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		gbavirtreg_cpu[(thumbinstr&0x7)]=(u8)sbyte;
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][SBYTE] LDSB Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
	return 0;
	}
	break;
	
	//LDSH Rd ,[Rb,Ro] (5.8)
	case(0x2f):{ //47dec ldsh
		/*
		//Rb
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		//Ro
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>6)&0x7), 32,0);
		
		s16 shword=cpuread_hword(dummyreg+dummyreg2);
		
		#ifdef DEBUGEMU
		iprintf("ldsh rd(%d)[%x] ,Rb(%d)[%x],Ro(%d)[%x] (5.7)\n",
		(int)(thumbinstr&0x7),(signed int)shword,
		(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg,
		(int)((thumbinstr>>6)&0x7),(unsigned int)dummyreg2);
		#endif
		
		//Rd
		faststr((u8*)&shword, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		//LDSB Rd,[Rb,Ro]
		s16 shword=cpuread_byte(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]+gbavirtreg_cpu[((thumbinstr>>6)&0x7)]);
		gbavirtreg_cpu[(thumbinstr&0x7)]=(u16)shword;
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][SHWORD] LDSH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
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
		u32 gbavirt_cpustack = gbavirtreg_cpu[0xd];
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] PUSH {R: %d %d %d %d %d %d %d %x }:regout:%x (5.14)\n",
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
			int cntr=0x7;	//enum thumb regs
			int offset=0;	//enum found regs
			while(cntr>=0){ //8 working low regs for thumb cpu 
					if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
						iprintf("pushing ind:%d",(int)cntr); //OK
						//ldmia reg! is (forcefully for thumb) descending stack
						offset++;
						cpuwrite_word((gbavirt_cpustack+0x4)-(offset*4), gbavirtreg_cpu[cntr]); //word aligned //starts from top (n-1)
					}
				cntr--;
			}
			
		//full descending stack
		gbavirtreg_cpu[0xd]=subsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff))*4);
		
	return 0;
	}
	break;
	
	//b: 10110101 = PUSH {Rlist,LR}  low regs (0-7) & LR
	case(0xB5):{
		
		//gba r13 descending stack operation
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0); 
		u32 gbavirt_cpustack = gbavirtreg_cpu[(0xd)];
		
		int cntr=0x8;	//enum thumb regs | backwards
		int offset=0; 	//enum found regs
		while(cntr>=0){ //8 working low regs for thumb cpu 
			if(cntr!=0x8){
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					iprintf("pushing ind:%d",(int)cntr); //OK
					//push is descending stack
					offset++;
					cpuwrite_word((gbavirt_cpustack+0x4)-(offset*4), gbavirtreg_cpu[cntr]); //word aligned //starts from top (n-1)
				}
			}
			else{ //our lr operator
				iprintf("pushing ind:LR"); //OK
				offset++;
				cpuwrite_word((gbavirt_cpustack+0x4)-(offset*4), gbavirtreg_cpu[0xe]); //word aligned //starts from top (n-1)
				//#ifdef DEBUGEMU
				//	iprintf("offset(%x):LR! ",(int)cntr);
				//#endif
			}
			cntr--;
		}
		//Update stack pointer, full descendant +1 because LR
		gbavirtreg_cpu[0xd]=subsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff)+1)*4 );
		
		#ifdef DEBUGEMU
		iprintf("[THUMB] PUSH {R: %x %x %x %x %x %x %x %x },LR :regout:%x (5.14)\n",
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
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		
		u32 gbavirt_cpustack = gbavirtreg_cpu[(0xd)];
		
		int cntr=0;
		int offset=0;
		while(cntr<0x8){ //8 working low regs for thumb cpu
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
			
				/*if(rom==0x08000648){ //648 == 0[0] 1[0] 2[0x0800039d]
					int f=0;
					for(f=0;f<8;f++){
						iprintf("|(%d)[%x]| ",(int)f,(unsigned int)cpuread_word((dummyreg+0x4)+(f*4)));
					}
				}*/
				
				//pop is descending + fixup (ascending)
				gbavirtreg_cpu[cntr]=cpuread_word((gbavirt_cpustack+0x4)+(offset*4)); //word aligned 
				offset++;														//starts from zero because log2(n-1) memory format on computers
			}
			cntr++;
		}
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		gbavirtreg_cpu[0xd]=addsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff))*4);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB] POP {R: %x %x %x %x %x %x %x %x } :regout:%x (5.14)\n",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff)) //because LR
		);
		#endif
		
	return 0;
	}
	break;
	
	//b: 10111101 = POP  {Rlist,PC} low regs (0-7) & PC
	case(0xBD):{
		//gba r13 ascending stack operation
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (0xd), 32,0);
		u32 gbavirt_cpustack = gbavirtreg_cpu[(0xd)];
		
		int cntr=0;
		int offset=0;
		while(cntr<0x9){ //8 working low regs for thumb cpu
			if(cntr!=0x8){
				if(((1<<cntr) & (thumbinstr&0xff)) > 0){
					//pop is descending +`fixup (ascending)
					gbavirtreg_cpu[cntr]=cpuread_word((gbavirt_cpustack+0x4)+(offset*4)); //word aligned
					offset++;														//starts from zero because log2(n-1) memory format on computers
				}
			}
			else{//our pc operator
				
				/*
				u32 tempdebug=0;
				if (rom==0x081e73f8){
					tempdebug=1;
				}
				*/
				
				rom=gbavirtreg_cpu[(0xf)]=(cpuread_word((gbavirt_cpustack+0x4)+(offset*4)))&0xfffffffe; //word aligned ANY PC relative offset.
				offset++; //REQUIRED FOR NEXT INDEX (available)
				
				/*
				if(tempdebug==1){
					iprintf("fucked up PC: %x",(unsigned int)rom);
					while(1);
				}
				*/
				
			}
			cntr++;
		}
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		gbavirtreg_cpu[(0xd)]=addsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff)+1)*4); //+1 PC
		
		#ifdef DEBUGEMU
			iprintf("[THUMB] POP {R: %x %x %x %x %x %x %x %x },LR :regout:%x (5.14)\n",
			(unsigned int)((thumbinstr&0xff)&0x80),
			(unsigned int)((thumbinstr&0xff)&0x40),
			(unsigned int)((thumbinstr&0xff)&0x20),
			(unsigned int)((thumbinstr&0xff)&0x10),
			(unsigned int)((thumbinstr&0xff)&0x08),
			(unsigned int)((thumbinstr&0xff)&0x04),
			(unsigned int)((thumbinstr&0xff)&0x02),
			(unsigned int)((thumbinstr&0xff)&0x01),
			(unsigned int)((thumbinstr&0xff)+0x1) //because PC
		);
		#endif
	return 0;
	}
	break;
	
	
	///////////////////5.16 Conditional branch
	//b: 1101 0000 / BEQ / Branch if Z set (equal)
	//BEQ label (5.16)
	
	//these read NZCV VIRT FLAGS (this means opcodes like this must be called post updatecpuregs(0);
	
	case(0xd0):{
		if (z_flag==1){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BEQ] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BEQ not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0001 / BNE / Branch if Z clear (not equal)
	//BNE label (5.16)
	case(0xd1):{
		if (z_flag==0){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
				iprintf("[BNE] label[%x] THUMB mode (5.16) \n",(unsigned int)rom); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
				iprintf("THUMB: BNE not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0010 / BCS / Branch if C set (unsigned higher or same)
	//BCS label (5.16)
	case(0xd2):{
		if (c_flag==1){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
				iprintf("[BCS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BCS not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0011 / BCC / Branch if C unset (lower)
	case(0xd3):{
		if (c_flag==0){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BCC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BCC not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0100 / BMI / Branch if N set (negative)
	case(0xd4):{
		if (n_flag==1){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BMI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
	
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BMI not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0101 / BPL / Branch if N clear (positive or zero)
	case(0xd5):{
		if (n_flag==0){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BPL] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BPL not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 0110 / BVS / Branch if V set (overflow)
	case(0xd6):{
		if (v_flag==1){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BVS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BVS not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 0111 / BVC / Branch if V unset (no overflow)
	case(0xd7):{
		if (v_flag==0){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BVC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BVC not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1000 / BHI / Branch if C set and Z clear (unsigned higher)
	case(0xd8):{
		if ((c_flag==1)&&(z_flag==0)){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BHI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else{
			#ifdef DEBUGEMU
			iprintf("THUMB: BHI not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1001 / BLS / Branch if C clr or Z Set (lower or same [zero included])
	case(0xd9):{
		if ((c_flag==0)||(z_flag==1)){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BLS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
			
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BLS not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	//b: 1101 1010 / BGE / Branch if N set and V set, or N clear and V clear
	case(0xda):{
		if ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
				iprintf("[BGE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BGE not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1011 / BLT / Branch if N set and V clear, or N clear and V set
	case(0xdb):{
		if ( ((n_flag==1)&&(v_flag==0)) || ((n_flag==0)&&(v_flag==1)) ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BLT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BLT not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1100 / BGT / Branch if Z clear, and either N set and V set or N clear and V clear
	case(0xdc):{
		if ( (z_flag==0) && ( ((n_flag==1)&&(v_flag==1)) || ((n_flag==0)&&(v_flag==0)) ) ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BGT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BGT not met! \n");
			#endif
		}
	return 0;
	}
	break;
	
	//b: 1101 1101 / BLE / Branch if Z set, or N set and V clear, or N clear and V set (less than or equal)
	case(0xdd):{	
		if ( ((z_flag==1) || (n_flag==1)) && ((v_flag==0) || ((n_flag==0) && (v_flag==1)) )  ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
			gbavirtreg_cpu[0xf]=rom=(((rom&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			iprintf("[reconstructing rom:(%x)]",(unsigned int)rom+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
			#ifdef DEBUGEMU
			iprintf("[BLE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)rom,(unsigned int)cpsrvirt); 
			#endif
		}
		else {
			#ifdef DEBUGEMU
			iprintf("THUMB: BLE not met! \n");
			#endif
		}
	return 0;	
	}
	break;
	
	
	//5.17 SWI software interrupt changes into ARM mode and uses SVC mode/stack (SWI 14)
	//SWI #Value8 (5.17)
	case(0xDF):{
		
		//iprintf("[thumb 1/2] SWI #(%x)",(unsigned int)cpsrvirt);
		gba.armirqenable=false;
		
		u32 stack2svc=gbavirtreg_cpu[0xe];	//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//ori: updatecpuflags(1,temp_arm_psr,0x13);
		updatecpuflags(1,cpsrvirt,0x13);
		
		gbavirtreg_cpu[0xe]=stack2svc;		//ARM has r13,r14 per CPU <mode> but this is shared on gba
		
		//we force ARM mode directly regardless cpsr
		armstate=0x0; //1 thmb / 0 ARM
		
		//iprintf("[thumb] SWI #0x%x / CPSR: %x(5.17)\n",(thumbinstr&0xff),cpsrvirt);
		swi_virt(thumbinstr&0xff);
		
		//if we don't use the BIOS handling, restore CPU mode inmediately
		#ifndef BIOSHANDLER
			//Restore CPU<mode> / SPSR (spsr_last) keeps SVC && restore SPSR T bit (THUMB/ARM mode)
				//note cpsrvirt is required because we validate always if come from same PSR mode or a different. (so stack swaps make sense)
			updatecpuflags(1,cpsrvirt | (((spsr_last>>5)&1)),spsr_last&0x1F);
		#endif
		
		//-0x2 because PC THUMB rom alignment / -0x2 because prefetch
		#ifdef BIOSHANDLER
			rom  = (u32)(0x08-0x2-0x2);
		#else
			//otherwise executes a possibly BX LR (callback ret addr) -> PC increases correctly later
		#endif
		
		gba.armirqenable=true;
		
		//restore correct SPSR (deprecated because we need the SPSR to remember SVC state)
		//spsr_last=spsr_old;
		//iprintf("[thumb 2/2] SWI #(%x)",(unsigned int)cpsrvirt);
		//swi 0x13 (ARM docs)
		
	return 0;	
	}
	break;
}

switch(thumbinstr>>7){
	////////////////////////////5.13
	case(0x160):{ //dec 352 : add bit[9] depth #IMM to the SP, positive offset 	
		//gba stack method (SP)
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD SP:(%x), (+#%d (sint)) (5.13) \n",(unsigned int)gbavirtreg_cpu[(0xd)],(signed int)dbyte_tmp);
		#endif
		
		gbavirtreg_cpu[(0xd)]=(gbavirtreg_cpu[(0xd)]+dbyte_tmp);
		
		//update processor flags aren't set in this instruction
	return 0;
	}	
	break;
	
	case(0x161):{ //dec 353 : add bit[9] depth #IMM to the SP, negative offset
		//gba stack method (SP)
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD SP:(%x), (-#%d (sint)) (5.13) \n",(unsigned int)gbavirtreg_cpu[(0xd)],(signed int)dbyte_tmp);
		#endif
		
		gbavirtreg_cpu[(0xd)]=(gbavirtreg_cpu[(0xd)]-dbyte_tmp);
		
		//update processor flags aren't set in this instruction
	return 0;
	}
	break;
	
}

switch(thumbinstr>>6){
	//5.4
	//ALU OP: AND rd,rs (5.4) (this sets CPSR bits)
	case(0x100):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: AND Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=andasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}	
	break;
	
	//ALU OP: EOR rd,rs (5.4) (this sets CPSR bits)
	case(0x101):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: EOR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=eorasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: LSL rd,rs (5.4) (this sets CPSR bits)
	case(0x102):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: LSL Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=lslasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: LSR rd, rs (5.4) (this sets CPSR bits)
	case(0x103):{
	//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: LSR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=lsrasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ASR rd, rs (5.4) (this sets CPSR bits)
	case(0x104):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ASR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=asrasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ADC rd, rs (5.4) (this sets CPSR bits)
	case(0x105):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ADC Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=adcasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: SBC rd, rs (5.4) (this sets CPSR bits)
	case(0x106):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: SBC Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=sbcasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ROR rd, rs (5.4) (this sets CPSR bits)
	case(0x107):{
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ROR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=rorasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: TST Rd, Rs (5.4) (PSR bits only are affected)
	case(0x108):{
		/*
		//Rd
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		
		//Rs
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("ALU OP: TST rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=tstasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		*/
		//TST Rd,Rs
		tstasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: TST Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
	return 0;
	}	
	break;
	
	//ALU OP: NEG rd, rs (5.4) (this sets CPSR bits)
	case(0x109):{	
		//Rd
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//Rs
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: NEG Rd(%d), Rs(%d) (Rd=-Rs) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//NEG Rd,Rs (Rd=-Rs)
		gbavirtreg_cpu[(thumbinstr&0x7)]=negasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: CMP rd, rs (5.4) (PSR bits only are affected)
	case(0x10a):{	
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("ALU OP: CMP rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=cmpasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		*/
		
		//CMP Rd,Rs
		cmpasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: CMP Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
	return 0;
	}
	break;
	
	//ALU OP: CMN rd, rs (5.4) (PSR bits only are affected)
	case(0x10b):{	
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("ALU OP: CMN rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg2,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg);
		#endif
		dummyreg=cmnasm(dummyreg2,dummyreg); 	//opcode rd,rs
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		*/
		//CMN Rd,Rs
		cmnasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: CMN Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
	return 0;
	}
	break;
	
	//ALU OP: ORR rd, rs (5.4) (this sets CPSR bits)
	case(0x10c):{	
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
			iprintf("ALU OP: ORR r%d[%x], r%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=orrasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ORR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=orrasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//ALU OP: MUL rd, rs (5.4) (this sets CPSR bits)
	case(0x10d):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("ALU OP: MUL r%d[%x], r%d[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=mulasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: MUL Rd(%d), Rs(%d) (Rd=Rs*Rd)(5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=mulasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)],gbavirtreg_cpu[(thumbinstr&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: BIC Rd, Rs (5.4) (this sets CPSR bits)
	case(0x10e):{	
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("ALU OP: BIC r(%d)[%x], r(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=bicasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: BIC Rd(%d), Rs(%d) (Rd&=(~Rs))(5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=bicasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: MVN rd, rs (5.4) (this sets CPSR bits)
	case(0x10f):{
		/*
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		dummyreg=mvnasm(dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
		iprintf("ALU OP: MVN rd(%d)[%x], rs(%d)[%x] (5.4)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		//iprintf("CPSR:%x \n",cpsrvirt);
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: MVN Rd(%d), Rs(%d) (Rd=(~Rs) or Rd=WORD^Rs) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//MVN Rd,Rs (Rd&=(~Rs))
		gbavirtreg_cpu[(thumbinstr&0x7)]=mvnasm(gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	
	//high regs <-> low regs
	////////////////////////////5.5
	//ADD rd,hs (5.5)
	case(0x111):{
		/*
		//rd
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		//rs
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+8), 32,0);
		
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		#ifdef DEBUGEMU
		iprintf("HI reg ADD rd(%d)[%x], hs(%d)[%x] (5.5)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+8),(unsigned int)dummyreg2);
		#endif
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Rd(%d), Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>3)&0x7)+8));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		gbavirtreg_cpu[(thumbinstr&0x7)]+=gbavirtreg_cpu[(((thumbinstr>>3)&0x7)+8)];
	return 0;
	}
	break;

	//ADD hd,rs (5.5)	
	case(0x112):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		#ifdef DEBUGEMU
		iprintf("HI reg op ADD hd%d[%x], rs%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+8),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		*/
	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Hd(%d), Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]+=gbavirtreg_cpu[((thumbinstr>>3)&0x7)];	
	return 0;
	}
	break;
	
	//ADD hd,hs (5.5) 
	case(0x113):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		iprintf("HI reg op ADD hd%d[%x], hs%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=addasm(dummyreg,dummyreg2);
		
		//these don't update CPSR flags
		
		//done? update desired reg content
		faststr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		*/
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Hd(%d), Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]+=gbavirtreg_cpu[(((thumbinstr>>3)&0x7)+0x8)];
		
	return 0;
	}
	break;
	
	//CMP Rd,Hs (5.5) (this sets CPSR bits)
	case(0x115):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		iprintf("HI reg op CMP rd%d[%x], hs%d[%x] (5.5)\n",(int)(thumbinstr&0x7),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);	
		*/
		cmpasm(gbavirtreg_cpu[(thumbinstr&0x7)],gbavirtreg_cpu[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Rd(%d), Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
	return 0;
	}
	break;
	
	//CMP hd,rs (5.5) (this sets CPSR bits)
	case(0x116):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr>>3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("HI reg op CMP hd(%d)[%x], rs(%d)[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)((thumbinstr>>3)&0x7),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		*/
		cmpasm(gbavirtreg_cpu[((thumbinstr&0x7)+0x8)],gbavirtreg_cpu[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Hd(%d), Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>3)&0x7));
		#endif
	return 0;
	}
	break;
	
	//CMP hd,hs (5.5)  (this sets CPSR bits)
	case(0x117):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (((thumbinstr>>3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		iprintf("HI reg op CMP hd%d[%x], hd%d[%x] (5.5)\n",(int)((thumbinstr&0x7)+0x8),(unsigned int)dummyreg,(int)(((thumbinstr>>3)&0x7)+0x8),(unsigned int)dummyreg2);
		#endif
		dummyreg=cmpasm(dummyreg,dummyreg2);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		//iprintf("CPSR:%x \n",cpsrvirt);
		*/
		
		cmpasm(gbavirtreg_cpu[((thumbinstr&0x7)+0x8)],gbavirtreg_cpu[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Hd(%d), Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
	return 0;
	}
	break;
	
	//MOV
	//MOV rd,hs (5.5)	
	case(0x119):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (((thumbinstr>>0x3)&0x7)+0x8), 32,0);
		
		#ifdef DEBUGEMU
		iprintf("mov rd(%d),hs(%d)[%x] \n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)dummyreg);
		#endif
		
		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, (thumbinstr&0x7), 32,0);
		*/
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Rd(%d),Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>0x3)&0x7)+0x8));
		#endif
		
		gbavirtreg_cpu[(thumbinstr&0x7)]=gbavirtreg_cpu[(((thumbinstr>>0x3)&0x7)+0x8)];
	return 0;
	}
	break;
	
	//MOV Hd,Rs (5.5)
	case(0x11a):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7), 32,0);
		#ifdef DEBUGEMU
		iprintf("mov hd%d,rs%d[%x] \n",(int)(((thumbinstr)&0x7)+0x8),(int)(thumbinstr>>0x3)&0x7,(unsigned int)dummyreg);
		#endif

		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2,gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		*/
		
		if (((thumbinstr&0x7)+0x8) == 0xf){
			rom=gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]=(gbavirtreg_cpu[((thumbinstr>>0x3)&0x7)]-0x2)&0xfffffffe; //post-add fixup
			#ifdef DEBUGEMU
				iprintf("ROM (MOV) Restore! ");
			#endif
		}
		
		else
			gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]=gbavirtreg_cpu[((thumbinstr>>0x3)&0x7)];
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Hd(%d),Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>0x3)&0x7));
		#endif
		
	return 0;
	}
	break;
	
	//MOV hd,hs (5.5)
	case(0x11b):{
		/*
		fastldr((u8*)&dummyreg, gbavirtreg_cpu, (((thumbinstr>>0x3)&0x7)+0x8), 32,0);
		#ifdef DEBUGEMU
		iprintf("mov hd(%d),hs(%d)[%x] \n",(int)(((thumbinstr)&0x7)+0x8),(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)dummyreg);
		#endif
		
		dummyreg2=movasm(dummyreg);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
		faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((thumbinstr&0x7)+0x8), 32,0);
		*/
		
		if (((thumbinstr&0x7)+0x8) == 0xf){
			rom=gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]=(gbavirtreg_cpu[((thumbinstr>>0x3)&0x7)+0x8]-0x2)&0xfffffffe; //post-add fixup
			#ifdef DEBUGEMU
				iprintf("ROM (MOV) Restore! ");
			#endif
		}
		else
			gbavirtreg_cpu[((thumbinstr&0x7)+0x8)]=gbavirtreg_cpu[(((thumbinstr>>0x3)&0x7)+0x8)];
			
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Hd(%d),Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>0x3)&0x7)+0x8));
		#endif
		
	}
	break;
	
	//						thumb BX branch exchange (rs)
	case(0x11c):{
		//rs
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7), 32,0);
		
		/*
		if(((thumbinstr>>0x3)&0x7)!=0xf){
			u32 rdaddress=gbavirtreg_cpu[((thumbinstr>>0x3)&0x7)];
		}
		else{
			u32 rdaddress=rom;
		}
		*/
		u32 rdaddress=gbavirtreg_cpu[((thumbinstr>>0x3)&0x7)];
		
		//unlikely (r0-r7) will never be r15
		if(((thumbinstr>>0x3)&0x7)==0xf){
			//this shouldnt happen!
			#ifdef DEBUGEMU
				iprintf("thumb BX tried to be PC! (from RS) this is not supposed to HAPPEN!");
			#endif
			while(1);
		}
		
		u32 temppsr;
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((rdaddress&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&0x1f);
	
		rom=(u32)((rdaddress&0xfffffffe)-0x2); //postadd fixup & align two boundaries
	
		#ifdef DEBUGEMU
			iprintf("BX Rs(%d)! CPSR:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)temppsr);
		#endif
		
		return 0;
	}
	break;
	
	//						thumb BX (hs)
	case(0x11D):{
		//hs
		//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((thumbinstr>>0x3)&0x7)+0x8, 32,0);
		u32 rdaddress=gbavirtreg_cpu[(((thumbinstr>>0x3)&0x7)+0x8)];
		
		//BX PC sets bit[0] = 0 and adds 4
		if((((thumbinstr>>0x3)&0x7)+0x8)==0xf){
			rdaddress+=0x2; //+2 now because thumb prefetch will later add 2 more
		}
		
		u32 temppsr;
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((rdaddress&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&0x1f);
	
		rom=(u32)((rdaddress&0xfffffffe)-0x2); //postadd fixup & align two boundaries
	
		#ifdef DEBUGEMU
			iprintf("BX Hs(%d)! CPSR:%x",(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)temppsr);
		#endif
		return 0;
	}
	break;
}

//default:
//iprintf("unknown OP! %x\n",thumbinstr>>9); //debug
//break;

//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

return thumbinstr;
}

/////////////////////////////////////ARM virt/////////////////////////////////////////
//5.1
u32 __attribute__ ((hot)) disarmcode(u32 arminstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//validate conditional execution flags:
switch(dummyreg5=((arminstr>>28)&0xf)){
case(0):
	//z set EQ (equ)
	if(z_flag!=1){ //already cond_mode == negate current status (wrong)
		#ifdef DEBUGEMU
		iprintf("EQ not met! ");
		#endif
		return 0;
	}
	
break;

case(1):
//z clear NE (not equ)
	if(z_flag!=0){
		#ifdef DEBUGEMU
		iprintf("NE not met!");
		#endif
		return 0;
	}
break;

case(2):
//c set CS (unsigned higher)
	if(c_flag!=1) {
		#ifdef DEBUGEMU
		iprintf("CS not met!");
		#endif
		return 0;
	}
break;

case(3):
//c clear CC (unsigned lower)
	if(c_flag!=0){
		#ifdef DEBUGEMU
		iprintf("CC not met!");
		#endif
		return 0;
	}
break;

case(4):
//n set MI (negative)
	if(n_flag!=1){
		#ifdef DEBUGEMU
		iprintf("MI not met!");
		#endif
		return 0;
	}
break;

case(5):
//n clear PL (positive or zero)
	if(n_flag!=0) {
		#ifdef DEBUGEMU
		iprintf("PL not met!");
		#endif
		return 0;
	}
break;

case(6):
//v set VS (overflow)
	if(v_flag!=1) {
		#ifdef DEBUGEMU
		iprintf("VS not met!");
		#endif
		return 0;
	}
break;

case(7):
//v clear VC (no overflow)
	if(v_flag!=0){
		#ifdef DEBUGEMU
		iprintf("VC not met!");
		#endif
		return 0;
	}
break;

case(8):
//c set and z clear HI (unsigned higher)
	if((c_flag!=1)&&(z_flag!=0)){
		#ifdef DEBUGEMU
		iprintf("HI not met!");
		#endif
		return 0;
	}
break;

case(9):
//c clear or z set LS (unsigned lower or same)
	if((c_flag!=0)||(z_flag!=1)){
		#ifdef DEBUGEMU
		iprintf("LS not met!");
		#endif
		return 0;
	}
break;

case(0xa):
//(n set && v set) || (n clr && v clr) GE (greater or equal)
	if( ((n_flag!=1) && (v_flag!=1)) || ((n_flag!=0) && (v_flag!=0)) ) {
		#ifdef DEBUGEMU
		iprintf("GE not met!");
		#endif
		return 0;
	}
break;

case(0xb):
//(n set && v clr) || (n clr && v set) LT (less than)
	if( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ){
		#ifdef DEBUGEMU
		iprintf("LT not met!");
		#endif
		return 0;
	}
break;

case(0xc):
// (z clr) && ((n set && v set) || (n clr && v clr)) GT (greater than)
if( (z_flag!=0) && ( ((n_flag!=1) && (v_flag!=1))  || ((n_flag!=0) && (v_flag!=0)) ) ) {
	#ifdef DEBUGEMU
	iprintf("CS not met!");
	#endif
	return 0;
}
break;

case(0xd):
//(z set) || ((n set && v clear) || (n clr && v set)) LT (less than or equ)
if( (z_flag!=1) || ( ((n_flag!=1) && (v_flag!=0)) || ((n_flag!=0) && (v_flag!=1)) ) ) {
	#ifdef DEBUGEMU
	iprintf("CS not met!");
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
switch((dummyreg=(arminstr)) & 0xff000000){

	//EQ equal
	case(0x0a000000):{
		if (z_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BEQ ");
			#endif
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				/*
				fastldr((u8*)&dummyreg2, gbavirtreg_cpu, (0xf), 32,0);
				dummyreg2+=0x8;
				faststr((u8*)&dummyreg2,gbavirtreg_cpu, (0xe), 32,0);
				*/
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	}
	break;
	
	//NE not equal
	case(0x1a000000):
		if (z_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BNE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CS unsigned higher or same
	case(0x2a000000):
		if (c_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BCS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//CC unsigned lower
	case(0x3a000000):
		if (c_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BCC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//MI negative
	case(0x4a000000):
	if (n_flag==1){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BMI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//PL Positive or Zero
	case(0x5a000000):
		if (n_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BPL ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VS overflow
	case(0x6a000000):
		if (v_flag==1){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BVS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//VC no overflow
	case(0x7a000000):
		if (v_flag==0){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BVC ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//HI usigned higher
	case(0x8a000000):
		if ( (c_flag==1) && (z_flag==0) ){
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BHI ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LS unsigned lower or same
	case(0x9a000000):
		if ( (c_flag==0) || (z_flag==1) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BLS ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GE greater or equal
	case(0xaa000000):
		if ( ((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BGE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//LT less than
	case(0xba000000):
		if ( ((n_flag==1) && (v_flag==0)) || ((n_flag==0) && (v_flag==1)) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BLT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//GT greather than
	case(0xca000000):
		if ( (z_flag==0) && (((n_flag==1) && (v_flag==1)) || ((n_flag==0) && (v_flag==0))) ){ 
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BGT ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
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
			
			s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom); //+/- 32MB of addressing. 
			//after that LDR is required (requires to be loaded on a register).
			//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
			rom=(u32)s_word;
			
			#ifdef DEBUGEMU
				iprintf("(5.3) BLE ");
			#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
		}
	return 0;
	break;
	
	//AL always
	case(0xea000000):{
		
		//new
		s32 s_word=(((arminstr&0xffffff)<<2)+(int)rom+(0x8)); //+/- 32MB of addressing for branch offset / prefetch is considered here
		//after that LDR is required (requires to be loaded on a register).
		//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
		
		rom=(u32)s_word-(0x4); //fixup next IP handled by the emulator
		
		#ifdef DEBUGEMU
			iprintf("(5.3) BAL ");
		#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
	}
	return 0;
	break;
	
	//NV never
	case(0xf):
	
	#ifdef DEBUGEMU
		iprintf("(5.3) BNV ");
	#endif
			
			//link bit
			if( ((arminstr>>24)&1) == 1){
				//LR=PC
				gbavirtreg_cpu[0xe]=gbavirtreg_cpu[0xf]+0x8;
				#ifdef DEBUGEMU
					iprintf("link bit!");
				#endif
			}
	return 0;
	break;
	
}

//ARM 5.3 b (Branch exchange) BX
switch(((arminstr) & 0x012fff10)){
	case(0x012fff10):{
	
	//Rn
	//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr)&0xf), 32,0); 
	u32 temprom=gbavirtreg_cpu[((arminstr)&0xf)];
	u32 temppsr=0;
	
	if(((arminstr)&0xf)==0xf){
		iprintf("BX Rn%d[%x]! PC is undefined behaviour!",(int)((arminstr)&0xf),(unsigned int)temprom);
		while(1);
	}
	
	//(BX ARM-> THUMB value will always add +4)
	if((temprom&0x1)==1){
		//bit[0] RAM -> bit[5] PSR
		temppsr=((temprom&0x1)<<5)	| cpsrvirt;		//set bit[0] rn -> PSR bit[5]
		temprom&=~(1<<0); //align to log2 (because memory access/struct are always 4 byte)
		temprom+=(0x2);   //Prefetch (+2 now, +2 later = +4) 
	}
	else{
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5]
		//alignment for ARM case is done below
	}
	
	//due ARM's ARM mode post +0x4 opcode execute, instruction pointer is reset
	gbavirtreg_cpu[0xf]=rom=(temprom-(0x4));
	
	//set CPU <mode> (included bit[5]) and keeping all other CPSR bits
	updatecpuflags(1,temppsr,temppsr&0x1f);
	
	#ifdef DEBUGEMU
		iprintf("BX rn(%d)[%x]! set_psr:%x",(int)((arminstr)&0xf),(unsigned int)(gbavirtreg_cpu[0xf]+0x2),(unsigned int)temppsr);
	#endif
	
	return 0;
	}
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
	//iprintf("ARM opcode output %x \n",((arminstr>>20)&0x21));
}
//iprintf("s:%d,i:%d \n",setcond_arm,immop_arm);


//2 / 2 process ARM opcode by set bits earlier
if(isalu==1){
//iprintf("ARM opcode output %x \n",((arminstr>>21)&0xf));
	
	switch((arminstr>>21)&0xf){
		//AND rd,rs / AND rd,#Imm
		case(0x0):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=andasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("AND Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=andasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("AND Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
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
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=eorasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("EOR Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=eorasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("EOR Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
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
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=subasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("SUB Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=subasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("SUB Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
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
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=rsbasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("RSB Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=rsbasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("RSB Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
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
			
				//Rn (1st op reg) 		 bit[19]---bit[16] | ((arminstr>>16)&0xf)
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				//PC directive (+0x8 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					//iprintf("[imm]PC fetch!");
					gbavirtreg_cpu[(arminstr>>12)&0xf]=(rom+dummyreg2+0x8); //Rn (PC) + #Imm
				}
				else{
					gbavirtreg_cpu[(arminstr>>12)&0xf]=(gbavirtreg_cpu[((arminstr>>16)&0xf)]+dummyreg2);
				}
				
				//Rd destination reg	 bit[15]---bit[12] ((arminstr>>12)&0xf)
				
				#ifdef DEBUGEMU
					iprintf("ADD Rd(%d)[%x]<-Rn[%x]",(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[(arminstr>>12)&0xf],(int)((arminstr>>16)&0xf));
				#endif
				return 0;
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] | ((arminstr>>16)&0xf)
				
				//Rm (operand 2 )		 bit[11]---bit[0] | ((arminstr)&0xf)
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//Rs loaded | ((dummyreg2>>4)&0xf)
						
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),Rs(%d)[#0x%x byte] \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf),(int)(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded | ((dummyreg2>>4)&0xf)
						
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),Rs(%d)[#0x%x byte] \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf),(int)(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded | ((dummyreg2>>4)&0xf)
						
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),Rs(%d)[#0x%x byte] \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf),(int)(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded | ((dummyreg2>>4)&0xf)
						
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),Rs(%d)[#0x%x byte] \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf),(int)(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm | ((dummyreg2>>3)&0x1f)
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm(%x) \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm | ((dummyreg2>>3)&0x1f)
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm(%x) \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm | ((dummyreg2>>3)&0x1f)
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm(%x) \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm | ((dummyreg2>>3)&0x1f)
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm(%x) \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				
				//PC directive (+0x12 prefetch)
				if (((arminstr>>16)&0xf)==0xf){
					#ifdef DEBUGEMU
						iprintf("[reg]PC fetch!");
					#endif
					
					//Rd PC
					gbavirtreg_cpu[(arminstr>>12)&0xf]=addasm(rom,dummyreg3+(0x12)); //+0x12 for prefetch
				}
				else{
					//Rd
					gbavirtreg_cpu[(arminstr>>12)&0xf]=addasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				}
				
				//Rd destination reg	 bit[15]---bit[12] | ((arminstr>>12)&0xf)
				
				#ifdef DEBUGEMU
					iprintf("ADD Rd(%d)<-Rn(%d),Rm(%d) (5.4)\n",
						(int)((arminstr>>12)&0xf),
						(int)((arminstr>>16)&0xf),
						(int)((arminstr)&0xf)
					);
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
			
		return 0;
		}
		break;
	
		//adc rd,rs
		case(0x5):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] | ((arminstr>>16)&0xf)
				
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=adcasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12] | ((arminstr>>12)&0xf)
				
				#ifdef DEBUGEMU
					iprintf("ADC Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=adcasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("ADC Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
				);
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//sbc rd,rs
		case(0x6):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=sbcasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("SBC Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=sbcasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("SBC Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
				);
				#endif
			}
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//rsc rd,rs
		case(0x7):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=rscasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("RSC Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=rscasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("RSC Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
				);
				#endif
			}
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//tst rd,rs //set CPSR
		case(0x8):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				tstasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				#ifdef DEBUGEMU
					iprintf("TST [and] Rn(%d),#Imm[%x](ror:(%x)[%x]) (5.4) \n",
					(int)((arminstr>>16)&0xf),
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
					//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//iprintf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				tstasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				#ifdef DEBUGEMU
					iprintf("TST Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf));
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//TEQ Rd,Rs //set CPSR
		case(0x9):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				teqasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				#ifdef DEBUGEMU
					iprintf("TEQ [and] Rn(%d),#Imm[%x](ror:(%x)[%x]) (5.4) \n",
					(int)((arminstr>>16)&0xf),
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
					//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//iprintf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				teqasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				#ifdef DEBUGEMU
					iprintf("TEQ Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf));
				#endif
			}
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//cmp rd,rs //set CPSR
		case(0xa):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				cmpasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				#ifdef DEBUGEMU
					iprintf("CMP [and] Rn(%d),#Imm[%x](ror:(%x)[%x]) (5.4) \n",
					(int)((arminstr>>16)&0xf),
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
					//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//iprintf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				cmpasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				#ifdef DEBUGEMU
					iprintf("CMP Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf));
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//cmn rd,rs //set CPSR
		case(0xb):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				cmnasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				#ifdef DEBUGEMU
					iprintf("CMN [and] Rn(%d),#Imm[%x](ror:(%x)[%x]) (5.4) \n",
					(int)((arminstr>>16)&0xf),
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
					//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),Rs(%d) \n",
								(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf)
							);
						#endif
						//least signif byte (Rs) used opc Rm,Rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[(dummyreg2>>4)&0xf]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
					//show arminstr>>4
					//iprintf("dummyreg2:%x",dummyreg2);
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%d),#Imm(%x) \n",(int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
		
				//op 1 opc op 2
				cmnasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				#ifdef DEBUGEMU
					iprintf("CMN Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf));
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//orr rd,rs	//set CPSR
		case(0xc):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=orrasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("ORR Rd(%d)[%x]<-Rn(%d)[%x],#Imm[%x](ror:%x[%x])/CPSR:%x (5.4) \n",
					(int)(arminstr>>12)&0xf,(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],
					(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(unsigned int)(arminstr&0xff),
					(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2,
					(unsigned int)cpsrvirt);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=orrasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("ORR Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
				);
				#endif
			}
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//mov rd,rs
		case(0xd):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=movasm(dummyreg2);
				//rd (1st op reg) 		 bit[19]---bit[16] 
				//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("MOV Rn(%d),#Imm[%x] (ror:%x[%x])(5.4) \n",
						(int)((arminstr>>12)&0xf),
						(unsigned int)(arminstr&0xff),
						(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
					);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
				
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				
				//if PC align for prefetch
				if(((arminstr)&0xf)==0xf){
					//opcode rd,rm
					dummyreg=movasm(rom+dummyreg3+0x8);
				}
				else
					dummyreg=movasm(dummyreg3);
				
				//check if rom Rd
				if(((arminstr>>12)&0xf)==0xf){
					rom=gbavirtreg_cpu[((arminstr>>12)&0xf)]=dummyreg;
				}
				//any other reg Rd
				else{
					gbavirtreg_cpu[((arminstr>>12)&0xf)]=dummyreg;
				}
				
				//rd (1st op reg) 		 bit[19]---bit[16] 
				//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("MOV Rd(%x)[%x],Rm(%d)[%x] (5.4)\n",
					(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],
					(int)((arminstr)&0xf),(unsigned int)dummyreg3
				);
				#endif
			}
			
			//check for S bit here and update (virt<-asm) processor flags
				if(setcond_arm==1)
					updatecpuflags(0,cpsrasm,0x0);
					//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//bic rd,rs
		case(0xe):{
			if(immop_arm==1){	//for #Inmediate OP operate
			
				//rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg,gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=bicasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg2);
			
				//rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
				iprintf("BIC Rd(%d)<-Rn(%d),#Imm[%x](ror:%x[%x]) (5.4) \n",
				(int)((arminstr>>12)&0xf),
				(int)((arminstr>>16)&0xf),
				(unsigned int)(arminstr&0xff),
				(unsigned int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
				);
				#endif
			}
			else{	//for Register (operand 2) operator / shift included
		
				//Rn (1st op reg) 		 bit[19]---bit[16] 
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
				//Rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
						
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//Rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0);
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),Rs(%x)[%x] \n",
							(unsigned int)((arminstr)&0xf),
							(unsigned int)((dummyreg2>>4)&0xf),
							(unsigned int)gbavirtreg_cpu[((dummyreg2>>4)&0xf)]);
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				//op 1 opc op 2
				//dummyreg2=andasm(dummyreg,dummyreg3);
				
				//Rd
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=bicasm(gbavirtreg_cpu[((arminstr>>16)&0xf)],dummyreg3);
				
				//Rd destination reg	 bit[15]---bit[12]
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				
				#ifdef DEBUGEMU
					iprintf("BIC Rd(%d),Rn(%d),Rm(%d) (5.4)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(int)((arminstr)&0xf)
				);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	
		//MVN Rd,Rs
		case(0xf):{
			
			if(immop_arm==1){	//for #Inmediate OP operate
				//Rn (1st op reg) 		 bit[19]---bit[16] / unused because of #Imm 
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//#Imm (operand 2)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
			
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=mvnasm(dummyreg2);
				#ifdef DEBUGEMU
					iprintf("MVN Rn(%d),#Imm[%x] (ror:(%x)[%x])(5.4) \n",
						(int)((arminstr>>12)&0xf),
						(unsigned int)(arminstr&0xff),
						(int)(2*((arminstr>>8)&0xf)),(unsigned int)dummyreg2
					);
				#endif
			
				//rd (1st op reg) 		 bit[19]---bit[16] 
				//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
			}
			else{	//for Register (operand 2) operator / shift included	
				//rn (1st op reg) 		 bit[19]---bit[16] / ignored
				//fastldr((u8*)&dummyreg, (u32)gbavirtreg[0], ((arminstr>>16)&0xf), 32,0);
			
				//rm (operand 2 )		 bit[11]---bit[0]
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
				//shifting part:
				//applied to Rm available to shifted register
				//Use bit[11]---bit[8](Rs's) bottom byte ammount 
				//to do shift #Imm & opcode
			
				//iprintf("bits:%x",((arminstr>>4)&0xfff));
			
				//(currently at: shift field) rs shift opcode to Rm
				if( ((dummyreg2=((arminstr>>4)&0xfff)) &0x1) == 1){
			
					//lsl
					if((dummyreg2&0x6)==0x0){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						
						#ifdef DEBUGEMU
						iprintf("LSL rm(%d),rs(%d) \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						
						#ifdef DEBUGEMU
						iprintf("LSR rm(%d),rs(%d) \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						
						#ifdef DEBUGEMU
						iprintf("ASR rm(%d),rs(%d) \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//rs loaded into dr4
						//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg2>>4)&0xf), 32,0); 
						
						#ifdef DEBUGEMU
						iprintf("ROR rm(%d),rs(%d) \n",(int)((arminstr)&0xf),(int)((dummyreg2>>4)&0xf));
						#endif
						//least signif byte (rs) used opc rm,rs
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg2>>4)&0xf)]&0xff));
					}
					//compatibility: refresh CPU flags when barrel shifter is used
					updatecpuflags(0,cpsrasm,0x0);
				}
				
				//#Imm ammount shift & opcode to Rm
				else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
					//LSL Rm,#Imm bit[5]
					if((dummyreg2&0x6)==0x0){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSL Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//lsr
					else if ((dummyreg2&0x6)==0x2){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("LSR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//asr
					else if ((dummyreg2&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ASR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
					//ror
					else if ((dummyreg2&0x6)==0x6){
						//bit[11]---bit[7] #Imm used opc rm,#Imm
						
						dummyreg3=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(dummyreg2>>3)&0x1f); //LSL Rm,#Imm bit[5]
						#ifdef DEBUGEMU
							iprintf("ROR Rm(%x),#Imm[%x] \n",(unsigned int)((arminstr)&0xf),(unsigned int)((dummyreg2>>3)&0x1f));
						#endif
					}
				//compatibility: refresh CPU flags when barrel shifter is used
				updatecpuflags(0,cpsrasm,0x0);
				}
				
				//opcode rd,rm
				gbavirtreg_cpu[((arminstr>>12)&0xf)]=mvnasm(dummyreg3);
		
				//rd (1st op reg) 		 bit[19]---bit[16] 
				//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				#ifdef DEBUGEMU
					iprintf("MVN rd(%d),rm(%d) (5.4)\n",
						(int)((arminstr>>12)&0xf),
						(int)((arminstr)&0xf)
					);
				#endif
			}
		
			//check for S bit here and update (virt<-asm) processor flags
			if(setcond_arm==1)
				updatecpuflags(0,cpsrasm,0x0);
				//iprintf("\n CPSR:%x",cpsrvirt);
		return 0;
		}
		break;
	} //end switch conditional 5.4 flags

}//end if 5.4

//5.5 MRS / MSR (use TEQ,TST,CMN and CMP without S bit set)
//xxxxxx - xxxx / bit[21]--bit[12]
//iprintf("PSR:%x",((arminstr>>16)&0x3f));

switch((arminstr>>16)&0x3f){

	//only 11 bit (N,Z,C,V,I,F & M[4:0]) are defined for ARM processor
	//bits PSR(27:8:5) are reserved and must not be modified.
	//1) so reserved bits must be preserved when changing values in PSR
	//2) programs will never check the reserved status bits while PSR check.
	//so read, modify , write should be employed
	
	//MRS (transfer PSR to Register Rd)
	case(0xf):{ 		
	
		//source PSR is: CPSR & save cond flags
		if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
			#ifdef DEBUGEMU
				iprintf("CPSR save!:%x",(unsigned int)cpsrvirt);
			#endif
			//MRS
			gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpsrvirt;
			
			//dummyreg=cpsrvirt;
			//gbavirtreg_cpu[((arminstr>>12)&0xf)];
			//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
		}
		//source PSR is: SPSR<mode> & save cond flags
		else{
			#ifdef DEBUGEMU
				iprintf("SPSR save!:%x",(unsigned int)spsr_last);
			#endif
			
			//dummyreg=spsr_last;
			//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
			//MRS
			gbavirtreg_cpu[((arminstr>>12)&0xf)]=spsr_last;
		}
	return 0;
	}
	break;
	
	//MSR (transfer Register Rd to PSR)
	case(0x29):{ 	
	
		//CPSR
		if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
			
			//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
			
			//dummy PSR
			//dummyreg&=0xf90f03ff; //important PSR bits
			
			#ifdef DEBUGEMU
				iprintf("CPSR Restore!:(%x)",(unsigned int)(gbavirtreg_cpu[(arminstr&0xf)]&0xf90f03ff));
			#endif
			//cpsrvirt=dummyreg;
			
			//modified (cpu state id updated) / //important PSR bits
			updatecpuflags(1,(gbavirtreg_cpu[(arminstr&0xf)]&0xf90f03ff),(gbavirtreg_cpu[(arminstr&0xf)])&0x1f);
		}
		//SPSR
		else{
			//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
			//dummy PSR
			//dummyreg&=0xf90f03ff; //important PSR bits
			
			#ifdef DEBUGEMU
				iprintf("SPSR restore!:%x",(unsigned int)(gbavirtreg_cpu[(arminstr&0xf)]&0xf90f03ff));
			#endif
			spsr_last=(gbavirtreg_cpu[(arminstr&0xf)]&0xf90f03ff);
		}
	return 0;
	}
	break;
	
	//MSR (transfer Register Rd or #Imm to PSR flags only- NZCV)
	case(0x28):{
	
		//CPSR
		if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
				//dummy PSR
				//dummyreg&=0xf90f03ff; //important PSR bits
				u32 psrflags=((cpsrvirt) & ~(0xf0000000)) | (gbavirtreg_cpu[(arminstr&0xf)]&0xf0000000); //(gbavirtreg_cpu[(arminstr&0xf)]&0xf0000000);
				
				#ifdef DEBUGEMU
					iprintf("CPSR PSR FLAGS ONLY RESTR from Rd(%d)!:PSR[%x] \n",(int)(arminstr&0xf),(unsigned int)psrflags);
				#endif
				
				updatecpuflags(1,psrflags,cpsrvirt&0x1f);
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				u32 psrflags=((cpsrvirt) & ~(0xf0000000)) | (dummyreg2&0xf0000000);
				
				#ifdef DEBUGEMU
					iprintf("CPSR PSR FLAGS ONLY RESTR from #Imm!:PSR[%x] \n",(unsigned int)psrflags);
				#endif
				
				updatecpuflags(1,psrflags,cpsrvirt&0x1f);
			}
		}
		//SPSR
		else{
			//operand reg
			if( ((arminstr>>25) &0x1) == 0){
				//rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0);
				//dummy PSR
				//dummyreg&=0xf90f03ff; //important PSR bits
				u32 psrflags=((spsr_last) & ~(0xf0000000)) | (gbavirtreg_cpu[(arminstr&0xf)]&0xf0000000); //(gbavirtreg_cpu[(arminstr&0xf)]&0xf0000000);
				
				#ifdef DEBUGEMU
					iprintf("SPSR PSR FLAGS ONLY RESTR from Rd(%d)!:PSR[%x] \n",(int)(arminstr&0xf),(unsigned int)psrflags);
				#endif
				
				spsr_last=psrflags;
			}
			//#imm
			else{
				//#Imm (operand)		 bit[11]---bit[0]
				//rotate field:
				//#imm value is zero extended to 32bit, then subject
				//to rotate right by twice the value in rotate field:
				dummyreg2 = (2*((arminstr>>8)&0xf)); 
				dummyreg2=rorasm((arminstr&0xff),dummyreg2);
				u32 psrflags=((spsr_last) & ~(0xf0000000)) | (dummyreg2&0xf0000000);
				
				#ifdef DEBUGEMU
					iprintf("SPSR PSR FLAGS ONLY RESTR from #Imm!:PSR[%x] \n",(unsigned int)psrflags);
				#endif
				
				spsr_last=psrflags;
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
		//iprintf("MUL/MLA opcode! (5.6)\n");
		switch((arminstr>>20)&0x3){
			//btw: rn is ignored as whole
			//multiply only & dont alter CPSR cpu flags
			case(0x0):
				//Rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//Rs
				//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
					iprintf("MUL Rd(%d),Rm(%d),Rs(%d)",
						(int)((arminstr>>16)&0xf),
						(int)(arminstr&0xf),
						(int)((arminstr>>8)&0xf)
					);
				#endif
				
				//MUL Rd,Rm,Rs
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=(gbavirtreg_cpu[(arminstr&0xf)]*gbavirtreg_cpu[((arminstr>>8)&0xf)]);
				
				//Rd
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
			//multiply only & set CPSR cpu flags
			case(0x1):
				//rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				iprintf("MUL Rd(%d),Rm(%d),Rs(%d) (PSR s)",
					(int)((arminstr>>16)&0xf),
					(int)(arminstr&0xf),
					(int)((arminstr>>8)&0xf)
				);
				#endif
				
				//MULS Rd,Rm,Rs
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=mulasm(gbavirtreg_cpu[(arminstr&0xf)],gbavirtreg_cpu[((arminstr>>8)&0xf)]);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
				
				//rd
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
			//mult and accum & dont alter CPSR cpu flags
			case(0x2):
				//rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				iprintf("MUL Rd(%d),Rm(%d),Rs(%d) (PSR s)",
					(int)((arminstr>>16)&0xf),
					(int)(arminstr&0xf),
					(int)((arminstr>>8)&0xf)
				);
				#endif
				
				//MLA Rd, Rm,Rs,Rn
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=mlaasm(gbavirtreg_cpu[(arminstr&0xf)],gbavirtreg_cpu[((arminstr>>8)&0xf)],gbavirtreg_cpu[((arminstr>>12)&0xf)]);
				
				//update cpu flags
				//updatecpuflags(0,cpsrasm,0x0);
				
				//rd
				//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			break;
			
			//mult and accum & set CPSR cpu flags
			case(0x3):
				//rm
				//fastldr((u8*)&dummyreg, gbavirtreg_cpu, (arminstr&0xf), 32,0); 
				
				//rs
				//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>8)&0xf), 32,0); 
				
				//rn
				//fastldr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0); 
				
				#ifdef DEBUGEMU
				iprintf("mla Rd(%d),Rm(%d),Rs(%d),Rn(%d) (PSR s)",
					(int)((arminstr>>16)&0xf),
					(int)(arminstr&0xf),
					(int)((arminstr>>8)&0xf),
					(int)((arminstr>>12)&0xf)
				);
				#endif
				//MLA Rd, Rm,Rs,Rn
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=mlaasm(gbavirtreg_cpu[(arminstr&0xf)],gbavirtreg_cpu[((arminstr>>8)&0xf)],gbavirtreg_cpu[((arminstr>>12)&0xf)]);
				
				//update cpu flags
				updatecpuflags(0,cpsrasm,0x0);
				
				//rd
				//faststr((u8*)&dummyreg, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
				
			break;
			
		}
	return 0;
	}
	break;
}

//5.7 LDR / STR
//take bit[26] & 0x40 (if set) and save contents if found 
switch( ((dummyreg=((arminstr>>20)&0xff)) &0x40) ){
	//it is indeed a LDR/STR opcode
	case(0x40):{
		//rd
		//((arminstr>>12)&0xf) 
		
		//rn
		//((arminstr>>16)&0xf) 
		
		//IF NOT INMEDIATE (i=1)
		//decode = dummyreg / rd = dummyreg2 / rn = dummyreg3 / #imm|rm index /
		//post shifted OR #imm shifted = dummyreg4
		
		//dummyreg4 = calculated address/value (inside #imm/reg)
		//If shifted register? (offset is register Rm)
		if((dummyreg&0x20)==0x20){
			
			//R15/PC must not be chosen at Rm!
			if (((arminstr)&0xf)==0xf) return 0;
			
			//rm (operand 2 )		 bit[11]---bit[0]
			//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((arminstr)&0xf), 32,0);
			
			//printf("rm%d(%x) \n",((arminstr)&0xf),dummyreg4);
			
			//(currently at: shift field) rs shift opcode to Rm
			if( ((dummyreg5=((arminstr>>4)&0xff)) &0x1) == 1){
			
				//rs loaded [((dummyreg5>>4)&0xf)]
				//lsl
				if((dummyreg5&0x6)==0x0){
					#ifdef DEBUGEMU
						iprintf("LSL Rm(%d),Rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbavirtreg_cpu[((dummyreg5>>4)&0xf)]);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg5>>4)&0xf)]&0xff));
				}
				//lsr
				else if ((dummyreg5&0x6)==0x2){
					#ifdef DEBUGEMU
						iprintf("LSR Rm(%d),Rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbavirtreg_cpu[((dummyreg5>>4)&0xf)]);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg5>>4)&0xf)]&0xff));		
				}
				//asr
				else if ((dummyreg5&0x6)==0x4){
					#ifdef DEBUGEMU
						iprintf("ASR Rm(%d),Rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbavirtreg_cpu[((dummyreg5>>4)&0xf)]);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg5>>4)&0xf)]&0xff));		
				}
				//ror
				else if ((dummyreg5&0x6)==0x6){
					#ifdef DEBUGEMU
						iprintf("ROR Rm(%d),Rs(%d)[%x] \n",(int)((arminstr)&0xf),(int)((dummyreg5>>4)&0xf),(unsigned int)gbavirtreg_cpu[((dummyreg5>>4)&0xf)]);
					#endif
					//least signif byte (rs) uses: opcode rm,rs
					dummyreg=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],(gbavirtreg_cpu[((dummyreg5>>4)&0xf)]&0xff));		
				}
			//compatibility: refresh CPU flags when barrel shifter is used
			updatecpuflags(0,cpsrasm,0x0);
			}
			//#Imm ammount shift & opcode to Rm
			else{
				//show arminstr>>4
				//iprintf("dummyreg2:%x",dummyreg2);
			
				//lsl
				if((dummyreg5&0x6)==0x0){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=lslasm(gbavirtreg_cpu[((arminstr)&0xf)],((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
						iprintf("LSL Rm(%d)[%x],#Imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr)&0xf)],(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//lsr
				else if ((dummyreg5&0x6)==0x2){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=lsrasm(gbavirtreg_cpu[((arminstr)&0xf)],((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
						iprintf("LSR Rm(%d)[%x],#Imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr)&0xf)],(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//asr
				else if ((dummyreg5&0x6)==0x4){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=asrasm(gbavirtreg_cpu[((arminstr)&0xf)],((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
						iprintf("ASR Rm(%d)[%x],#Imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr)&0xf)],(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
				//ror
				else if ((dummyreg5&0x6)==0x6){
					//bit[11]---bit[7] #Imm used opc rm,#Imm
					dummyreg4=rorasm(gbavirtreg_cpu[((arminstr)&0xf)],((dummyreg5>>3)&0x1f));
					#ifdef DEBUGEMU
						iprintf("ROR Rm(%d)[%x],#Imm[%x] \n",(int)((arminstr)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr)&0xf)],(unsigned int)((dummyreg5>>3)&0x1f));
					#endif
				}
			//compatibility: refresh CPU flags when barrel shifter is used
			updatecpuflags(0,cpsrasm,0x0);
			}
		}
		
		//ELSE IF INMEDIATE (i=0)(absolute #Imm value)
		else{
			//#Imm value (operand 2)		 bit[11]---bit[0]
			dummyreg4=(arminstr&0xfff);
			#ifdef DEBUGEMU
			iprintf(" #Imm(0x%x) \n",(unsigned int)dummyreg4);			
			#endif
			
		}
		
		//pre indexing bit 	(add offset before transfer)
		if((dummyreg&0x10)==0x10){
			
			//up / increase  (Rn+=Rm)
			if((dummyreg&0x8)==0x8){
				dummyreg5=gbavirtreg_cpu[((arminstr>>16)&0xf)]+dummyreg4;
			}
			
			//down / decrease bit (Rn-=Rm)
			else{
				dummyreg5=gbavirtreg_cpu[((arminstr>>16)&0xf)]-dummyreg4;
			}
			
			//pre indexed says base is updated [!] (writeback = 1)
			dummyreg|= (1<<1);
			
			#ifdef DEBUGEMU
				iprintf("pre-indexed bit! \n");
			#endif
		}
		
		//keep original rn and copy
		//dummyreg5=(dummyreg3);
		
		//1)LDR/STR dummyreg as rd 
		
		//decode = dummyreg / rd = dummyreg2 / rn = dummyreg3 / 
		//dummyreg4 = either #Imm OR register offset
		
		//BEGIN MODIFIED 26 EN
		
		//2a) If register opcode?
		if((dummyreg&0x20)==0x20){
		
			//bit[20]
			//STR
			if((dummyreg&0x1)==0x0){
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//dereference Rn+offset
					//store RD into [Rn,#Imm]
					cpuwrite_byte(dummyreg5,(gbavirtreg_cpu[((arminstr>>12)&0xf)]&0xff));
					#ifdef DEBUGEMU
						iprintf("ARM:5.7 trying STRB Rd(%d), [b:Rn(%d)[%x],xxx] (5.9) \n",
						(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)]);
					#endif
				}
				//transfer word quantity
				else{
					//store RD into [RB,#Imm]
					cpuwrite_word(dummyreg5,gbavirtreg_cpu[((arminstr>>12)&0xf)]);
					#ifdef DEBUGEMU
						iprintf("ARM:5.7 trying to STR Rd(%d), [b:Rn(%d)[%x],xxx] (5.9)\n",(int)((arminstr>>12)&0xf),(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)]);
					#endif
				}
			}
			
			//LDR is taken care on cpuxxx_xxx opcodes
			//LDR
			else{
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//LDRB RD [Rn,#Imm]
					gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_byte(dummyreg5);
					
					#ifdef DEBUGEMU
						iprintf("\n GBA LDRB Rd(%d)[%x], [#0x%x] (5.9)\n",
						(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],(unsigned int)(dummyreg4));
					#endif
					//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				}
			
				//transfer word quantity
				else{
					//LDR RD [Rn,#Imm]
					gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_word(dummyreg5);
					
					#ifdef DEBUGEMU
						iprintf("\n GBA LDR Rd(%d)[%x], [#0x%x] (5.9)\n",
						(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],(unsigned int)dummyreg4);
					#endif
					//faststr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>12)&0xf), 32,0);
				}
			}
		}//end if register/shift opcode
		
		// END MODIFIED 26 EN
		
		//modified 27 en
		//2b) it's #Imm, (label) Rn or other inmediate value for STR/LDR
		else{
			//bit[20]
			//STR
			if((dummyreg&0x1)==0x0){
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					#ifdef DEBUGEMU
						iprintf("STRB Rd(%d)[%x],[Rn(%d)[%x]]",
						(int)((arminstr>>12)&0xf),(unsigned int)(gbavirtreg_cpu[((arminstr>>12)&0xf)]&0xff),
						(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)]);
					#endif
					//STR Rd,[rn]
					cpuwrite_byte(dummyreg5, (gbavirtreg_cpu[((arminstr>>12)&0xf)]&0xff));
				}
				//word quantity
				else{
					#ifdef DEBUGEMU
						iprintf("STR Rd(%d)[%x],[Rn(%d)[%x]]",
						(int)((arminstr>>12)&0xf),(unsigned int)(gbavirtreg_cpu[((arminstr>>12)&0xf)]&0xff),
						(int)((arminstr>>16)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)]);
					#endif
					//STR Rd,[rn]
					cpuwrite_word(dummyreg5, gbavirtreg_cpu[((arminstr>>12)&0xf)]);
				}
			}
			
			//LDR : 
			else{
				//transfer byte quantity
				if((dummyreg&0x4)==0x4){
					//iprintf("\n LDRB #imm");
					
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_byte(rom+dummyreg5+(0x8)); //align +8 for prefetching
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_byte(dummyreg5);
					}
					
					#ifdef DEBUGEMU
						iprintf("LDRB Rd(%d)[%x]<-LOADED [Rn(%d),#IMM(0x%x)] (5.7) \n",(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],(unsigned int)((arminstr>>16)&0xf),(unsigned int)dummyreg4);
					#endif
				
				}
				//transfer word quantity
				else{
					//if rn == r15 use rom / generate [PC, #imm] value into rd
					if(((arminstr>>16)&0xf)==0xf){
						gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_word(rom+dummyreg5+(0x8)); //align +8 for prefetching
					}
					//else rn / generate [Rn, #imm] value into rd
					else{
						gbavirtreg_cpu[((arminstr>>12)&0xf)]=cpuread_word(dummyreg5);
					}
					
					#ifdef DEBUGEMU
						iprintf("LDR Rd(%d)[%x]<-LOADED [Rn(%d)+#=(0x%x)] (5.7) \n",(int)((arminstr>>12)&0xf),(unsigned int)gbavirtreg_cpu[((arminstr>>12)&0xf)],(unsigned int)((arminstr>>16)&0xf),(unsigned int)dummyreg5);
					#endif
				}
			
			}
		}
		
		//modified 27 en end
		
		//3)post indexing bit (add offset after transfer)
		if((dummyreg&0x10)==0x0){
			#ifdef DEBUGEMU
			iprintf("post-indexed bit!");
			#endif
			dummyreg&= ~(1<<1); //forces the writeback post indexed base to be zero (base address isn't updated, basically)
		}
		
		//4)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		// (ALWAYS (if specified) except when R15)
		if( ((dummyreg&0x2)==0x2) && (((arminstr>>16)&0xf)!=0xf) ){
			
			//up / increase  (Rn+=Rm)
			if((dummyreg&0x8)==0x8){
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=gbavirtreg_cpu[((arminstr>>16)&0xf)]+dummyreg4;
			}
		
			//down / decrease bit (Rn-=Rm)
			else{
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=gbavirtreg_cpu[((arminstr>>16)&0xf)]-dummyreg4;
			}
			
			#ifdef DEBUGEMU
				iprintf("(new) rn writeback base addr! [%x]",(unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)]);
			#endif
			
			//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
		}
		//else: don't write-back address into base
		#ifdef DEBUGEMU
			iprintf("/******************************/");
		#endif
	}
	break;
} //end 5.7

	
//5.8 STM/LDM
switch( ( (dummyreg=((arminstr>>20)&0xff)) & 0x80)  ){
	case(0x80):{
		u32 savedcpsr=0;
		u8 writeback=0;
		
		//Rn
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0); 
		u32 rn_offset=gbavirtreg_cpu[((arminstr>>16)&0xf)];
		
		//1a)force 0x10 usr mode 
		if( ((dummyreg&0x4)==0x4) && ((cpsrvirt&0x1f)!=0x10)){
			savedcpsr=cpsrvirt;
			updatecpuflags(1,cpsrvirt,0x10);
			#ifdef DEBUGEMU
			iprintf("FORCED TO USERMODE!CPSR: %x",(unsigned int)cpsrvirt);
			#endif
			writeback=1;
		}
		//else do not load PSR or force usr(0x10) mode
		
		int cntr=0; 	//CPU register enum
		int offset=0;
		
		//4)STM/LDM
		//bit[20]
		//STM
		if((dummyreg&0x1)==0x0){
			#ifdef DEBUGEMU
			iprintf("STMIA Rd(%d)[%x], {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
			(int)((arminstr>>16)&0xf),
			(unsigned int)rn_offset,
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
			
			iprintf(":pushd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation STMIA
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((dummyreg&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((dummyreg&0x8)==0x8){
					#ifdef DEBUGEMU
						iprintf("asc stack! ");
					#endif
					
					//new
					cntr=0x0;	//enum thumb regs
					//offset=0;	//enum found regs
					while(cntr<0x10){ //15 working low regs for thumb cpu [0-15]
						if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
							#ifdef DEBUGEMU
								iprintf("pushing ind:%d",(int)cntr); //OK
							#endif
							
							//ldmia reg! is (forcefully for thumb) descendent
							
							cpuwrite_word((rn_offset)+(offset*4), gbavirtreg_cpu[cntr]); //word aligned //starts from zero
							offset++;
						}
						cntr++;
					}
				}
			
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						iprintf("desc stack! ");
					#endif
					
					//new
					cntr=0xf;	//enum thumb regs
					//offset=0;	//enum found regs
					while(cntr>=0){ //15 working low regs for thumb cpu [0-15]
						if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
							#ifdef DEBUGEMU
								iprintf("pushing ind:%d",(int)cntr); //OK
							#endif
							//ldmia reg! is (forcefully for thumb) descendent
							offset++;
							cpuwrite_word((rn_offset+0x4)-(offset*4), gbavirtreg_cpu[cntr]); //word aligned //starts from top (n-1)
						}
						cntr--;
					}
					
				}
			}
		}
		
		//LDM
		else{
			#ifdef DEBUGEMU
			iprintf("LDMIA Rd(%d)[%x], {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
			(int)((arminstr>>16)&0xf),
			(unsigned int)rn_offset,
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
			
			iprintf(":popd reg:0x%x (5.15)",
			(unsigned int)lutu32bitcnt(arminstr&0xffff)
			);
			#endif
			
			//Rn
			//stack operation LDMIA
			int cntr=0;
			int offset=0;
			
			//)pre indexing bit 	(add offset before transfer) affects the whole chain
			if((dummyreg&0x10)==0x10){
				//3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
				if((dummyreg&0x8)==0x8){
					#ifdef DEBUGEMU
						iprintf("asc stack! ");
					#endif
					
					
					while(cntr<0x10){ //8 working low regs for thumb cpu
						if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
							
							/*if(rom==0x08000648){ //648 == 0[0] 1[0] 2[0x0800039d]
							int f=0;
							for(f=0;f<8;f++){
								iprintf("|(%d)[%x]| ",(int)f,(unsigned int)cpuread_word((dummyreg+0x4)+(f*4)));
							}
							}*/
							
							//pop is descending + fixup (ascending)
							gbavirtreg_cpu[cntr]=cpuread_word((rn_offset)+(offset*4)); //word aligned starts from zero
							offset++;												
						}
						cntr++;
					}
				}
				
				//else descending stack 1 bit
				else{
					#ifdef DEBUGEMU
						iprintf("desc stack! ");
					#endif
					cntr=0xf;
					//offset=0;
					while(cntr>=0){ //8 working low regs for thumb cpu
						if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
							
							/*if(rom==0x08000648){ //648 == 0[0] 1[0] 2[0x0800039d]
							int f=0;
							for(f=0;f<8;f++){
								iprintf("|(%d)[%x]| ",(int)f,(unsigned int)cpuread_word((dummyreg+0x4)+(f*4)));
							}
							}*/
							
							//format: (descending)
							//offset++;
							//cpuwrite_word((gbavirt_cpustack+0x4)-(offset*4), gbavirtreg_cpu[cntr]); //word aligned //starts from top (n-1)
							
							//pop is descending
							offset++;
							gbavirtreg_cpu[cntr]=cpuread_word((rn_offset+0x4)-(offset*4)); //word aligned starts from top (n-1)												
						}
						cntr--;
					}
				}
			}
		}
		
		//4)post indexing bit (add offset after transfer) (by default on stmia/ldmia opcodes)
		if((dummyreg&0x10)==0x0){
			dummyreg|=0x2; //forces the writeback post indexed base
			#ifdef DEBUGEMU
				iprintf("post indexed (default)! \n");
			#endif
		}
		
		//5)
		//bit[21]
		//write-back new address into base (updated offset from base +index read) Rn
		if((dummyreg&0x2)==0x2){
			
			//if asc/ up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
			if((dummyreg&0x8)==0x8){
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=(u32)addsasm((u32)rn_offset,(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			
			//else descending stack 1 bit
			else{
				gbavirtreg_cpu[((arminstr>>16)&0xf)]=(u32)subsasm((u32)rn_offset,(lutu32bitcnt(arminstr&0xffff))*4);	//required for writeback later
			}
			//Rn update
			//faststr((u8*)&dummyreg3, gbavirtreg_cpu, ((arminstr>>16)&0xf), 32,0);
			
			#ifdef DEBUGEMU
			iprintf(" updated addr: %x / Bytes workd onto stack: %x \n", (unsigned int)gbavirtreg_cpu[((arminstr>>16)&0xf)],((lutu32bitcnt(arminstr&0xffff))*4));
			#endif
		}
		//else: don't write-back address into base
		
		//1b)forced 0x10 usr mode go back to SPSR mode
		if(writeback==1){
			#ifdef DEBUGEMU
			iprintf("RESTORED MODE:CPSR %x",(unsigned int)savedcpsr);
			#endif
			updatecpuflags(1,cpsrvirt,savedcpsr&0x1f);
			writeback=0;
		}
		
	break;
	}
}

//5.9 SWP Single Data Swap (swp rd,rm,[rn])
switch( ( (dummyreg=(arminstr)) & 0x1000090)  ){
	case(0x1000090):{
		//iprintf("SWP opcode!");
		//rn (address)
		//fastldr((u8*)&dummyreg2, gbavirtreg_cpu, ((dummyreg>>16)&0xf), 32,0);
		
		//rd is writeonly
		
		//rm
		//fastldr((u8*)&dummyreg4, gbavirtreg_cpu, ((dummyreg)&0xf), 32,0); 

		//swap byte
		if(dummyreg & (1<<22)){
			//[rn]->rd
			gbavirtreg_cpu[((dummyreg>>12)&0xf)]=cpuread_byte(gbavirtreg_cpu[((dummyreg>>16)&0xf)]);
			
			#ifdef DEBUGEMU
				iprintf("SWPB 1/2 [Rn(%d)]->Rd(%d) \n",
					(int)((dummyreg>>16)&0xf),(int)((dummyreg>>12)&0xf)
				);
			#endif
			
			//rm->[rn]
			cpuwrite_byte(gbavirtreg_cpu[((dummyreg>>16)&0xf)],(gbavirtreg_cpu[((dummyreg)&0xf)])&0xff);
			#ifdef DEBUGEMU
				iprintf("SWPB 2/2 Rm(%d)->[Rn(%d)] \n",
					(int)((dummyreg)&0xf),
					(int)((dummyreg>>16)&0xf)
				);
			#endif
		}
		else{
			//[rn]->rd
			gbavirtreg_cpu[((dummyreg>>12)&0xf)]=cpuread_word(gbavirtreg_cpu[((dummyreg>>16)&0xf)]);
			
			#ifdef DEBUGEMU
				iprintf("SWPB 1/2 [Rn(%d)]->Rd(%d) \n",
					(int)((dummyreg>>16)&0xf),(int)((dummyreg>>12)&0xf)
				);
			#endif
			
			//rm->[rn]
			cpuwrite_word(gbavirtreg_cpu[((dummyreg>>16)&0xf)],(gbavirtreg_cpu[((dummyreg)&0xf)]));
			#ifdef DEBUGEMU
				iprintf("SWPB 2/2 Rm(%d)->[Rn(%d)] \n",
					(int)((dummyreg)&0xf),
					(int)((dummyreg>>16)&0xf)
				);
			#endif
		}
		
	}
}

//5.10 software interrupt
switch( (arminstr) & 0xf000000 ){
	case(0xf000000):{
		
		#ifdef DEBUGEMU
			iprintf("[ARM] swi call #0x%x! (5.10)",(unsigned int)(arminstr&0xffffff));
		#endif
		
		armstate = 0;
		gba.armirqenable=false;
		
		/* //deprecated (because we need the SPSR to remember SVC state)
		//required because SPSR saved is not SVC neccesarily
		u32 spsr_old=spsr_last; //(see below SWI bios case)
		*/
		
		updatecpuflags(1,cpsrvirt,0x13);
		
		//iprintf("CPSR(entrymode):%x \n",cpsrvirt&0x1f);
		
		//iprintf("SWI #0x%x / CPSR: %x(5.17)\n",(thumbinstr&0xff),cpsrvirt);
		swi_virt(arminstr&0xffffff);
		
		gbavirtreg_cpu[0xe] = rom - (armstate ? 4 : 2);
		
		#ifdef BIOSHANDLER
			rom  = (u32)(0x08-0x4);
		#else
			//rom  = gbavirtreg_cpu[0xf] = (u32)(gbavirtreg_cpu[0xe]-0x4);
			//continue til BX LR (ret address cback)
		#endif
		
		//iprintf("swi%x",(unsigned int)(arminstr&0xff));
		
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
switch( (dummyreg=(arminstr)) & 0x06000010 ){
	case(0x06000010):
	//iprintf("undefined instruction!");
	exceptundef(arminstr);
	break;
}

//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

return arminstr;
}