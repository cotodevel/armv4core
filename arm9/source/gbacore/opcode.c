#include "opcode.h"
#include "gba.arm.core.h"
#include "translator.h"
#include "../settings.h"

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


//extract_word(u32 * buffer (haystack), u32 word (needle) , int buffsize, u32 * buffer (output data),u32 delimiter_word), u8 type = 0 character string / 1 = hex
//extracts all values until "delimiter"
int extract_word(u32 * buffer,u32 word ,u32 * buffer_out,u32 delimiter,u8 type){ //strings shall be constant (not movables) for strlen
int i=0,temp=0,m=0;

//removed because code was fugly
//char string search
if (type==0){
	//iprintf("char string search!");
	/*
	//m=0;
	for(i=0;i<size;i++){
		k=0;
		for(j=m;j<strlen(word);j++){
		
			if  ( (( ( buffer[i]>>(k*8) )&0xff ) == (word[j]) ) ){ //j gets actual pos, does not always begin from 0
				temp++;
				//debug
				//printf(" %x_i - k(%d)\n",( ( buffer[i]>>(k*8) )&0xff ),k);
				//j=strlen(word);
				m++; //increase word segment only til reach top
			}
			
			//not found? then reset counter
			else {
				m=0;
				j=strlen(word);
			}
			
			//did we reach top bit 8*3?
			if(k==3) {
				k=0;
				j=strlen(word); //next i
			}
			else k++;
		}
		
		//found word? get everything starting from there until delimiter
		if (temp >= strlen(word)){
			m=0;
			//0 gets re used by m, twice, both for reset, and as a valid offset. this causes double valid word found.
			if (temp != strlen(word)) { 
				//k-=1;
			}
			else i++; //aligned means the current i index hasn't changed, next index is what we look for.
			
				//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				while( (u32)( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				printf(" %x_i - k(%d)\n",(unsigned int)( ( buffer[i]>>(k*8) )&0xff ), (int)k);
				
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=( ( buffer[i]>>(k*8) ) );
				
				m++;
		
				if(k==3) {
					k=0;
					i++;
				}
				else k++;
				
			}
		break;
		}
	}
*/
}
//hex search
else if (type==32){
	//iprintf("hex search!");
	//iprintf("hex to look: %x",(unsigned int)word);
	int ret_val = 128*4;    //retries
    
    for(i=0;i<ret_val/4;i++){
		if  (buffer[i] == word ){ //j gets actual pos, does not always begin from 0
				temp++;
		}
        /*
		if(temp>0) {
            iprintf("found %x match!",word);
            while(1==1);
        }
        */
		//found word? get everything starting from there until delimiter
		if (temp > 0){
			//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
			while(buffer[m] != delimiter){
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=buffer[m];
				m++;				
			}
			buffer_out[m]=buffer[m]; //and copy over delimiter's last opcode
			
			m++; //offset fix for natural numbers (at least 1 was found!)
			break; //done? kill main process
		}
        
	}
    
    if(temp == 0){
        return 0;
    }
}
return m;
}


//moves data from host to emulator map
void nds_2_gba_copy(u8 * source,u8 * dest,int size){
    int i = 0;
    for(i = 0 ; i < size ; i++){
        CPUWriteByte_stack(((u32)(u8*)dest)+i, source[i]);
    }
}

//moves data from emulator to host map
void gba_2_nds_copy(u8 * source,u8 * dest,int size){
    int i = 0;
    for(i = 0 ; i < size ; i++){
       dest[i] = CPUReadByte_stack(source[i]);
    }
}

//translator.c assist functions

#ifdef LIBNDS
__attribute__((section(".itcm")))
#endif
u32 BASE_ADDR(u32 addr){
    addr=addr>>base_factor;
    return (addr<<base_factor);
}

#ifdef LIBNDS
__attribute__((section(".itcm")))
#endif

//[ARM] Branch Opcode
s32 arm_do_branch(u32 arm_opcode){
    
    //Branch & Branch w Link can:
    //+/- 32MB of addressing. so up to 0x01ffffff; then added to BASE PC.
	//also keep in mind prefetching (8 byte / 2 words ahead of current instruction for new address)
	
    s32 s_word=((arm_opcode&0xffffff)<<2);
    s32 old_pc = (s32)exRegs[0xf]+(s32)0x8;     //PREFETCH is +8
    exRegs[0xf]= ((u32)(s_word + old_pc - 0x4))&0x09ffffff;         //PREFETCH is +8 but -4 due to postfetch 
	
	//link bit
	if( ((arm_opcode>>24)&1) == 1){
        //LR=PC
		exRegs[0xe]=(u32)(old_pc&~0x3)-0x4;                         //PREFETCH is +8 but -4 due to postfetch 
		#ifdef DEBUGEMU
            printf("link bit! R14: %x",(unsigned int)exRegs[0xe]);
		#endif
	}
    
    #ifdef DEBUGEMU
        printf("new PC:%x ",(unsigned int)(exRegs[0xf]+0x4));
    #endif
    
    return exRegs[0xf];
}

//[ARM] ALU Opcode
void arm_alu_opcode(u32 arm_opcode){
    //flags
    u8 opcode_index = ((arm_opcode>>21)&0xf);
    u8 imm_op   = ((arm_opcode>>25)&1);
    u8 set_cond = ((arm_opcode>>20)&1);
    
    //scratchpad regs
    u32 scr_reg = 0;
    
    //Registers with required PC offset
    int pc_offset_rn=0;
    int pc_offset_rm=0;
    int pc_offset_rs=0;
    
    //Rd is outter scope
    u32 rd = ((arm_opcode>>12)&0xf);
    
    //Rn is outter scope 
	u32 rn = ((arm_opcode>>16)&0xf);
    
    //Calculate PC offset for Rn
    if (rn==0xf){
        if(imm_op==1){
            pc_offset_rn=exRegs[rn]+(8);    //+#imm? 8 for prefetch
        }
        else{
            pc_offset_rn=exRegs[rn]+(12);   //shift specified in register? 12 for prefetch
        }
    }
    else{
        pc_offset_rn=exRegs[rn];
    }
    
    //rd is target so it will be destroyed, no need to calc PC offset
    
    
    if(imm_op==1){	//for #Inmediate OP operate
        //Rn (1st op reg) 		 bit[19]---bit[16]
        //#Imm (operand 2)		 bit[11]---bit[0]
        //rotate field:
        //#imm value is zero extended to 32bit, then subject
        //to rotate right by twice the value in rotate field:
        u32 rotate_value = (2*((arm_opcode>>8)&0xf)); 
        u8 imm = (arm_opcode&0xff);
        scr_reg=rorasm(imm,rotate_value);
        
        //ready to execute below
    }
    else{	//for Register (operand 2) operator / shift included
        //Rm (operand 2 )		 bit[3]---bit[0]
        u32 rm = (arm_opcode&0xf);
        //shifting part: bit[11]---bit[4]
        u32 shift_block = (arm_opcode&0xfff);
        u16 shift_operation = (shift_block>>4); //0 = ARM #Imm Shift Op / 1 = ARM Rs Shift Op
        
        //Calculate PC Offset for Rm
        if (rm==0xf){
            if(imm_op==1){
                pc_offset_rm=exRegs[rm]+(8);    //+#imm? 8 for prefetch
            }
            else{
                pc_offset_rm=exRegs[rm]+(12);   //shift specified in register? 12 for prefetch
            }
        }
        else{
            pc_offset_rm=exRegs[rm];
        }
        
        //ARM Shift Op
        switch(shift_operation&0x1){
            
            //case 0
            //#Imm ammount shift & opcode to Rm
            case(0):{
                u8 imm8 = ((arm_opcode>>7)&0x1f);
                
                //1)"The assembler will convert LSR #0 (and ASR #0 and ROR #0) into LSL #0, and allow LSR #32 to be specified."
                //2) LSL #0 is a special case, where the op is the old value of the CPSR C flag. 
                //The contents of Rm are used directly as the second operand.
                
                //Shift Type (0)
                switch((shift_operation>>1)&0x3){                        
                    //LSL Rm,#Imm bit[5]
                    case(0x0):{
                        if(imm8 == 0){
                            //op2=rm directly
                        }
                        
                        scr_reg=lslasm(pc_offset_rm,imm8);
                        
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        #ifdef DEBUGEMU
                            printf("(5.5) LSL Rm(%x),#Imm[%x] \n",(unsigned int)(rm),(unsigned int)(imm8));
                        #endif
                    }
                    break;
                    //lsr
                    case(0x1):{
                        
                        if(imm8 == 0){
                            if(((pc_offset_rm>>31)&1)==1){
                                C_FLAG = true;
                            }
                            else{
                                C_FLAG = false;
                            }
                            
                            scr_reg=pc_offset_rm=imm8=0;
                        }
                        else{
                            scr_reg=lsrasm(pc_offset_rm,imm8);
                        }
                        
                        
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        #ifdef DEBUGEMU
                            printf("(5.6) LSR Rm(%x),#Imm[%x] \n",(unsigned int)(rm),(unsigned int)(imm8));
                        #endif
                    }
                    break;
                    //asr
                    case(0x2):{
                        if( imm8 == 0 ){
                            if(((pc_offset_rm>>31)&1)==1){
                                C_FLAG = true;
                                scr_reg=0xffffffff;
                            }
                            else{
                                C_FLAG = false;
                                scr_reg=0;
                            }
                        }
                        else{
                            scr_reg=asrasm(pc_offset_rm,imm8);
                        }
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        #ifdef DEBUGEMU
                            printf("(5.3) ASR Rm(%x),#Imm[%x] \n",(unsigned int)(rm),(unsigned int)(imm8));
                        #endif
                        
                    }
                    break;
                    //ror
                    case(0x3):{
                        if( imm8 == 0 ){
                            if(((pc_offset_rm>>31)&1)==1){
                                scr_reg|=(1<<31);
                            }
                            else{
                                scr_reg&=~(1<<31);
                            }
                        }
                        else{
                            scr_reg = rorasm(pc_offset_rm,imm8);
                        }
                        //bit[11]---bit[7] #Imm used opc rm,#Imm
                        #ifdef DEBUGEMU
                            printf("(5.10) ROR Rm(%x),#Imm[%x] \n",(unsigned int)(rm),(unsigned int)(imm8));
                        #endif
                    }
                    break;
                }
            }
            break;
            case(1):{
                u32 rs = ((shift_operation>>4)&0xf);
                
                //Calculate PC Offset for Rs
                if (rs==0xf){
                    if(imm_op==1){
                        pc_offset_rs=(exRegs[rs]&0xff)+(8);    //+#imm? 8 for prefetch
                    }
                    else{
                        pc_offset_rs=(exRegs[rs]&0xff)+(12);   //shift specified in register? 12 for prefetch
                    }
                }
                else{
                    pc_offset_rs=(exRegs[rs]&0xff);
                }
                
                //Shift Type (1)
                switch((shift_operation>>1)&0x3){                        
                    case(0x0):{
                        #ifdef DEBUGEMU
                            printf("(5.5) LSL Rm(%x),Rs(%x)[%x] \n",(unsigned int)(rm),(unsigned int)(rs),(unsigned int)pc_offset_rs);
                        #endif
                        //least signif byte (rs) used opc rm,rs
                        scr_reg=lslasm(pc_offset_rm,pc_offset_rs);
                    }
                    break;
                    case(0x1):{
                        #ifdef DEBUGEMU
                            printf("(5.6) LSR Rm(%x),Rs(%x)[%x] \n",(unsigned int)(rm),(unsigned int)(rs),(unsigned int)pc_offset_rs);
                        #endif
                        //least signif byte (rs) used opc rm,rs
                        scr_reg=lsrasm(pc_offset_rm,pc_offset_rs);
                    }
                    break;
                    case(0x2):{
                        #ifdef DEBUGEMU
                            printf("(5.3) ASR Rm(%x),Rs(%x)[%x] \n",(unsigned int)(rm),(unsigned int)(rs),(unsigned int)pc_offset_rs);
                        #endif
                        //least signif byte (rs) used opc rm,rs
                        scr_reg=asrasm(pc_offset_rm,pc_offset_rs);
                    }
                    break;
                    case(0x3):{
                        #ifdef DEBUGEMU
                            printf("(5.10) ROR Rm(%x),Rs(%x)[%x] \n",(unsigned int)(rm),(unsigned int)(rs),(unsigned int)pc_offset_rs);
                        #endif
                        //least signif byte (rs) used opc rm,rs
                        scr_reg=rorasm(pc_offset_rm,pc_offset_rs);
                    }
                    break;
                }
            }
            break;
        }     
        
        //ready to execute below
    }
    
    
    //Execute Opcode
    switch(opcode_index){
        //AND
        case(0b00000000):{
            exRegs[rd]=andasm(pc_offset_rn,scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) AND Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //EOR
        case(0b00000001):{
            exRegs[rd]=eorasm(pc_offset_rn,scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) EOR Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //SUB
        case(0b00000010):{
            exRegs[rd]=subasm(pc_offset_rn,scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) SUB Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //RSB
        case(0b00000011):{
            exRegs[rd]=rsbasm(pc_offset_rn,scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) RSB Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //ADD
        case(0b00000100):{
            exRegs[rd]=addasm(pc_offset_rn,scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) ADD Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //ADC
        case(0b00000101):{
            exRegs[rd]=adcasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) ADC Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //SBC
        case(0b00000110):{
            exRegs[rd]=sbcasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) SBC Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //RSC
        case(0b00000111):{
            exRegs[rd]=rscasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) RSC Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //TST
        case(0b00001000):{
            tstasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) TST Rn(%d),BarrelShiftedValue(%x) \n",(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //TEQ
        case(0b00001001):{
            teqasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) TEQ Rn(%d),BarrelShiftedValue(%x) \n",(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //CMP
        case(0b00001010):{
            cmpasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) CMP Rn(%d),BarrelShiftedValue(%x) \n",(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //CMN
        case(0b00001011):{
            cmnasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) CMN Rn(%d),BarrelShiftedValue(%x) \n",(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //ORR
        case(0b00001100):{
            exRegs[rd]=orrasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) ORR Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //MOV
        case(0b00001101):{
            if((rd)==0xf){
                exRegs[rd]=movasm(scr_reg-0x4); //postfetch -0x4
            }
            else{
                exRegs[rd]=movasm(scr_reg);
            }
            #ifdef DEBUGEMU
                printf("(4.10) MOV Rd(%d),Rm+BarrelShiftedValue(%x) \n",(int)(rd),(int)(scr_reg));
            #endif
        }
        break;
        //BIC
        case(0b00001110):{
            exRegs[rd]=bicasm(pc_offset_rn,scr_reg);    
            #ifdef DEBUGEMU
                printf("(4.10) BIC Rd(%d),Rn(%d),BarrelShiftedValue(%x) \n",(int)(rd),(int)(rn),(int)(scr_reg));
            #endif
        }
        break;
        //MVN
        case(0b00001111):{
            exRegs[rd]=mvnasm(scr_reg);
            #ifdef DEBUGEMU
                printf("(4.10) MVN Rd(%d),Rm+BarrelShiftedValue(%x) \n",(int)(rd),(int)(scr_reg));
            #endif
        }
        break;
    }
    
    //check for S bit here and update (virt<-asm) processor flags
    if(set_cond==1)
        updatecpuflags(0,cpsrasm,0x0);
}