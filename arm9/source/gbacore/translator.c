//coto: this translator is my work.
#include "translator.h"
#include "opcode.h"
#include "util.h"
#include "..\pu\pu.h"
#include "gba.arm.core.h"
#include "..\settings.h"
#include "../disk/fatmore.h"
#include "..\main.h"

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
        Z_FLAG=(cpsr>>30)&1 ? true : false;
		N_FLAG=(cpsr>>31)&1 ? true : false;
		C_FLAG=(cpsr>>29)&1 ? true : false;
		V_FLAG=(cpsr>>28)&1 ? true : false;
		cpsrvirt&=~0xF0000000;
		cpsrvirt|=((N_FLAG==true ? 1 : 0)<<31|(Z_FLAG==true ? 1 : 0)<<30|(C_FLAG==true ? 1 : 0)<<29|(V_FLAG==true ? 1 : 0)<<28);
		//cpsr = latest cpsrasm from virtual asm opcode
		//iprintf("(0)CPSR output: %x \n",cpsr);
		//iprintf("(0)cpu flags: Z[%x] N[%x] C[%x] V[%x] \n",z_flag,n_flag,c_flag,v_flag);
	
	break;
	
  	case (1):{			
			//backup cpumode begin
			switch(cpsrvirt&PSR_MODE){
				case(PSR_USR): case(PSR_SYS):
					spsr_last=spsr_usr=cpsrvirt;
					gbavirtreg_r13usr[0x0]=exRegs[0xd]; //user/sys is the same stacks
					gbavirtreg_r14usr[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup usr_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13usr[0x0]);
					#endif
				break;
				
				case(PSR_FIQ):
					spsr_last=spsr_fiq=cpsrvirt;
					gbavirtreg_r13fiq[0x0]=exRegs[0xd];
					gbavirtreg_r14fiq[0x0]=exRegs[0xe];
					
                    //If SPSR and CPSR are PSR_FIQ, all r8-r12 that is not FIQ would be destroyed, prevent that.
                    if((spsr_last&PSR_MODE) != PSR_FIQ){
                        //save 5 extra registers, that do belong to FIQ mode
                        gbavirtreg_fiq[0x0] = exRegs[0x0 + 8];
                        gbavirtreg_fiq[0x1] = exRegs[0x1 + 8];
                        gbavirtreg_fiq[0x2] = exRegs[0x2 + 8];
                        gbavirtreg_fiq[0x3] = exRegs[0x3 + 8];
                        gbavirtreg_fiq[0x4] = exRegs[0x4 + 8];
                        
                        //restore 5 extra registers, that do not belong to FIQ mode
                        exRegs[0x0 + 8]=gbavirtreg_nonfiq[0x0];
                        exRegs[0x1 + 8]=gbavirtreg_nonfiq[0x1];
                        exRegs[0x2 + 8]=gbavirtreg_nonfiq[0x2];
                        exRegs[0x3 + 8]=gbavirtreg_nonfiq[0x3];
                        exRegs[0x4 + 8]=gbavirtreg_nonfiq[0x4];
                    }
    
					#ifdef DEBUGEMU
						iprintf("stacks backup fiq_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13fiq[0x0]);
					#endif
				break;
				
				case(PSR_IRQ):
					spsr_last=spsr_irq=cpsrvirt;
					gbavirtreg_r13irq[0x0]=exRegs[0xd];
					gbavirtreg_r14irq[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup irq_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13irq[0x0]);
					#endif
				break;
				
				case(PSR_SVC):
					spsr_last=spsr_svc=cpsrvirt;  
					gbavirtreg_r13svc[0x0]=exRegs[0xd];
					gbavirtreg_r14svc[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup svc_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13svc[0x0]);
					#endif
				break;
				
				case(PSR_ABT):
					spsr_last=spsr_abt=cpsrvirt;
					gbavirtreg_r13abt[0x0]=exRegs[0xd];
					gbavirtreg_r14abt[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup abt_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13abt[0x0]);
					#endif
				break;
				
				case(PSR_UND):
					spsr_last=spsr_und=cpsrvirt;
					gbavirtreg_r13und[0x0]=exRegs[0xd];
					gbavirtreg_r14und[0x0]=exRegs[0xe];
					#ifdef DEBUGEMU
						iprintf("stacks backup und_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)gbavirtreg_r13und[0x0]);
					#endif
				break;
				
				default:
					// disable FIQ & set SVC
					//gba->reg[16].I |= 0x40;
					cpsr|=0x40;
					cpsr|=0x13;
				break;
			}
			
			//backup cpumode end
			
		//update SPSR on CPU change <mode> (this is exactly where CPU change happens)
		spsr_last=cpsr;
		
		//3)setup current CPU mode working set of registers and perform volatile stack/registers per mode swap
		switch(cpumode&PSR_MODE){
			//user/sys mode
			case(PSR_USR): case(PSR_SYS):
				exRegs[0xd]=gbavirtreg_r13usr[0x0]; //user SP/LR registers for cpu<mode> (user/sys is the same stacks)
				exRegs[0xe]=gbavirtreg_r14usr[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to usr_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			//fiq mode
			case(PSR_FIQ):
				exRegs[0xd]=gbavirtreg_r13fiq[0x0]; //fiq SP/LR registers for cpu<mode>
				exRegs[0xe]=gbavirtreg_r14fiq[0x0];
				
                //If SPSR and CPSR are PSR_FIQ, all r8-r12 that is not FIQ would be destroyed, prevent that.
                if((spsr_last&PSR_MODE) != PSR_FIQ){
                    //save 5 extra registers, that do not belong to FIQ mode
                    gbavirtreg_nonfiq[0x0]=exRegs[0x0 + 8];
                    gbavirtreg_nonfiq[0x1]=exRegs[0x1 + 8];
                    gbavirtreg_nonfiq[0x2]=exRegs[0x2 + 8];
                    gbavirtreg_nonfiq[0x3]=exRegs[0x3 + 8];
                    gbavirtreg_nonfiq[0x4]=exRegs[0x4 + 8];
                    
                    //restore 5 extra registers, that do belong to FIQ mode
                    exRegs[0x0 + 8]=gbavirtreg_fiq[0x0];
                    exRegs[0x1 + 8]=gbavirtreg_fiq[0x1];
                    exRegs[0x2 + 8]=gbavirtreg_fiq[0x2];
                    exRegs[0x3 + 8]=gbavirtreg_fiq[0x3];
                    exRegs[0x4 + 8]=gbavirtreg_fiq[0x4];
                }
                    
                #ifdef DEBUGEMU
					iprintf("| stacks swap to fiq_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			//irq mode
			case(PSR_IRQ):
				exRegs[0xd]=gbavirtreg_r13irq[0x0]; //irq SP/LR registers for cpu<mode>
				exRegs[0xe]=gbavirtreg_r14irq[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to irq_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			//svc mode
			case(PSR_SVC):
				exRegs[0xd]=gbavirtreg_r13svc[0x0]; //svc SP/LR registers for cpu<mode> (user/sys is the same stacks)
				exRegs[0xe]=gbavirtreg_r14svc[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to svc_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			//abort mode
			case(PSR_ABT):
				exRegs[0xd]=gbavirtreg_r13abt[0x0]; //abt SP/LR registers for cpu<mode>
				exRegs[0xe]=gbavirtreg_r14abt[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to abt_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			//undefined mode
			case(PSR_UND):
				exRegs[0xd]=gbavirtreg_r13und[0x0]; //und SP/LR registers for cpu<mode>
				exRegs[0xe]=gbavirtreg_r14und[0x0];
				#ifdef DEBUGEMU
					iprintf("| stacks swap to und_psr:%x:(%x)",(unsigned int)cpumode&PSR_MODE,(unsigned int)exRegs[0xd]);
				#endif
			break;
			
		}
		//end if cpumode
		
		//then update cpsr (mode) <-- cpsr & spsr case dependant
		cpsr&=~PSR_MODE;
		cpsr|=(cpumode&PSR_MODE);
		
		//->switch to arm/thumb mode depending on cpsr for virtual env
		if( ((cpsr>>5)&1) == 0x1 )
			armState=false;
		else
			armState=true;
		
		cpsr&=~(1<<0x5);
		cpsr|=((armState==false?1:0)<<0x5);
		
		//save changes to CPSR
		cpsrvirt=cpsr;
        
	}
	break;
	
	
	default:
	break;
}

return 0;
}

///////////////////////////////////////THUMB virt/////////////////////////////////////////

#ifndef DEBUGEMU
//__attribute__((section(".itcm")))
#endif
__attribute__ ((hot))
u32 disthumbcode(u32 thumbinstr){
//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

#ifdef DEBUGEMU
debuggeroutput();
#endif

//Low regs
switch(thumbinstr>>11){
	////////////////////////5.1
	//LSL opcode
	case 0x0:
		#ifdef DEBUGEMU
		iprintf("[THUMB] LSL r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
        exRegs[(thumbinstr&0x7)]=lslasm(exRegs[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//LSR opcode
	case 0x1: 
		#ifdef DEBUGEMU
		iprintf("[THUMB] LSR r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
        exRegs[(thumbinstr&0x7)]=lsrasm(exRegs[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//ASR opcode
	case 0x2:
		#ifdef DEBUGEMU
		iprintf("[THUMB] ASR r(%d),r(%d),(#0x%x) (5.1)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
        exRegs[(thumbinstr&0x7)]=asrasm(exRegs[((thumbinstr>>3)&7)],((thumbinstr>>6)&0x1f));
		
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
		
        exRegs[((thumbinstr>>8)&0x7)]=movasm((u32)(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//cmp / compare contents of rd with #imm bit[0-8] / (this sets CPSR bits)
	case 0x5:
		#ifdef DEBUGEMU
		iprintf("[THUMB] CMP (PSR: %x) <= r(%d),(#0x%x) (5.3)\n",(unsigned int)cpsrasm,(int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
        cmpasm(exRegs[((thumbinstr>>8)&0x7)],(u32)(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//add / add #imm bit[7] value to contents of rd and then place result on rd (this sets CPSR bits)
	case 0x6:
		#ifdef DEBUGEMU
		iprintf("[THUMB] ADD r(%d), (#%x) (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
        //rn
		exRegs[((thumbinstr>>8)&0x7)]=addasm(exRegs[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;
	
	//sub / sub #imm bit[0-8] value from contents of rd and then place result on rd (this sets CPSR bits)
	case 0x7:	
		#ifdef DEBUGEMU
		iprintf("[THUMB] SUB r(%d), (#%x) (5.3)\n", (int)((thumbinstr>>8)&0x7),(unsigned int)(thumbinstr&0xff));
		#endif
		
        //rn
		exRegs[((thumbinstr>>8)&0x7)]=subasm(exRegs[((thumbinstr>>8)&0x7)],(thumbinstr&0xff));
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	break;

	//5.6
	//PC relative load WORD 10-bit Imm
	case 0x9:
        #ifdef DEBUGEMU
		iprintf("[THUMB](WORD) LDR r(%d)[%x], [PC:%x] (5.6) \n",(int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[((thumbinstr>>8)&0x7)],(unsigned int)(((exRegs[0xf]+0x4)+((thumbinstr&0xff)<<2))&0xfffffffd));
		#endif
        
		exRegs[((thumbinstr>>8)&0x7)]=CPUReadMemory(((exRegs[0xf]+0x4)+((thumbinstr&0xff)<<2))&0xfffffffd); //[PC+0x4,#(8<<2)Imm] / because prefetch and alignment
	return 0;
	break;
	
	////////////////////////////////////5.9 LOAD/STORE low reg with #Imm
	
	/* STR RD,[RB,#Imm] */
	case(0xc):{
		#ifdef DEBUGEMU
			iprintf("\n [THUMB](WORD) STR rd(%d), [rb(%d),#0x%x] (5.9)\n",(int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
			iprintf("content @%x:[%x]",(unsigned int)(exRegs[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2)),(unsigned int)u32read(exRegs[((thumbinstr>>3)&0x7)] + (((thumbinstr>>6)&0x1f)<<2)));
		#endif
		
		//STR RD , [RB + #Imm]
		//CPUWriteMemory(addsasm(exRegs[((thumbinstr>>3)&0x7)],(((thumbinstr>>6)&0x1f)<<2)), exRegs[(thumbinstr&0x7)]);
		CPUWriteMemory(exRegs[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2), exRegs[(thumbinstr&0x7)]);
		
	return 0;
	}
	break;
	
	/* LDR RD,[RB,#Imm] */
	//warning: small error on arm7tdmi docs (this should be LDR, but is listed as STR) as bit 11 set is load, and unset store
	case(0xd):{ //word quantity (#Imm is 7 bits, filled with bit[0] & bit[1] = 0 by shifting >> 2 )
        #ifdef DEBUGEMU
			iprintf("\n [THUMB](WORD) LDR Rd(%d):[Rb(%d),#%x] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<2));
		#endif
        
		//LDR RD = [RB + #Imm] (7-bit alignment because it is WORD ammount)
		exRegs[(thumbinstr&0x7)]=CPUReadMemory(exRegs[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<2));
		
	return 0;
	}
	break;
	
	//STRB Rd, [Rb,#IMM] (5.9)
	case(0xe):{
		
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD]STRB Rd(%d), [Rb(%d),(#0x%x)] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
		
		//2b) STR RD , [RB + #Imm] 
		CPUWriteByte(exRegs[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f),exRegs[(thumbinstr&0x7)]);
		
	return 0;
	}
	break;
	
	//LDRB Rd, [Rb,#Imm] (5.9)
	case(0xf):{ //byte quantity (#Imm is 5 bits)
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][WORD] LDRB Rd(%d), [Rb(%d),#(0x%x)] (5.9)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x1f));
		#endif
        
        //LDR RD = [RB + #Imm] / (5-bit because it is not WORD ammount)
		exRegs[(thumbinstr&0x7)]=CPUReadByte(exRegs[((thumbinstr>>3)&0x7)]+((thumbinstr>>6)&0x1f));
		
	return 0;
	}
	break;
	
	/////////////////////5.10
	//store halfword from rd to low reg rs
	// STRH Rd, [Rb,#IMM] (5.10)
	case(0x10):{		
		//STR RD [RB,#Imm] //bit[6] , and bit[0] unset
		CPUWriteHalfWord(exRegs[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<1),exRegs[(thumbinstr&0x7)]);
		
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
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD] LDRH Rd(%d), [Rb(%d),#(0x%x)] (5.9)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(unsigned int)(((thumbinstr>>6)&0x1f)<<1)
			);
		#endif
		
        exRegs[(thumbinstr&0x7)]=CPUReadHalfWord(exRegs[((thumbinstr>>3)&0x7)]+(((thumbinstr>>6)&0x1f)<<1));
		
	return 0;
	}
	break;
	
	/////////////////////5.11
	//STR RD, [SP,#IMM] / note: this opcode doesn't increase SP
	case(0x12):{
		#ifdef DEBUGEMU
			iprintf("\n [THUMB][HWORD] STR Rd(%d), [SP:(%x),(#0x%x)] \n",
			(int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[0xd],(unsigned int)((thumbinstr&0xff)<<2)
			);
		#endif
		
		CPUWriteMemory(exRegs[0xd]+((thumbinstr&0xff)<<2),exRegs[((thumbinstr>>8)&0x7)]);
	return 0;
	}
	break;
	
	//LDR RD, [SP,#IMM]
	case(0x13):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] LDR Rd(%d), [SP:(%x),#[%x]] \n",(int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[0xd],(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		
        exRegs[((thumbinstr>>8)&0x7)]=CPUReadMemory(exRegs[0xd]+((thumbinstr&0xff)<<2));
		
	return 0;
	}
	break;
	
	
	/////////////////////5.12 (CPSR codes are unaffected)
	//add #Imm to the current PC value and load the result in rd
	//ADD  Rd, [PC,#IMM] (5.12)	/*VERY HEALTHY AND TESTED GBAREAD CODE*/
	case(0x14):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d), [PC:(%x),#[0x%x]] (5.12) \n",
			(int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[0xf],(unsigned int)((thumbinstr&0xff)<<2));
		#endif
		
		exRegs[((thumbinstr>>8)&0x7)]=CPUReadMemory(exRegs[0xf]+((thumbinstr&0xff)<<2));
		
	return 0;
	}
	break;
	
	//add #Imm to the current SP value and load the result in rd / does not modify sp
	//ADD  Rd, [SP,#IMM] (5.12)
	case(0x15):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d), [SP:(%x),#[%x]] (5.12) \n",(unsigned int)((thumbinstr>>8)&0x7),(unsigned int)exRegs[0xd],(unsigned int)((thumbinstr&0xff)<<2)
			);
		#endif
        
        exRegs[((thumbinstr>>8)&0x7)]=CPUReadMemory(exRegs[0xd]+((thumbinstr&0xff)<<2));
		
	return 0;
	}	
	break;
	
	/////////////////////5.15 multiple load store
	//STMIA rb!,{Rlist}
	case(0x18):{
		
		u32 gbavirt_cpustack=exRegs[((thumbinstr>>8)&0x7)];
			
		//new
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//stmia reg! is (forcefully for thumb) descendent
					CPUWriteMemory(gbavirt_cpustack+(offset*4), exRegs[cntr]); //word aligned
					offset++;
				}
			cntr++;
		}
			
		//update rd <-(address+reg ammount*4) starting from zero (so last 4 bytes are next pointer available)
		// & writeback always the new Rb
		
		exRegs[((thumbinstr>>8)&0x7)]=(gbavirt_cpustack+((lutu16bitcnt(thumbinstr&0xff))*4));		//get decimal value from registers selected
		
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
		
        
	return 0;
	}
	break;
	
	//LDMIA rd!,{Rlist}
	case(0x19):{
		u32 gbavirt_cpustack=exRegs[((thumbinstr>>8)&0x7)];
			
		//new
		int cntr=0;	//enum thumb regs
		int offset=0; //enum found regs
		while(cntr<0x8){ //8 working low regs for thumb cpu 
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//ldmia reg! is (forcefully for thumb) ascendent
					//CPUWriteMemory(dummyreg+(offset*4), exRegs[cntr]); //word aligned
					/*temp=*/exRegs[cntr]=CPUReadMemory(gbavirt_cpustack+(offset*4));
					offset++;
				}
			cntr++;
		}
		
		//update Rb
		exRegs[((thumbinstr>>8)&0x7)]=gbavirt_cpustack+((lutu16bitcnt(thumbinstr&0xff))*4);
        
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
		
        
    return 0;
	}
	break;
	
	
	//				5.18 BAL (branch always) PC-Address (+/- 2048 bytes)
	//must be half-word aligned (bit 0 set to 0)
	case(0x1c):{
            
			/* //Broken
			s16 temp=((thumbinstr)&0x7ff);
			
			exRegs[0xf] = (exRegs[0xf]+0x4) + (mvnasm((2048-temp)) * 2) - 0x2 ; //# of halfwords is 2 bytes each (offset11) / -0x2 postadd PC
			*/
			
			s16 temp=(((thumbinstr)&0x7ff)<<1)&0x7ff;
			
			exRegs[0xf] = (exRegs[0xf]+0x4) + (mvnasm((2048-temp)) * 2) - 0x2 ; //# of halfwords is 2 bytes each (offset11) / -0x2 postadd PC
			
            #ifdef DEBUGEMU
				iprintf("[PC Reconstucted :(%x)] \n",(unsigned int)exRegs[0xf]+0x2);
				iprintf("[BAL][THUMB] label[#%d bytes] \n",(signed int)(temp)); 
			#endif
			
	return 0;	
	}
	break;
	
	//	5.19 long branch with link
	case(0x1e):{
		
		u32 part1=CPUReadHalfWord(exRegs[0xf]); //0xnnnn0000
		u32 part2=CPUReadHalfWord(exRegs[0xf]+0x2); //0x0000nnnn
		
        #ifdef DEBUGEMU
			iprintf("[THMB BL] part1: %x | part2: %x \n",(unsigned int)part1,(unsigned int)part2);
		#endif
		
		s32 srom=0;
		
		u32 temppc = exRegs[0xf]+(0x4); //prefetch
		
		part1 = ((part1&0x7ff)<<12);
		part2 = ((part2&0x7ff)<<1);
		
		srom = (exRegs[0xf]) + 2 + (s32)((part1+part2)&0x007FFFFF);
		
		//patch bad BL addresses
		//The destination address range is (PC+4)-400000h..+3FFFFEh, ie. PC+/-4M.
		if( ((s32)((int)srom-(int)exRegs[0xf]) > (s32)0x003ffffe)
			|| 
			((s32)((int)srom-(int)exRegs[0xf]) < (s32)0xffc00000)
		){
			exRegs[0xf]=(exRegs[0xf]&0xffff0000) | ((u32)srom&0xffff);
		}
		else{
			exRegs[0xf]=srom;
		}
		
		exRegs[0xe] = (temppc)|1; // next instruction @ ret address
		
		#ifdef DEBUGEMU
			iprintf("LONG BRANCH WITH LINK [1/2]: PC:[%x],LR[%x] (5.19) \n",(unsigned int)(exRegs[0xf]+2),(unsigned int)exRegs[0xe]);
		#endif
		
		return 0;
		
	break;
	}
}

switch(thumbinstr>>9){
	//5.2 (this sets CPSR bits)
	//ADD Rd, Rs, Rn
	case(0xc):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d),Rs(%d),Rn(%d) (5.2)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
		
		//ADD Rd, Rs, Rn
		exRegs[(thumbinstr&0x7)]=addasm(exRegs[((thumbinstr>>3)&0x7)],exRegs[((thumbinstr>>6)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	
	//SUB Rd, Rs, Rn (this sets CPSR bits)
	case(0xd):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] SUB Rd(%d),Rs(%d),Rn(%d) (5.2)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
        //SUB Rd, Rs, Rn
		exRegs[(thumbinstr&0x7)]=subasm(exRegs[((thumbinstr>>3)&0x7)],exRegs[((thumbinstr>>6)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//ADD Rd, Rs, #Imm (this sets CPSR bits)
	case(0xe):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD Rd(%d),Rs(%x),(#0x%x) (5.2) \n",(unsigned int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x7));
		#endif
        
        //ADD Rd,[Rs,#Imm]
		exRegs[(thumbinstr&0x7)]=addasm(exRegs[((thumbinstr>>3)&0x7)],(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//SUB Rd, Rs, #imm (this sets CPSR bits)
	case(0xf):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] SUB Rd(%d),Rs(%x),(#0x%x) (5.2) \n",(unsigned int)(thumbinstr&0x7),(unsigned int)((thumbinstr>>3)&0x7),(unsigned int)((thumbinstr>>6)&0x7));
		#endif
        
        //SUB Rd,[Rs,#Imm]
		exRegs[(thumbinstr&0x7)]=subasm(exRegs[((thumbinstr>>3)&0x7)],(thumbinstr>>6)&0x7);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	
	/////////////////////5.7
	
	//STR RD, [Rb,Ro]
	case(0x28):{ //40dec
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD]STR Rd(%d) [Rb(%d),Ro(%d)] (5.7)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7)
			);
		#endif
        
        //STR Rd, [Rb,Ro]
		CPUWriteMemory(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)],exRegs[(thumbinstr&0x7)]);
		
	return 0;
	}	
	break;
	
	//STRB RD ,[Rb,Ro] (5.7) (little endian lsb <-)
	case(0x2a):{ //42dec
        
    	#ifdef DEBUGEMU
			iprintf("[THUMB][BYTE] STRB Rd(%d) [Rb(%d),Ro(%d)] (5.7)\n",
			(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7)
			);
		#endif
        
        //STRB Rd, [Rb,Ro]
        CPUWriteByte(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)],exRegs[(thumbinstr&0x7)]);
        
	return 0;
	}	
	break;
	
	
	//LDR rd,[rb,ro] (correct method for reads)
	case(0x2c):{ //44dec
        #ifdef DEBUGEMU
			iprintf("[THUMB][WORD] LDR Rd(%d) ,[Rb(%d),Ro(%d)] (5.7)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
		exRegs[(thumbinstr&0x7)]=CPUReadMemory(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
	return 0;
	}
	break;
	
	//LDRB Rd,[Rb,Ro]
	case(0x2e):{ //46dec
        #ifdef DEBUGEMU
			iprintf("[THUMB][BYTE] LDRB Rd(%d) ,[Rb(%d),Ro(%d)] (5.7)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
		exRegs[(thumbinstr&0x7)]=CPUReadByte(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
	return 0;
	}
	break;
	
	
	//////////////////////5.8
	//halfword
	//iprintf("STRH RD ,[Rb,Ro] (5.8) \n"); //thumbinstr
	case(0x29):{ //41dec strh
		#ifdef DEBUGEMU
			iprintf("[THUMB][HWORD] STRH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
        //STRH Rd,[Rb,Ro]
		CPUWriteHalfWord(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)],exRegs[(thumbinstr&0x7)]);
		
	return 0;
	}	
	break;
	
	// LDRH RD ,[Rb,Ro] (5.8)\n
	case(0x2b):{ //43dec ldrh
		#ifdef DEBUGEMU
			iprintf("[THUMB][HWORD] LDRH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
        //LDRH Rd,[Rb,Ro]
		exRegs[(thumbinstr&0x7)]=CPUReadHalfWord(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
		
	return 0;
	}
	break;
	
	//LDSB RD ,[Rb,Ro] (5.8)
	
	case(0x2d):{ //45dec ldsb
		#ifdef DEBUGEMU
			iprintf("[THUMB][SBYTE] LDSB Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
		//LDSB Rd,[Rb,Ro]
		s8 sbyte=CPUReadByte(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
		exRegs[(thumbinstr&0x7)]=(u8)sbyte;
		
	return 0;
	}
	break;
	
	//LDSH Rd ,[Rb,Ro] (5.8)
	case(0x2f):{ //47dec ldsh
		#ifdef DEBUGEMU
			iprintf("[THUMB][SHWORD] LDSH Rd(%d) ,[Rb(%d),Ro(%d)] (5.8)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7),(int)((thumbinstr>>6)&0x7));
		#endif
        
		//LDSB Rd,[Rb,Ro]
		s16 shword=CPUReadByte(exRegs[((thumbinstr>>3)&0x7)]+exRegs[((thumbinstr>>6)&0x7)]);
		exRegs[(thumbinstr&0x7)]=(u16)shword;
		
	return 0;
	}
	break;
}

switch(thumbinstr>>8){
	///////////////////////////5.14
	//b: 10110100 = PUSH {Rlist} low regs (0-7)
	case(0xB4):{
		
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
        
		//gba stack method (stack pointer) / requires descending pointer
		u32 gbavirt_cpustack = exRegs[0xd];
		
		//new
			int cntr=0x7;	//enum thumb regs
			int offset=0;	//enum found regs
			while(cntr>=0){ //8 working low regs for thumb cpu 
					if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
						//iprintf("pushing ind:%d",(int)cntr); //OK
						//ldmia reg! is (forcefully for thumb) descending stack
						offset++;
						CPUWriteMemory((gbavirt_cpustack+0x4)-(offset*4), exRegs[cntr]); //word aligned //starts from top (n-1)
					}
				cntr--;
			}
			
		//full descending stack
		exRegs[0xd]=gbavirt_cpustack-(lutu16bitcnt(thumbinstr&0xff))*4;
		
	return 0;
	}
	break;
	
	//b: 10110101 = PUSH {Rlist,LR}  low regs (0-7) & LR
	case(0xB5):{
		
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
        
		//gba r13 descending stack operation
		u32 gbavirt_cpustack = exRegs[(0xd)];
		
		int cntr=0x8;	//enum thumb regs | backwards
		int offset=0; 	//enum found regs
		while(cntr>=0){ //8 working low regs for thumb cpu 
			if(cntr!=0x8){
				if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
					//iprintf("pushing ind:%d",(int)cntr); //OK
					//push is descending stack
					offset++;
					CPUWriteMemory((gbavirt_cpustack+0x4)-(offset*4), exRegs[cntr]); //word aligned //starts from top (n-1)
				}
			}
			else{ //our lr operator
				//iprintf("pushing ind:LR"); //OK
				offset++;
				CPUWriteMemory((gbavirt_cpustack+0x4)-(offset*4), exRegs[0xe]); //word aligned //starts from top (n-1)
				//#ifdef DEBUGEMU
				//	iprintf("offset(%x):LR! ",(int)cntr);
				//#endif
			}
			cntr--;
		}
		//Update stack pointer, full descendant +1 because LR
		exRegs[0xd]=gbavirt_cpustack-((lutu16bitcnt(thumbinstr&0xff)+1)*4);
		
	return 0;
	}
	break;
	
	//b: 10111100 = POP  {Rlist} low regs (0-7)
	case(0xBC):{
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
		
        //gba r13 ascending stack operation
		u32 gbavirt_cpustack = exRegs[(0xd)];
		
		int cntr=0;
		int offset=0;
		while(cntr<0x8){ //8 working low regs for thumb cpu
			if( ((1<<cntr) & (thumbinstr&0xff)) > 0 ){
			
				/*if(exRegs[0xf]==0x08000648){ //648 == 0[0] 1[0] 2[0x0800039d]
					int f=0;
					for(f=0;f<8;f++){
						iprintf("|(%d)[%x]| ",(int)f,(unsigned int)CPUReadMemory((dummyreg+0x4)+(f*4)));
					}
				}*/
				
				//pop is descending + fixup (ascending)
				exRegs[cntr]=CPUReadMemory((gbavirt_cpustack+0x4)+(offset*4)); //word aligned 
				offset++;														//starts from zero because log2(n-1) memory format on computers
			}
			cntr++;
		}
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		exRegs[0xd]=addsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff))*4);
		
	return 0;
	}
	break;
	
	//b: 10111101 = POP  {Rlist,PC} low regs (0-7) & PC
	case(0xBD):{
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
        
        //gba r13 ascending stack operation
		u32 gbavirt_cpustack = exRegs[(0xd)];
		
		int cntr=0;
		int offset=0;
		while(cntr<0x9){ //8 working low regs for thumb cpu
			if(cntr!=0x8){
				if(((1<<cntr) & (thumbinstr&0xff)) > 0){
					//pop is descending +`fixup (ascending)
					exRegs[cntr]=CPUReadMemory((gbavirt_cpustack+0x4)+(offset*4)); //word aligned
					offset++;														//starts from zero because log2(n-1) memory format on computers
				}
			}
			else{//our pc operator
				
				/*
				u32 tempdebug=0;
				if (exRegs[0xf]==0x081e73f8){
					tempdebug=1;
				}
				*/
				
				exRegs[0xf]=(CPUReadMemory((gbavirt_cpustack+0x4)+(offset*4)))&0xfffffffe; //word aligned ANY PC relative offset.
				offset++; //REQUIRED FOR NEXT INDEX (available)
				
				/*
				if(tempdebug==1){
					iprintf("fucked up PC: %x",(unsigned int)exRegs[0xf]);
					while(1);
				}
				*/
				
			}
			cntr++;
		}
		
		//restore is ascending (so Fixup stack offset address, to restore n registers)
		exRegs[(0xd)]=addsasm(gbavirt_cpustack,(lutu16bitcnt(thumbinstr&0xff)+1)*4); //+1 PC
		
	return 0;
	}
	break;
	
	
	///////////////////5.16 Conditional branch
	//b: 1101 0000 / BEQ / Branch if Z set (equal)
	//BEQ label (5.16)
	
	//these read NZCV VIRT FLAGS (this means opcodes like this must be called post updatecpuregs(0);
	
	case(0xd0):{
		if (Z_FLAG){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=((((u32)exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BEQ] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (!Z_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
				iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
                iprintf("[BNE] label[%x] THUMB mode (5.16) \n",(unsigned int)exRegs[0xf]); 
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
		if (C_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
				iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
                iprintf("[BCS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (!C_FLAG){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BCC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (N_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BMI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (!N_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BPL] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (V_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BVS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if (!V_FLAG){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BVC] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ((C_FLAG)&&(!Z_FLAG)){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BHI] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ((!C_FLAG)||(Z_FLAG)){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
			iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
			
            iprintf("[BLS] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ( ((N_FLAG)&&(V_FLAG)) || ((!N_FLAG)&&(!V_FLAG)) ){
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			
			#ifdef DEBUGEMU
                iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
                iprintf("[BGE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ( ((N_FLAG)&&(!V_FLAG)) || ((!N_FLAG)&&(V_FLAG)) ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
                iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
                iprintf("[BLT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ( (!Z_FLAG) && ( ((N_FLAG)&&(V_FLAG)) || ((!N_FLAG)&&(!V_FLAG)) ) ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			
			#ifdef DEBUGEMU
                iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
                iprintf("[BGT] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		if ( ((Z_FLAG) || (N_FLAG)) && ((!V_FLAG) || ((!N_FLAG) && (V_FLAG)) )  ){ 
			//s8 temp=(((thumbinstr&0xff00)>>8)<<1); //in case this is halfword swap
			s8 temp=((thumbinstr&0xff)<<1);
			
            exRegs[0xf]=(((exRegs[0xf]&0xfffffffe)+0x4) + (s8)(temp) -0x2 ); // postadd alignment.
			
			#ifdef DEBUGEMU
                iprintf("[reconstructing rom:(%x)]",(unsigned int)exRegs[0xf]+0x2); //#imm fixup (+2 because we took -0x2 from spec for our postadd PC)
                iprintf("[BLE] label[%x] THUMB mode / CPSR:%x (5.16) \n",(unsigned int)exRegs[0xf],(unsigned int)cpsrvirt); 
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
		
        #ifdef DEBUGEMU
            iprintf("5.17 [thumb] SWI #(%x)",(unsigned int)thumbinstr&0xff);
		#endif
        
        armIrqEnable=false;
		
		updatecpuflags(1,cpsrvirt,PSR_SVC);
		
		//iprintf("[thumb] SWI #0x%x / CPSR: %x(5.17)\n",(thumbinstr&0xff),cpsrvirt);
		swi_virt(thumbinstr&0xff);
		
		//if we don't use the BIOS handling, SPSR<mode> is restored
		#ifndef BIOSHANDLER
            updatecpuflags(1,cpsrvirt,spsr_last&PSR_MODE);
            
        //jump to bios (swi) vector
        #else
			exRegs[0xf]  = (u32)(0x08-0x2-0x2);
		
        #endif
		
		armIrqEnable=true;
		
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
			iprintf("[THUMB][WORD] ADD SP:(%x), (+#%d (sint)) (5.13) \n",(unsigned int)exRegs[(0xd)],(signed int)dbyte_tmp);
		#endif
		
		exRegs[(0xd)]=(exRegs[(0xd)]+dbyte_tmp);
		
		//update processor flags aren't set in this instruction
	return 0;
	}	
	break;
	
	case(0x161):{ //dec 353 : add bit[9] depth #IMM to the SP, negative offset
		//gba stack method (SP)
		//cvert to 8 bit + bit[9] for sign extend
		s32 dbyte_tmp=((thumbinstr&0x7f)<<2);
		
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ADD SP:(%x), (-#%d (sint)) (5.13) \n",(unsigned int)exRegs[(0xd)],(signed int)dbyte_tmp);
		#endif
		
		exRegs[(0xd)]=(exRegs[(0xd)]-dbyte_tmp);
		
		//update processor flags aren't set in this instruction
	return 0;
	}
	break;
	
}

switch(thumbinstr>>6){
	//5.4
	//ALU OP: AND rd,rs (5.4) (this sets CPSR bits)
	case(0x100):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: AND Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=andasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}	
	break;
	
	//ALU OP: EOR rd,rs (5.4) (this sets CPSR bits)
	case(0x101):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: EOR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=eorasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: LSL rd,rs (5.4) (this sets CPSR bits)
	case(0x102):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: LSL Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=lslasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: LSR rd, rs (5.4) (this sets CPSR bits)
	case(0x103):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: LSR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=lsrasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ASR rd, rs (5.4) (this sets CPSR bits)
	case(0x104):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ASR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=asrasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ADC rd, rs (5.4) (this sets CPSR bits)
	case(0x105):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ADC Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=adcasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: SBC rd, rs (5.4) (this sets CPSR bits)
	case(0x106):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: SBC Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=sbcasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: ROR rd, rs (5.4) (this sets CPSR bits)
	case(0x107):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ROR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=rorasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: TST Rd, Rs (5.4) (PSR bits only are affected)
	case(0x108):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: TST Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
        //TST Rd,Rs
		tstasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}	
	break;
	
	//ALU OP: NEG rd, rs (5.4) (this sets CPSR bits)
	case(0x109):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: NEG Rd(%d), Rs(%d) (Rd=-Rs) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//NEG Rd,Rs (Rd=-Rs)
		exRegs[(thumbinstr&0x7)]=mvnasm(exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: CMP rd, rs (5.4) (PSR bits only are affected)
	case(0x10a):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: CMP Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
        
        //CMP Rd,Rs
		cmpasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//ALU OP: CMN rd, rs (5.4) (PSR bits only are affected)
	case(0x10b):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: CMN Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
        
		//CMN Rd,Rs
		cmnasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//ALU OP: ORR rd, rs (5.4) (this sets CPSR bits)
	case(0x10c):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: ORR Rd(%d), Rs(%d) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=orrasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//ALU OP: MUL rd, rs (5.4) (this sets CPSR bits)
	case(0x10d):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: MUL Rd(%d), Rs(%d) (Rd=Rs*Rd)(5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=mulasm(exRegs[((thumbinstr>>3)&0x7)],exRegs[(thumbinstr&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: BIC Rd, Rs (5.4) (this sets CPSR bits)
	case(0x10e):{	
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: BIC Rd(%d), Rs(%d) (Rd&=(~Rs))(5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		exRegs[(thumbinstr&0x7)]=bicasm(exRegs[(thumbinstr&0x7)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	//ALU OP: MVN rd, rs (5.4) (this sets CPSR bits)
	case(0x10f):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] ALU OP: MVN Rd(%d), Rs(%d) (Rd=(~Rs) or Rd=WORD^Rs) (5.4)\n",(int)(thumbinstr&0x7),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//MVN Rd,Rs (Rd&=(~Rs))
		exRegs[(thumbinstr&0x7)]=mvnasm(exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
	return 0;
	}
	break;
	
	
	//high regs <-> low regs
	////////////////////////////5.5
	//ADD rd,hs (5.5) / these don't update CPSR flags
	case(0x111):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Rd(%d), Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>3)&0x7)+8));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		exRegs[(thumbinstr&0x7)]+=exRegs[(((thumbinstr>>3)&0x7)+8)];
	return 0;
	}
	break;

	//ADD hd,rs (5.5)	/   //these don't update CPSR flags
	case(0x112):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Hd(%d), Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>3)&0x7));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		exRegs[((thumbinstr&0x7)+0x8)]+=exRegs[((thumbinstr>>3)&0x7)];	
	return 0;
	}
	break;
	
	//ADD hd,hs (5.5)   /   these don't update CPSR flags 
	case(0x113):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg ADD Hd(%d), Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
		
		//ADD Rd,Hs (Rd = Rd + Hs)
		exRegs[((thumbinstr&0x7)+0x8)]+=exRegs[(((thumbinstr>>3)&0x7)+0x8)];
		
	return 0;
	}
	break;
	
	//CMP Rd,Hs (5.5) (this sets CPSR bits)
	case(0x115):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Rd(%d), Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
        
        cmpasm(exRegs[(thumbinstr&0x7)],exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//CMP hd,rs (5.5) (this sets CPSR bits)
	case(0x116):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Hd(%d), Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>3)&0x7));
		#endif
        
        cmpasm(exRegs[((thumbinstr&0x7)+0x8)],exRegs[((thumbinstr>>3)&0x7)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//CMP hd,hs (5.5)  (this sets CPSR bits)
	case(0x117):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] HI reg CMP Hd(%d), Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>3)&0x7)+0x8));
		#endif
        
        cmpasm(exRegs[((thumbinstr&0x7)+0x8)],exRegs[(((thumbinstr>>3)&0x7)+0x8)]);
		
		//update processor flags
		updatecpuflags(0,cpsrasm,0x0);
		
	return 0;
	}
	break;
	
	//MOV
	//MOV rd,hs (5.5)	
	case(0x119):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Rd(%d),Hs(%d) (5.5)\n",(int)(thumbinstr&0x7),(int)(((thumbinstr>>0x3)&0x7)+0x8));
		#endif
		
		exRegs[(thumbinstr&0x7)]=exRegs[(((thumbinstr>>0x3)&0x7)+0x8)];
	return 0;
	}
	break;
	
	//MOV Hd,Rs (5.5)
	case(0x11a):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Hd(%d),Rs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)((thumbinstr>>0x3)&0x7));
		#endif
		
        if (((thumbinstr&0x7)+0x8) == 0xf){
			exRegs[0xf]=exRegs[((thumbinstr&0x7)+0x8)]=(exRegs[((thumbinstr>>0x3)&0x7)]-0x2)&0xfffffffe; //post-add fixup
			#ifdef DEBUGEMU
				iprintf("PC Restore!");
			#endif
		}
		else
			exRegs[((thumbinstr&0x7)+0x8)]=exRegs[((thumbinstr>>0x3)&0x7)];
		
	return 0;
	}
	break;
	
	//MOV hd,hs (5.5)
	case(0x11b):{
		#ifdef DEBUGEMU
			iprintf("[THUMB][WORD] MOV Hd(%d),Hs(%d) (5.5)\n",(int)((thumbinstr&0x7)+0x8),(int)(((thumbinstr>>0x3)&0x7)+0x8));
		#endif
		
        if (((thumbinstr&0x7)+0x8) == 0xf){
			exRegs[0xf]=exRegs[((thumbinstr&0x7)+0x8)]=(exRegs[((thumbinstr>>0x3)&0x7)+0x8]-0x2)&0xfffffffe; //post-add fixup
			#ifdef DEBUGEMU
				iprintf("ROM (MOV) Restore! ");
			#endif
		}
		else
			exRegs[((thumbinstr&0x7)+0x8)]=exRegs[(((thumbinstr>>0x3)&0x7)+0x8)];
		
	}
	break;
	
	//						thumb BX branch exchange (rs)
	case(0x11c):{
		u32 rdaddress=exRegs[((thumbinstr>>0x3)&0x7)];
		
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
		updatecpuflags(1,temppsr,temppsr&PSR_MODE);
	
		exRegs[0xf]=(u32)((rdaddress&0xfffffffe)-0x2); //postadd fixup & align two boundaries
	
		#ifdef DEBUGEMU
			iprintf("BX Rs(%d)! CPSR:%x",(int)((thumbinstr>>0x3)&0x7),(unsigned int)temppsr);
		#endif
		
		return 0;
	}
	break;
	
	//						thumb BX (hs)
	case(0x11D):{
		//hs
		u32 rdaddress=exRegs[(((thumbinstr>>0x3)&0x7)+0x8)];
		
		//BX PC sets bit[0] = 0 and adds 4
		if((((thumbinstr>>0x3)&0x7)+0x8)==0xf){
			rdaddress+=0x2; //+2 now because thumb prefetch will later add 2 more
		}
		
		u32 temppsr;
		temppsr= cpsrvirt & ~(1<<5);	 	//unset bit[5] //align to log2(n) (ARM mode)
		temppsr|=((rdaddress&0x1)<<5);		//set bit[0] from rn
		
		//set CPU <mode> (included bit[5])
		updatecpuflags(1,temppsr,temppsr&PSR_MODE);
	
		exRegs[0xf]=(u32)((rdaddress&0xfffffffe)-0x2); //postadd fixup & align two boundaries
	
		#ifdef DEBUGEMU
			iprintf("BX Hs(%d)! CPSR:%x",(int)(((thumbinstr>>0x3)&0x7)+0x8),(unsigned int)temppsr);
		#endif
		return 0;
	}
	break;
}

//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

return thumbinstr;
}




/////////////////////////////////////ARM virt/////////////////////////////////////////
#ifndef DEBUGEMU
__attribute__((section(".itcm")))
#endif
__attribute__ ((hot))
u32 disarmcode(u32 arminstr){

#ifdef DEBUGEMU
debuggeroutput();
#endif


//validate conditional execution flags: affects branch / ALU Opcodes / and Load Store OPs so they are split
switch((arminstr>>28)&0xf){
    case(0):
        //z set EQ (equ)
        if(!Z_FLAG){ //already cond_mode == negate current status (wrong)
            #ifdef DEBUGEMU
            iprintf("EQ not met! ");
            #endif
            return 0;
        }
        
    break;

    case(1):
    //z clear NE (not equ)
        if(Z_FLAG){
            #ifdef DEBUGEMU
            iprintf("NE not met!");
            #endif
            return 0;
        }
    break;

    case(2):
    //c set CS (unsigned higher)
        if(!C_FLAG) {
            #ifdef DEBUGEMU
            iprintf("CS not met!");
            #endif
            return 0;
        }
    break;

    case(3):
    //c clear CC (unsigned lower)
        if(C_FLAG){
            #ifdef DEBUGEMU
            iprintf("CC not met!");
            #endif
            return 0;
        }
    break;

    case(4):
    //n set MI (negative)
        if(!N_FLAG){
            #ifdef DEBUGEMU
            iprintf("MI not met!");
            #endif
            return 0;
        }
    break;

    case(5):
    //n clear PL (positive or zero)
        if(N_FLAG) {
            #ifdef DEBUGEMU
            iprintf("PL not met!");
            #endif
            return 0;
        }
    break;

    case(6):
    //v set VS (overflow)
        if(!V_FLAG) {
            #ifdef DEBUGEMU
            iprintf("VS not met!");
            #endif
            return 0;
        }
    break;

    case(7):
    //v clear VC (no overflow)
        if(V_FLAG){
            #ifdef DEBUGEMU
            iprintf("VC not met!");
            #endif
            return 0;
        }
    break;

    case(8):
    //c set and z clear HI (unsigned higher)
        if((!C_FLAG)||(Z_FLAG)){
            #ifdef DEBUGEMU
            iprintf("HI not met!");
            #endif
            return 0;
        }
    break;

    case(9):
    //c clear or z set LS (unsigned lower or same)
        if((C_FLAG)||(!Z_FLAG)){
            #ifdef DEBUGEMU
            iprintf("LS not met!");
            #endif
            return 0;
        }
    break;

    case(0xa):
    //(n equals v)
        if( N_FLAG != V_FLAG) {
            #ifdef DEBUGEMU
            iprintf("GE not met!");
            #endif
            return 0;
        }
    break;

    case(0xb):
    //(n not equal to v)
        if( N_FLAG == V_FLAG ){
            #ifdef DEBUGEMU
            iprintf("LT not met!");
            #endif
            return 0;
        }
    break;

    case(0xc):
    // Z clear AND (N equals V) GT (greater than)
    if( (Z_FLAG) || (N_FLAG != V_FLAG) ) {
        #ifdef DEBUGEMU
        iprintf("CS not met!");
        #endif
        return 0;
    }
    break;

    case(0xd):
    //Z set OR (N not equal to V)  LT (less than or equ)
    if( (!Z_FLAG) && (N_FLAG == V_FLAG) ) {
        #ifdef DEBUGEMU
        iprintf("CS not met!");
        #endif
        return 0;
    }
    break;

    case(0xe): 
        //always AL
    break;

    case(0xf):default:{
        //never NV / dont execute anything unhandled
        return 0;
    }
    break;

}



//Opcode Execution starts from here
//
    
//is multiply?
if((((arminstr>>22)&0x3f)==0) && (((arminstr>>4)&0xf)==0x9) ){
    //multiply
    #ifdef DEBUGEMU
        iprintf("[ARM] Multiply \n");
    #endif
    
    u8 set_cond = ((arminstr>>20)&1);
    //MUL or MLA: 0 mul / 1 mla
    u8 mul_opcode = ((arminstr>>21)&1);
    
    u32 rd = ((arminstr>>16)&0xf);
    u32 rm = (arminstr&0xf);
    u32 rs = ((arminstr>>8)&0xf);
    u32 rn = ((arminstr>>12)&0xf);
    
    if(mul_opcode == 0){
        #ifdef DEBUGEMU
            iprintf("(4.21) MUL Rd(%d),Rm(%d),Rs(%d) ",
                (int)(rd),
                (int)(rm),
                (int)(rs)
            );
        #endif
        //MULS Rd=Rm*Rs
        exRegs[rd]=mulasm(exRegs[rm],exRegs[rs]);
    }
    else{
        #ifdef DEBUGEMU
            iprintf("(4.21) MLA Rd(%d),Rm(%d),Rs(%d) ",
                (int)(rd),
                (int)(rm),
                (int)(rs)
            );
        #endif
        //MLAS Rd:=Rm*Rs+Rn
        exRegs[rd]=mlaasm(exRegs[rm],exRegs[rs],exRegs[rn]);
    }
    if(set_cond == 1){
        //update cpu flags
        updatecpuflags(0,cpsrasm,0x0);
    }
    
    return 0;
}

if((((arminstr>>23)&0x1f)==1) && (((arminstr>>4)&0xf)==0x9) ){
    //multiply long
    #ifdef DEBUGEMU
        iprintf("[ARM] Multiply long\n");
    #endif
    
    u32 rdhi = ((arminstr>>16)&0xf);
    u32 rdlo = ((arminstr>>12)&0xf);
    u32 rs = ((arminstr>>8)&0xf);
    u32 rm = (arminstr&0xf);
    
    u8 set_cond = ((arminstr>>20)&1);
    
    //MUL or MLA: 0 mul / 1 mla (64bit)
    u8 mul_opcode = ((arminstr>>21)&1);
    
    //sign bit: 0 unsigned / 1 signed
    u8 sign_bit = ((arminstr>>22)&1);
    
    u32 reglo = 0;
    u32 reghi = 0;
    
    if(mul_opcode == 0){
        if(sign_bit == 0){
            #ifdef DEBUGEMU
                iprintf("(4.17) UMULL Rdlo(%d),Rdhi(%d),Rm(%d),Rn(%d)",
                    (int)(rdlo),
                    (int)(rdhi),
                    (int)(rm),
                    (int)(rs)
                );
            #endif
            umullasm((u32*)&reglo,(u32*)&reghi,exRegs[rm],exRegs[rs]);
        }
        else{
            #ifdef DEBUGEMU
                iprintf("(4.17) SMULL Rdlo(%d),Rdhi(%d),Rm(%d),Rn(%d)",
                    (int)(rdlo),
                    (int)(rdhi),
                    (int)(rm),
                    (int)(rs)
                );
            #endif
            smullasm((u32*)&reglo,(u32*)&reghi,exRegs[rm],exRegs[rs]);
        }
    }
    else{
        if(sign_bit == 0){
            #ifdef DEBUGEMU
                iprintf("(4.17) UMLAL Rdlo(%d),Rdhi(%d),Rm(%d),Rn(%d)",
                    (int)(rdlo),
                    (int)(rdhi),
                    (int)(rm),
                    (int)(rs)
                );
            #endif
            umlalasm((u32*)&reglo,(u32*)&reghi,exRegs[rm],exRegs[rs],exRegs[rdlo],exRegs[rdhi]);
        }
        else{
            #ifdef DEBUGEMU
                iprintf("(4.17) SMLAL Rdlo(%d),Rdhi(%d),Rm(%d),Rn(%d)",
                    (int)(rdlo),
                    (int)(rdhi),
                    (int)(rm),
                    (int)(rs)
                );
            #endif
            smlalasm((u32*)&reglo,(u32*)&reghi,exRegs[rm],exRegs[rs],exRegs[rdlo],exRegs[rdhi]);
        }
    }
    
    exRegs[rdlo] = reglo;
    exRegs[rdhi] = reghi;
    
    if(set_cond == 1){
        //update cpu flags
        updatecpuflags(0,cpsrasm,0x0);
    }
    return 0;
}

/*
//is single data swap?
if( (((arminstr>>20)&0x14)==0x10) && (((arminstr>>4)&0xff)==0x9) ){
    //word quantity
    #ifdef DEBUGEMU
        iprintf("[ARM] SWP \n");
    #endif
    return 0;
}
if( (((arminstr>>20)&0x14)==0x14) && (((arminstr>>4)&0xff)==0x9) ){
    //byte quantity
    #ifdef DEBUGEMU
        iprintf("[ARM] SWPB \n");
    #endif
    return 0;
}
*/

//is branch and exchange? (0x12fff1)
if(((arminstr>>4)&0xffffff)==0x12fff1){
    #ifdef DEBUGEMU
        iprintf("[ARM] BX \n");
    #endif
    
    
    //ARM 4.7 b (Branch exchange) BX
    switch(arminstr & 0x012fff10){
        case(0x012fff10):{
        
            //Rn
            u32 rn = ((arminstr)&0xf);
            u32 temprom=exRegs[rn];
            u32 temppsr=0;
            
            if(rn==0xf){
                #ifdef DEBUGEMU
                    iprintf("(4.7) BX Rn%d[%x]! PC is undefined behaviour!",(int)(rn),(unsigned int)temprom);
                #endif
                while(1);
            }
            
            //(BX ARM-> THUMB value will always add +4)
            if(temprom&0x1){
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
            exRegs[0xf]=(u32)(temprom-(0x4));
            
            //set CPU <mode> (included bit[5]) and keeping all other CPSR bits
            updatecpuflags(1,temppsr,temppsr&PSR_MODE);
            
            #ifdef DEBUGEMU
                iprintf("(4.7) BX rn(%d)[%x]! set_psr:%x",(int)(rn),(unsigned int)(exRegs[0xf]+0x2),(unsigned int)temppsr);
            #endif
        
        return 0;
        }
        break;
    }
    
}

/*
//is halfword data transfer? 
if( (((arminstr>>22)&0x0)==0) && (((arminstr>>4)&0xff)&0x9)
    &&
    (((arminstr>>25)&0x7)==0x0)
 ){
    //register offset
    #ifdef DEBUGEMU
        iprintf("[ARM] strh / ldrh reg offset \n");
    #endif
    return 0;
}
if( (((arminstr>>22)&0x1)==1) && (((arminstr>>4)&0xf)&0x9) 
    &&
    (((arminstr>>25)&0x7)==0x0)
 ){
    //imm offset
    #ifdef DEBUGEMU
        iprintf("[ARM] strh / ldrh imm offset \n");
    #endif
    return 0;
}
*/

if( 
    (
        (
            (   (((arminstr>>22)&0x0)==0) && (((arminstr>>4)&0xf9)==0x9) && (((arminstr>>25)&0x7)==0x0) ) //strh / ldrh reg offset
                ||
            (   (((arminstr>>22)&0x1)==1) && (((arminstr>>4)&0x9)==0x9) && (((arminstr>>25)&0x7)==0x0)  )  //strh / ldrh imm offset
        )
        
        
    )
    ||
    ( 
        ( 
            (((arminstr>>20)&0x14)==0x10)   //SWP
            || 
            (((arminstr>>20)&0x14)==0x14)   //SWPB
        )
        && 
        (((arminstr>>4)&0xff)==0x9)
    )    
)
{
    //iprintf("fallback!");
    
    switch((arminstr>>25)&0x7){
        
        //[ARM] SWPB/SWP/LDRH/STRH opcodes
        case(0b00000000):{
        
            //iprintf("caught as ldr/strh!");
            //modes:
            //rn,#imm+- offset8
            //rn,+-rm
            //rn,#imm+- offset8!
            //rn,+-offset8!
            //rn,+-rm!
            //rn,#+-offset8 post indexed
            //rn,+-rm post indexed
            
            //swp / ldrh / ldsb / ldsh
            u32 type = (arminstr>>4)&0xf;
            u8 offset_type = 0; //1 #imm, 0 offset is register src
            
            if( ((arminstr>>22)&0x1) == 1 ){
               offset_type = 1; 
            }
            else{
               offset_type = 0;
            }
            
            u32 rn=((arminstr>>16)&0xf),rd=((arminstr>>12)&0xf),rm=(arminstr&0xf);
            u32 offset=0;
            u32 address=0;
            u8 load_store = ((arminstr>>20)&1); //0 store / 1 load
            
            //valid opcode?
            if((type == 0b1001) || (type == 0b1011) || (type == 0b1101) || (type == 0b1111)){
            
                //(absolute #Imm value) 
                if(offset_type == 1){
                    //#Imm value (operand 2)		MSB bit[11]---bit[8] | bit[3]---bit[0] LSB
                    offset=(((arminstr>>8)&0xf)<<4)|(arminstr&0xf);
                    #ifdef DEBUGEMU
                        iprintf(" #Imm(0x%x) \n",(unsigned int)offset);
                    #endif
                    
                }
                    
                //If offset register? Rm
                else{
                    //R15/PC must not be chosen at Rm!
                    if (rm==0xf) return 0;
                    offset=exRegs[rm];
                    #ifdef DEBUGEMU
                        iprintf("offset register");			
                    #endif
                }
                    
                //pre indexing bit 	(add offset before transfer)
                if((arminstr>>24)&1){		
                    //up / increase  (Rn+=Rm)
                    if((arminstr>>23)&1){
                        address=(u32)exRegs[rn]+(u32)offset;
                    }
                        
                    //down / decrease bit (Rn-=Rm)
                    else{
                        address=(u32)exRegs[rn]-(u32)offset;
                    }
                        
                    #ifdef DEBUGEMU
                        iprintf("pre-indexed bit! \n");
                    #endif
                        
                    //pre indexed says base is updated [!] (writeback = 1)
                    arminstr |= (1<<21);
                }
                //post indexed bit (add offset below), but we update address with Rn slot for next LDRx/STRx
                else{
                        address=(u32)exRegs[rn];
                }
                
                //decider
                switch(type){
                    //swp
                    case(0b1001):{
                        //swap byte
                        if((arminstr>>22)&1){
                            #ifdef DEBUGEMU
                                iprintf("(4.35) SWPB 1/2 [Rn(%d)]->Rd(%d) \n",(int)(rn),(int)(rd));
                            #endif
                        
                            //[rn]->rd
                            exRegs[rd]=CPUReadByte(exRegs[rn]);
                        
                        
                            #ifdef DEBUGEMU
                            iprintf("(4.35) SWPB 2/2 Rm(%d)->[Rn(%d)] \n",(int)(rm),(int)(rn));
                            #endif
                        
                            //rm->[rn]
                            CPUWriteByte(exRegs[rn],(exRegs[rm]&0xff));
                        }
                        //swap word
                        else{
                            #ifdef DEBUGEMU
                                iprintf("(4.36) SWP 1/2 [Rn(%d)]->Rd(%d) \n",(int)(rn),(int)(rd));
                            #endif
                        
                            //[rn]->rd
                            exRegs[rd]=CPUReadMemory(exRegs[rn]);
                        
                            #ifdef DEBUGEMU
                                iprintf("(4.36) SWP 2/2 Rm(%d)->[Rn(%d)] \n",(int)(rm),(int)(rn));
                            #endif
                        
                            //rm->[rn]
                            CPUWriteMemory(exRegs[rn],(exRegs[rm]));
                        }
                       
                    return 0; //SWP Done
                    }
                    break;
                    
                    //unsigned halfword
                    case(0b1011):{
                        //ldrh
                        if(load_store&1){
                            
                            #ifdef DEBUGEMU
                                if((arminstr>>22)&0){
                                    iprintf("(4.10) LDRH rd(%x),rn(%x),rm(%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)rm);
                                }
                                else{
                                    iprintf("(4.10) LDRH rd(%x),rn(%x),#(0x%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)offset);
                                }
                            #endif
                            
                            exRegs[rd]=CPUReadHalfWord(address);
                        }
                        //strh
                        else{
                            #ifdef DEBUGEMU
                                if((arminstr>>22)&0){
                                    iprintf("(4.10) STRH rd(%x),rn(%x),rm(%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)rm);
                                }
                                else{
                                    iprintf("(4.10) STRH rd(%x),rn(%x),#(0x%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)offset);
                                }
                            #endif
                            
                            CPUWriteHalfWord(address,exRegs[rd]&0xffff);
                        }
                    return 0;
                    }
                    break;
                    
                    //signed byte
                    case(0b1101):{
                        //ldrsb
                        if(load_store&1){
                            
                            #ifdef DEBUGEMU
                                if((arminstr>>22)&0){
                                    iprintf("(4.10) LDRSB rd(%x),rn(%x),rm(%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)rm);
                                }
                                else{
                                    iprintf("(4.10) LDRSB rd(%x),rn(%x),#(0x%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)offset);
                                }
                            #endif
                            
                            exRegs[rd]=(u8)CPUReadByteSigned(address);
                        }
                        //strsb is ignored by armv4 spec
                        else{
                            
                        }
                    return 0;
                    }
                    break;
                    
                    //signed halfword
                    case(0b1111):{
                        //ldrsh
                        if(load_store&1){
                            
                            #ifdef DEBUGEMU
                                if((arminstr>>22)&0){
                                    iprintf("(4.10) LDRSH rd(%x),rn(%x),rm(%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)rm);
                                }
                                else{
                                    iprintf("(4.10) LDRSH rd(%x),rn(%x),#(0x%x) \n",(unsigned int)rd,(unsigned int)rn,(unsigned int)offset);
                                }
                            #endif
                            
                            exRegs[rd]=(u16)CPUReadHalfWordSigned(address);
                        }
                        //strsh is ignored by armv4 spec
                        else{
                            
                        }
                    return 0;
                    }
                    break;
                }
                
                //3)post indexing bit (add offset after transfer)
                if((arminstr>>24)&0){
                    #ifdef DEBUGEMU
                        iprintf("post-indexed bit!");
                    #endif
                    arminstr |= (1<<1);  //writeback is always enabled when post index.
                }
                    
                //4)
                //bit[21]
                //write-back new address into base (updated offset from base +index read) Rn
                // (ALWAYS (if specified) except when R15)
                if( (arminstr&0x2) && (rn!=0xf) ){
                
                    //up / increase  (Rn+=Rm)
                    if((arminstr>>23)&1){
                        address=(u32)exRegs[rn]+(u32)offset;
                    }
                        
                    //down / decrease bit (Rn-=Rm)
                    else{
                        address=(u32)exRegs[rn]-(u32)offset;
                    }
                    
                    exRegs[rn]=address;
                    
                    #ifdef DEBUGEMU
                        iprintf("(new) rn writeback base addr! [%x]",(unsigned int)exRegs[rn]);
                    #endif
                    
                    #ifdef DEBUGEMU
                        iprintf("/******************************/");
                    #endif
                    
                }
        
            }
            return 0; //quit since we executed a valid opcode
        }
        break;
        
    }
}
    
//UNDEFINED INSTRUCTION (conditionally executed). If true it will be executed
if( (((arminstr>>25)&0x7) == 0x3) && (((arminstr>>4)&1)==1) ){
    #ifdef DEBUGEMU
        iprintf("undefined instruction! %x \n",(unsigned int)arminstr);
    #endif
    
    exceptundef(arminstr);
    return 0;
}

//Data Process / PSR Transfer
if(((arminstr>>26)&0x3)==0x0){
    #ifdef DEBUGEMU
        iprintf("Data Process / PSR Transfer \n");
    #endif
    
    //MSR/MRS
    
    //check MSR/MRS bits
    if( 
        ((arminstr&0x3f0000) == 0xf0000) ||
        ((arminstr&0x3ff000) == 0x29f000) ||
        ((arminstr&0x3ff000) == 0x28f000 )
    ){

        //MRS / MSR (use TEQ,TST,CMN and CMP without S bit set)
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
                        iprintf("(4.19) CPSR save!:%x",(unsigned int)cpsrvirt);
                    #endif
                    
                    //MRS
                    exRegs[((arminstr>>12)&0xf)]=cpsrvirt;
                    
                }
                //source PSR is: SPSR<mode> & save cond flags
                else{
                    #ifdef DEBUGEMU
                        iprintf("(4.19) SPSR save!:%x",(unsigned int)spsr_last);
                    #endif
                    
                    //dummyreg=spsr_last;
                    //MRS
                    exRegs[((arminstr>>12)&0xf)]=spsr_last;
                }
            return 0;
            }
            break;
            
            //MSR (transfer Register Rd to PSR)
            case(0x29):{ 	
            
                //CPSR
                if( ((((arminstr>>22)&0x3ff)) &0x1) == 0){
                    
                    //dummy PSR
                    //dummyreg&=0xf90f03ff; //important PSR bits
                    
                    #ifdef DEBUGEMU
                        iprintf("(4.20) CPSR Restore!:(%x)",(unsigned int)(exRegs[(arminstr&0xf)]&ARMV4_PSR_BITS));
                    #endif
                    //cpsrvirt=dummyreg;
                    
                    //modified (cpu state id updated) / //important PSR bits
                    updatecpuflags(1,(exRegs[(arminstr&0xf)]&ARMV4_PSR_BITS),(exRegs[(arminstr&0xf)])&PSR_MODE);
                }
                //SPSR
                else{
                    //dummy PSR
                    //dummyreg&=0xf90f03ff; //important PSR bits
                    
                    #ifdef DEBUGEMU
                        iprintf("(4.20) SPSR restore!:%x",(unsigned int)(exRegs[(arminstr&0xf)]&0xf90f03ff));
                    #endif
                    spsr_last=(exRegs[(arminstr&0xf)]&0xf90f03ff);
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
                        
                        //dummy PSR
                        //dummyreg&=0xf90f03ff; //important PSR bits
                        u32 psrflags=((cpsrvirt) & ~(0xf0000000)) | (exRegs[(arminstr&0xf)]&0xf0000000); //(exRegs[(arminstr&0xf)]&0xf0000000);
                        
                        #ifdef DEBUGEMU
                            iprintf("(4.20) CPSR PSR FLAGS ONLY RESTR from Rd(%d)!:PSR[%x] \n",(int)(arminstr&0xf),(unsigned int)psrflags);
                        #endif
                        
                        updatecpuflags(1,psrflags,cpsrvirt&PSR_MODE);
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
                            iprintf("(4.20) CPSR PSR FLAGS ONLY RESTR from #Imm!:PSR[%x] \n",(unsigned int)psrflags);
                        #endif
                        
                        updatecpuflags(1,psrflags,cpsrvirt&PSR_MODE);
                    }
                }
                //SPSR
                else{
                    //operand reg
                    if( ((arminstr>>25) &0x1) == 0){
                        //rm
                        //dummy PSR
                        //dummyreg&=0xf90f03ff; //important PSR bits
                        u32 psrflags=((spsr_last) & ~(0xf0000000)) | (exRegs[(arminstr&0xf)]&0xf0000000); //(exRegs[(arminstr&0xf)]&0xf0000000);
                        
                        #ifdef DEBUGEMU
                            iprintf("(4.20) SPSR PSR FLAGS ONLY RESTR from Rd(%d)!:PSR[%x] \n",(int)(arminstr&0xf),(unsigned int)psrflags);
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
                            iprintf("(4.20) SPSR PSR FLAGS ONLY RESTR from #Imm!:PSR[%x] \n",(unsigned int)psrflags);
                        #endif
                        
                        spsr_last=psrflags;
                    }
                }
            }
            break;
        }

    }
    
    //ALU Opcodes
    else{
    
        switch((arminstr>>21)&0xf){
            
            case(0x0):case(0x1):case(0x2):case(0x3):case(0x4):case(0x5):case(0x6):case(0x7):case(0x8):
            case(0x9):case(0xa):case(0xb):case(0xc):case(0xd):case(0xe):case(0xf):
            {
                arm_alu_opcode(arminstr);
                return 0;
            }
            break;
        
            default:{
                //dont return from here, in case opcode was none of these earlier        
            }
            break;
        }
        
    }
    
    return 0;
}

//Single Data Transfer
if(
    (((arminstr>>25)&0x7)==0x2) //ldr r0,=0x123 / ldr r0,[r0,#0x10] / Offset is #imm value
    ||
    (((arminstr>>25)&0x7)==0x3) //ldr r3,[r0,r1,lsl #0x2] / offset is register

  ){
    #ifdef DEBUGEMU
        iprintf("Single Data Transfer \n");
    #endif
    
    //4.9 LDR / STR
    //take bit[26] & 0x40 (if set) and save contents if found 
    if(((arminstr>>26)&0x3) == 0x1){
        #ifdef DEBUGEMU
            iprintf("[ARM] 4.9 LDR/STR! \n");
        #endif

        u8 is_load = ((arminstr>>20)&1); //1 load / 0 store
        u8 is_writeback = ((arminstr>>21)&1); //0 no writeback /1 writeback
        u8 is_byte = ((arminstr>>22)&1); //1 byte / 0 word
        u8 updown_offset = ((arminstr>>23)&1); //1 up / 0 down
        u8 pre_index = ((arminstr>>24)&1); //1 pre: add offset before transfer / 0 post : add offset after transfer
        u8 is_imm = ((arminstr>>25)&1);    //0 imm / 1 reg src

        u32 rd = ((arminstr>>12)&0xf);
        u32 rn = ((arminstr>>16)&0xf);

        u32 relative_offset=0;  //relative offset from PC/Rn
        u32 calc_address=0;

        //#offset bit[11]---bit[4] is shifted imm to rm m bit[3]---bit[0] is rm
        if(is_imm == 1){
            u32 rm=(arminstr&0xf);
            u32 shift_rm = ((arminstr&0xff0)>>4);
            
            //R15/PC must not be chosen at Rm!
            if (rm==0xf) return 0;
            
            //ARM shift type 1 (4.5.2) : bit[11]---bit[7] rs value -> rm op rs
            if((shift_rm&1)==1){
                //rs loaded
                u32 rs=(shift_rm>>4)&0xf;
                //shift type
                switch((shift_rm>>1)&0x3){
                    //lsl
                    case(0):{
                        #ifdef DEBUGEMU
                            iprintf("(5.5) LSL Rm(%d),Rs(%d)[%x] \n",(int)(rm),(int)(rs),(unsigned int)exRegs[rs]);
                        #endif
                        //least signif byte (rs) uses: opcode rm,rs
                        relative_offset=lslasm(exRegs[rm],(exRegs[rs]&0xff));
                    }
                    break;
                    
                    //lsr
                    case(1):{
                        #ifdef DEBUGEMU
                            iprintf("(5.6) LSR Rm(%d),Rs(%d)[%x] \n",(int)(rm),(int)(rs),(unsigned int)exRegs[rs]);
                        #endif
                        //least signif byte (rs) uses: opcode rm,rs
                        relative_offset=lsrasm(exRegs[rm],(exRegs[rs]&0xff));		
                    }
                    break;
                    //asr
                    case(2):{
                        #ifdef DEBUGEMU
                            iprintf("(5.3) ASR Rm(%d),Rs(%d)[%x] \n",(int)(rm),(int)(rs),(unsigned int)exRegs[rs]);
                        #endif
                        //least signif byte (rs) uses: opcode rm,rs
                        relative_offset=asrasm(exRegs[rm],(exRegs[rs]&0xff));		
                    }
                    break;
                    //ror
                    case(3):{
                        #ifdef DEBUGEMU
                            iprintf("(5.10) ROR Rm(%d),Rs(%d)[%x] \n",(int)(rm),(int)(rs),(unsigned int)exRegs[rs]);
                        #endif
                        //least signif byte (rs) uses: opcode rm,rs
                        relative_offset=rorasm(exRegs[rm],(exRegs[rs]&0xff));		
                    }
                    break;
                }
                
            }
            
            //ARM shift type 0 (4.5.2)
            else{
                //#imm5 loaded
                u32 imm5 = (arminstr>>7)&0x1f;
            
                switch((shift_rm>>1)&0x3){
                    //lsl
                    case(0):{
                        #ifdef DEBUGEMU
                            iprintf("(5.5) LSL Rm(%d)[%x],#Imm[%x] \n",(int)(rm),(unsigned int)exRegs[rm],(unsigned int)imm5);
                        #endif
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        relative_offset=lslasm(exRegs[rm],imm5);                
                    }
                    break;
                    //lsr
                    case(1):{
                        #ifdef DEBUGEMU
                            iprintf("(5.6) LSR Rm(%d)[%x],#Imm[%x] \n",(int)(rm),(unsigned int)exRegs[rm],(unsigned int)imm5);
                        #endif
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        relative_offset=lsrasm(exRegs[rm],imm5);
                    }
                    break;
                    //asr
                    case(2):{
                        #ifdef DEBUGEMU
                            iprintf("(5.3) ASR Rm(%d)[%x],#Imm[%x] \n",(int)(rm),(unsigned int)exRegs[rm],(unsigned int)imm5);
                        #endif
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        relative_offset=asrasm(exRegs[rm],imm5);
                    }
                    break;
                    //ror
                    case(3):{
                        #ifdef DEBUGEMU
                            iprintf("(5.10) ROR Rm(%d)[%x],#Imm[%x] \n",(int)(rm),(unsigned int)exRegs[rm],(unsigned int)imm5);
                        #endif
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        relative_offset=rorasm(exRegs[rm],imm5);
                    }
                    break;
                }
                
            }
        }

        //#offset bit[11]---bit[0] is #imm
        else{
            relative_offset = (arminstr&0xfff);
        }

        //pre indexing bit 	(add offset before transfer)
        if(pre_index == 1){

            //up / increase  (Rn+=Rm)
            if(updown_offset == 1){
                calc_address=(u32)((int)exRegs[rn]+(int)relative_offset);
            }

            //down / decrease bit (Rn-=Rm)
            else{
                calc_address=(u32)((int)exRegs[rn]-(int)relative_offset);
            }

            #ifdef DEBUGEMU
                iprintf("pre-indexed bit! \n");
            #endif

            //pre indexed says base is updated [!] (writeback = 1)
            //is_writeback = 1;  //debug
        }
        //post indexed, Rn is intact future LDRx/STRx opcodes
        else{
            calc_address=(u32)(exRegs[rn]);
        }
        
        //load/stores opcodes
            
        //bit[20]
        //STR
        if(is_load == 0){
            //transfer byte quantity
            if(is_byte == 1){
                #ifdef DEBUGEMU
                    iprintf("(4.9) STRB Rd(%d)[%x]<-LOADED [Rn(%d),#IMM(0x%x)] \n",(int)(rd),(unsigned int)exRegs[rd],(unsigned int)(rn),(unsigned int)relative_offset);
                #endif
                
                if(rd==0xf){
                    CPUWriteByte(calc_address,(exRegs[rd]&0xff)+0xc);    //pc as store is pc+0xc (prefetch + next opcode) (STRB as well?)
                }
                else{
                    CPUWriteByte(calc_address,(exRegs[rd]&0xff));
                }
            }
            //word quantity
            else{
                #ifdef DEBUGEMU
                    iprintf("(4.9) STR Rd(%d)[%x]<-LOADED [Rn(%d),#IMM(0x%x)] \n",(int)(rd),(unsigned int)exRegs[rd],(unsigned int)(rn),(unsigned int)relative_offset);
                #endif
                    
                if(rd==0xf){
                    CPUWriteMemory(calc_address,exRegs[rd]+0xc);    //pc as store is pc+0xc (prefetch + next opcode)
                }
                else{
                    CPUWriteMemory(calc_address,exRegs[rd]);
                }
            }
        }

        //LDR : 
        else{
            //transfer byte quantity / LDRB rn,#imm
            if(is_byte == 1){
                //if rn == r15 use rom / generate [PC, #imm] value into rd  /   align +8 for prefetching
                if(rn==0xf){
                    exRegs[rd]=CPUReadByte( (u32)( (int)exRegs[0xf]+(int)0x8+(int)relative_offset) );
                }
                //else rn / generate [Rn, #imm] value into rd
                else{
                    exRegs[rd]=CPUReadByte(calc_address);
                }
                //dont move for debugging
                #ifdef DEBUGEMU
                    iprintf("(4.9) LDRB Rd(%d)[%x]<-LOADED [Rn(%d),#IMM(0x%x)] \n",(int)(rd),(unsigned int)exRegs[rd],(unsigned int)(rn),(unsigned int)relative_offset);
                #endif
            }
            //transfer word quantity / LDR rn,#imm
            else{
                //if rn == r15 use rom / generate [PC, #imm] value into rd  /   align +8 for prefetching
                if(rn==0xf){
                    exRegs[rd]=CPUReadMemory((u32)( (int)exRegs[0xf]+(int)0x8+(int)relative_offset));
                }
                //else rn / generate [Rn, #imm] value into rd
                else{
                    exRegs[rd]=CPUReadMemory(calc_address);
                }
                //dont move for debugging
                #ifdef DEBUGEMU
                    iprintf("(4.9) LDR Rd(%d)[%x]<-LOADED [Rn(%d),#IMM(0x%x)] \n",(int)(rd),(unsigned int)exRegs[rd],(unsigned int)(rn),(unsigned int)relative_offset);
                #endif
            }
        }

        //load stores end

        //3)post indexing bit (add offset after transfer)
        if(pre_index == 0){
            #ifdef DEBUGEMU
            iprintf("post-indexed bit!");
            #endif
            
            is_writeback = 1;  //writeback is always enabled when post index.
        }

        //4)
        //bit[21]
        //write-back new address into base (updated offset from base +index read) Rn
        // (ALWAYS (if specified) except when R15)
        if( (is_writeback == 1) && (rn!=0xf) ){
            
            //up / increase  (Rn+=Rm)
            if(updown_offset == 1){
                exRegs[rn]=(u32)((int)calc_address+(int)relative_offset);
            }

            //down / decrease bit (Rn-=Rm)
            else{
                exRegs[rn]=(u32)((int)calc_address-(int)relative_offset);
            }
            
            //dont move this debug info due to proper offset debug
            #ifdef DEBUGEMU
                iprintf("(new) rn writeback base addr! [%x]",(unsigned int)exRegs[rn]);
            #endif
        }

    } //end 4.9


    return 0;
}

//Block Data Transfer
if(((arminstr>>25)&0x7)==0x4){
    #ifdef DEBUGEMU
        iprintf("Block Data Transfer \n");
    #endif
    
    //[ARM] 4.11 STM/LDM
    u32 rn=((arminstr>>16)&0xf);
    int reg_total = (unsigned int)lutu32bitcnt(arminstr&0xffff);
    bool s_bit = ((arminstr>>22)&1) ? true : false;
    int offset=0;
    bool pre_index = ((arminstr>>24)&1) ? true : false; //0 post / 1 pre
    
    //STM bit 0 /LDM bit 1
    //LDM
    if((arminstr>>20)&1){
        #ifdef DEBUGEMU
        iprintf("(4.11) [ARM] LDM Rd(%d), {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
        (unsigned int)rn,
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
        reg_total
        );
        #endif
        
        int cntr=0;
        
        //3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
        if((arminstr>>23)&1){
            #ifdef DEBUGEMU
                iprintf("asc stack! ");
            #endif
            
            while(cntr<0x10){
                if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
                    //post
                    if(pre_index==false){
                        exRegs[cntr]=CPUReadMemory((exRegs[rn])+(offset*4));												                            
                        offset++;
                    }
                    //pre
                    else{
                        offset++;
                        exRegs[cntr]=CPUReadMemory((exRegs[rn])+(offset*4)-4);
                    }
                }
                //LDM ^?
                if((cntr == 15) && s_bit == true){
                    //SPSR<mode> is restored
                    updatecpuflags(1,cpsrvirt,spsr_last&PSR_MODE);    
                }
                cntr++;
            }
        }
        
        //else descending stack 1 bit
        else{
            #ifdef DEBUGEMU
                iprintf("desc stack! ");
            #endif
            while(cntr<0x10){
                if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
                    //post
                    if(pre_index==false){
                        exRegs[cntr]=CPUReadMemory(exRegs[rn]-(reg_total*4)+(offset*4));
                        offset++;
                    }
                    //pre
                    else{
                        offset++;
                        exRegs[cntr]=CPUReadMemory(exRegs[rn]-(reg_total*4)+(offset*4)-4);
                    }
                    
                }
                //LDM ^?
                if((cntr == 15) && s_bit == true){
                    //SPSR<mode> is restored
                    updatecpuflags(1,cpsrvirt,spsr_last&PSR_MODE);    
                }
                cntr++;
            }
        }
    }
    
    //STM
    else{
        #ifdef DEBUGEMU
        iprintf("(4.11) [ARM] STM Rd(%d), {R: %d %d %d %d %d %d %d %x \n %d %d %d %d %d %d %d %d } \n",
        (unsigned int)rn,
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
        (unsigned int)reg_total
        );
        #endif
        
        int cntr=0;
        
        //3a) up / ascending 0, bit  (add cpsr bit[5] depth from Rn)
        if((arminstr>>23)&1){
            #ifdef DEBUGEMU
                iprintf("asc stack! ");
            #endif
            
            while(cntr<0x10){
                if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
                    //STM rn,pc?  
                    //a)Whenever R15 is stored to memory the stored value is the address of the STM instruction plus 12
                    //b)spsr is user psr
                    if(cntr == 15){
                        spsr_last=spsr_usr;
                        //post
                        if(pre_index==false){
                            CPUWriteMemory(exRegs[rn]+(offset*4),exRegs[cntr]+0xc);
                            offset++;
                        }
                        //pre
                        else{
                            offset++;
                            CPUWriteMemory(exRegs[rn]+(offset*4)-4,exRegs[cntr]+0xc);
                        }
                    }
                    else{
                        //post
                        if(pre_index==false){
                            CPUWriteMemory(exRegs[rn]+(offset*4),exRegs[cntr]);
                            offset++;
                        }
                        //pre
                        else{
                            offset++;
                            CPUWriteMemory(exRegs[rn]+(offset*4)-4,exRegs[cntr]);
                        }
                    }
                }
                cntr++;
            }
        }
        
        //else descending stack 1 bit
        else{
            #ifdef DEBUGEMU
                iprintf("desc stack! ");
            #endif
            while(cntr<0x10){
                if( ((1<<cntr) & (arminstr&0xffff)) > 0 ){
                    //STM rn,pc?
                    //a)Whenever R15 is stored to memory the stored value is the address of the STM instruction plus 12
                    //b)spsr is user psr
                    if(cntr == 15){
                        spsr_last=spsr_usr;
                        //post
                        if(pre_index==false){
                            CPUWriteMemory(exRegs[rn]-(reg_total*4)+(offset*4),exRegs[cntr]+0xc);
                            offset++;
                        }
                        //pre
                        else{
                            offset++;
                            CPUWriteMemory(exRegs[rn]-(reg_total*4)+(offset*4)-4,exRegs[cntr]+0xc);
                        }
                    }
                    else{
                        //post
                        if(pre_index==false){
                            CPUWriteMemory(exRegs[rn]-(reg_total*4)+(offset*4),exRegs[cntr]);
                            offset++;
                        }
                        //pre
                        else{
                            offset++;
                            CPUWriteMemory(exRegs[rn]-(reg_total*4)+(offset*4)-4,exRegs[cntr]);
                        }
                    }
                }
                cntr++;
            }
        }
    }
        
        
    //4)post indexing bit (add offset after transfer) (by default on stmia/ldmia opcodes)
    if(pre_index==false){
        #ifdef DEBUGEMU
            iprintf("post indexed (default)! \n");
        #endif
        arminstr|=(1<<21); //forces the writeback post indexed base
    }
    
    //5)
    //write-back new address into base (updated offset from base +index read) Rn
    if((arminstr>>21)&1){        
        #ifdef DEBUGEMU
            iprintf(" updated addr: %x / Bytes workd onto stack: %d \n", (unsigned int)exRegs[rn],(int)(reg_total*4));
        #endif
        
        //if ascending  (add cpsr bit[5] depth from Rn)
        if((arminstr>>23)&1){
            exRegs[rn]=(u32)((int)exRegs[rn]+(int)(reg_total*4));
        }
        //else descending
        else{
            exRegs[rn]=(u32)((int)exRegs[rn]-(int)(reg_total*4));
        }
    }
    //else: don't write-back address into base
    return 0;
}

//Branch
if(((arminstr>>25)&0x7)==0x5){
    #ifdef DEBUGEMU
        iprintf("Branch \n");
    #endif
    
    //[ARM] 4.5 Branch / Branch Conditional / Branch with Link
    switch(arminstr & 0xff000000){
        //EQ equal
        case(0x0a000000):case(0x0b000000):{
            if (Z_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BEQ ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //NE not equal
        case(0x1a000000):case(0x1b000000):{
            if (!Z_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BNE ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //CS unsigned higher or same
        case(0x2a000000):case(0x2b000000):{
            if (C_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BCS ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //CC unsigned lower
        case(0x3a000000):case(0x3b000000):{
            if (!C_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BCC ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //MI negative
        case(0x4a000000):case(0x4b000000):{
        if (N_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BMI ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //PL Positive or Zero
        case(0x5a000000):case(0x5b000000):{
            if (!N_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BPL ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //VS overflow
        case(0x6a000000):case(0x6b000000):{
            if (V_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BVS ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //VC no overflow
        case(0x7a000000):case(0x7b000000):{
            if (!V_FLAG){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BVC ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //HI usigned higher
        case(0x8a000000):case(0x8b000000):{
            if ( (C_FLAG) && (!Z_FLAG) ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BHI ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //LS unsigned lower or same
        case(0x9a000000):case(0x9b000000):{
            if ( (!C_FLAG) || (Z_FLAG) ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BLS ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //GE greater or equal
        case(0xaa000000):case(0xab000000):{
            if ( ((N_FLAG) && (V_FLAG)) || ((!N_FLAG) && (!V_FLAG)) ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BGE ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //LT less than
        case(0xba000000):case(0xbb000000):{
            if ( ((N_FLAG) && (!V_FLAG)) || ((!N_FLAG) && (V_FLAG)) ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BLT ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //GT greather than
        case(0xca000000):case(0xcb000000):{
            if ( (!Z_FLAG) && (((N_FLAG) && (V_FLAG)) || ((!N_FLAG) && (!V_FLAG))) ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BGT ");
                #endif
                arm_do_branch(arminstr);
            }
        return 0;
        }
        break;
        
        //LE less than or equal
        case(0xda000000):case(0xdb000000):{
            if ( 	((!Z_FLAG) || ( (N_FLAG) && (!V_FLAG))) 
                    ||
                    ((!N_FLAG) && (V_FLAG) )
                ){
                #ifdef DEBUGEMU
                    iprintf("(4.2) BLE ");
                #endif
                arm_do_branch(arminstr);    
            
            }
        return 0;
        }
        break;
        
        //AL always / Branch
        case(0xea000000):case(0xeb000000):{
            #ifdef DEBUGEMU
                iprintf("(4.2) BAL ");
            #endif
            arm_do_branch(arminstr);
        return 0;
        }
        break;
        
        //NV never
        case(0xfa000000):case(0xfb000000):{
            #ifdef DEBUGEMU
                iprintf("(4.2) BNV ");
            #endif
            //undefined by ARMv4 spec
            //arm_do_branch(arminstr);
            return 0;
        }
        break;
        
    }
}

//Co processor data transfer
if(((arminstr>>25)&0x7)==0x6){
    #ifdef DEBUGEMU
        iprintf("Co Processor Data Transfer \n");
    #endif
    return 0;
}

//Co processor data Operation
if( (((arminstr>>24)&0xf)==0xe) && (((arminstr>>4)&1)==0) ){
    #ifdef DEBUGEMU
        iprintf("Co Processor Data Operation \n");
    #endif
    return 0;
}

//Co processor register Transfer
if( (((arminstr>>24)&0xf)==0xe) && (((arminstr>>4)&1)==1) ){
    #ifdef DEBUGEMU
        iprintf("Co Processor Register Transfer \n");
    #endif
    return 0;
}

//SWI
if(((arminstr>>24)&0xf)==0xf){
    #ifdef DEBUGEMU
        iprintf("SWI \n");
    #endif
    
    //4.34 software interrupt
    switch( (arminstr) & 0xf000000 ){
        case(0xf000000):{
            
            #ifdef DEBUGEMU
                iprintf("(4.34) SWI #0x%x! ",(unsigned int)(arminstr&0xffffff));
            #endif
            
            armState = true;
            armIrqEnable=false;
            
            //SPSR is saved
            updatecpuflags(1,cpsrvirt,PSR_SVC);
            
            swi_virt(arminstr&0xffffff);
            
            exRegs[0xe] = (u32)exRegs[0xf] - (u32)( (armState == true) ? 4 : 2);
            
            #ifdef BIOSHANDLER
                exRegs[0xf]  = (u32)(0x08-0x4);
            #else
                //continue til BX LR (ret address cback) (for protected bios reads)
            #endif
            
            //SPSR<mode> is restored
            updatecpuflags(1,cpsrvirt,spsr_last&PSR_MODE);
            
        }
    }
    return 0;
}
    
//4.1 (Co- Processor calls) gba has not CP

//4.1 CDP not implemented on gba

//4.1 STC / LDC (copy / restore memory to cause undefined exception) not emulated

//4.1 MRC / MCR not implemented / used on GBA ( Co - processor)

//REQUIRED so DTCM and EWRAM have sync'd pages
drainwrite();

return 0;
}