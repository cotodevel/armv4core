//CPU opcodes (that do not rely on translator part, but GBA opcodes) here!

#include <nds.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <fat.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <filesystem.h>
#include <dirent.h>
#include <time.h> //NDS7 clock

#include "supervisor.h"

#include "pu.h"
#include "../gbacore/opcode.h"
#include "../gbacore/util.h"
#include "../gbacore/translator.h"
#include "../gbacore/bios.h"
#include "../gbacore/gba.arm.core.h"
#include "../gbacore/spinlock.h"

//disk
#include "../disk/fatmore.h"

//sharedIPC
#include "gba_ipc.h"

//shared clock
#include "../../common/clock.h"

#include "../settings.h"





//GBA Interrupts
//4000200h - IE - Interrupt Enable Register (R/W)
//  Bit   Expl.
//  0     LCD V-Blank                    (0=Disable)
//  1     LCD H-Blank                    (etc.)
//  2     LCD V-Counter Match            (etc.)
//  3     Timer 0 Overflow               (etc.)
//  4     Timer 1 Overflow               (etc.)
//  5     Timer 2 Overflow               (etc.)
//  6     Timer 3 Overflow               (etc.)
//  7     Serial Communication           (etc.)
//  8     DMA 0                          (etc.)
//  9     DMA 1                          (etc.)
//  10    DMA 2                          (etc.)
//  11    DMA 3                          (etc.)
//  12    Keypad                         (etc.)
//  13    Game Pak (external IRQ source) (etc.)


void vblank_thread(){
 
			if(0 == (REG_KEYINPUT & (KEY_START))){


///////////////////////////////////opcode test

//tempbuffer[0x1]=(u32)call_adrstack[0x3]; //r1 branch thumb opcode

//swp r0,r1,[r2]

//tempbuffer[0x0]=0xc00ffee0; //patched bios address
//tempbuffer[0x0]=0xc00ffeee;
//tempbuffer[0x0]=(u32)&gbabios[0];
//tempbuffer[0x1]=0xc0ca;
//tempbuffer[0x1]=0x0;
//tempbuffer[0x1]=(u32)&tempbuffer2[0];
//tempbuffer[0x1]=0x3;

//tempbuffer[0x2]=(u32)(u32*)rom;

//tempbuffer[0x2]=(u32)&tempbuffer2[0];
//tempbuffer[0x2]=0xff;
//tempbuffer[0x2]=(u32)&tempbuffer2[0]; //COFFE1 SHOULD BE AT tempbuf2[0]

//tempbuffer2[0]=0x0; //AND COFFEE 2 SHOULD BE AT R0

//tempbuffer[0x3]=0xc33ffee3;
//tempbuffer[0x3]=(u32)(u32*)rom;

//tempbuffer[0x3]=(u32)&tempbuffer2[0x0];

//tempbuffer2[1]=0xcac0c0ca;

//tempbuffer[0x3]=(u32)&tempbuffer[0xc];
//tempbuffer[0x3]=(u32)(u32*)rom;
//tempbuffer[0x4]=0xc00ffee4;
//tempbuffer[0x4]=0xc0c0caca;
//tempbuffer[0x4]=(u32)(u32*)rom;
//tempbuffer[0x5]=0xc00ffee5;

//tempbuffer[0x6]=-56;
//tempbuffer[0x6]=0xc00ffee6; //for some reason ldrsb does good with sign bit 2^8 values, but -128 is Z flag enabled  (+127 / -128) 
//tempbuffer[0x6]=(u32)(u32*)rom;

//tempbuffer[0x7]=0xc00ffee7;
//tempbuffer[0x8]=0xc00ffee8;
//tempbuffer[0x9]=0xc00ffee9;
//tempbuffer[0xa]=0xc00ffeea;
//tempbuffer[0xb]=0xc00ffeeb;
//tempbuffer[0xc]=0xc00ffeec;

//*((u32*)gbastckmodeadr_curr+0)=0xfedcba98;

//stack ptr
//tempbuffer[0xd]=(u32)(u32*)gbastckmodeadr_curr;
//tempbuffer[0xd]=(u32)(u32*)gbastckmodeadr_curr;

//tempbuffer[0xd]=0xc00ffeed;

//tempbuffer[0xe]=0xc070eeee;
//tempbuffer[0xf]=rom;

//c07000-1:  usr/sys  _ c17100-6: fiq _ c27200-1: irq _ c37300-1: svc _ c47400-1: abt _ c57500-1: und

gbavirtreg_r13usr[0]=0xc07000;
gbavirtreg_r14usr[0]=0xc07001;

gbavirtreg_r13fiq[0]=0xc17100;
gbavirtreg_r14fiq[0]=0xc17101;
gbavirtreg_fiq[0x0]=0xc171002;
gbavirtreg_fiq[0x1]=0xc171003;
gbavirtreg_fiq[0x2]=0xc171004;
gbavirtreg_fiq[0x3]=0xc171005;
gbavirtreg_fiq[0x4]=0xc171006;

gbavirtreg_r13irq[0]=0xc27200;
gbavirtreg_r14irq[0]=0xc27201;

gbavirtreg_r13svc[0]=0xc37300;
gbavirtreg_r14svc[0]=0xc37301;

gbavirtreg_r13abt[0]=0xc47400;
gbavirtreg_r14abt[0]=0xc47401;

gbavirtreg_r13und[0]=0xc57500;
gbavirtreg_r14und[0]=0xc57501;

//tempbuffer[0x0]=0xc0700000;
//tempbuffer[0x1]=0xc0700001;
//tempbuffer[0x2]=0xc0700002;
//tempbuffer[0x3]=0xc0700003;
//tempbuffer[0x4]=0xc0700004;
//tempbuffer[0x5]=0xc0700005;
//tempbuffer[0x6]=0xc0700006;
//tempbuffer[0x7]=0xc0700007;
//tempbuffer[0x8]=0xc0700008;
//tempbuffer[0x9]=0xc0700009;
//tempbuffer[0xa]=0xc070000a;
//tempbuffer[0xb]=0xc070000b;
//tempbuffer[0xc]=0xc070000c;
//tempbuffer[0xd]=0xc070000d;
//tempbuffer[0xe]=0xc070000e;


//writing to (0x7fff for stmiavirt opcode) destroys PC , be aware that buffer_input[0xf] slot should be filled before rewriting PC.
//0 for CPURES/BACKUP / 2 for stack/push pop / 1 free

//stack pointer test
/*
iprintf("old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xb07f); //add sp,#508

iprintf("new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

iprintf("old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);


disthumbcode(0xb0ff); //add sp,#508

iprintf("new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/


// PUSH / POP REG TEST OK
/*
iprintf(" 1/2 old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr); 

disthumbcode(0xb4ff);  //push r0-r7

//clean registers
for(i=0;i<16;i++){
	*((u32*)exRegs[0]+(i))=0x0;
}
iprintf(" 1/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

iprintf(" 2/2 old sfp:%x ",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xbcff); //pop r0-r7

iprintf(" 2/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/

// PUSH r0-r7,LR / POP r0,-r7,PC	REG TEST OK
/*
iprintf(" 1/2 old sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xb5ff);  //push r0-r7,LR

//clean registers
for(i=0;i<16;i++){
	*((u32*)exRegs[0]+(i))=0x0;
}
rom=0x0;

iprintf(" 1/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);

iprintf(" 2/2 old sfp:%x ",(u32)(u32*)gbastckfpadr_curr);

disthumbcode(0xbdff); //pop r0-r7,PC

iprintf(" 2/2 new sfp:%x \n",(u32)(u32*)gbastckfpadr_curr);
*/

/* //single stack save/restore gbareg 5.11
//backup r3 (5.11)
disthumbcode(0x9302); //str r3,[sp,#0x8]

dummyreg2=0;
faststr((u8*)&dummyreg2, (u32)exRegs[0], (0x3), 32,0);

//restore (5.11)
disthumbcode(0x9b02); //ldr r3,[sp,#0x8]
*/

//*(u32*)gbastckfpadr_curr++;

//reads: u8 = 1 byte depth / index / 16u = 2 / 32u = 4


/*
//stmia / ldmia test 5.15
disthumbcode(0xc3ff); // stmia r3!,{rdlist}

//clean registers

for(i=0;i<16;i++){
	if(i!=0x2 )
		exRegs[i]=0x0;

}

disthumbcode(0xcaff); // ldmia r2!,{rdlist}
//disthumbcode(0xcbff); // ldmia r3!,{rdlist} //r3 doesnt work as the address is incremented, ldmia requires same of as stmia
*/


//disthumbcode(0x4708); //bx r1 (branch to ARM mode)
//disthumbcode(0x4709); //bx r1 (branch to thumb mode)
//disthumbcode(0x472d); //bx r5 (branch to thumb code)


//branch test
//disthumbcode(0x2901); 	//cmp r1,#0x1
//disthumbcode(0xd080);		//beq #0x100


//disthumbcode(0xdf01);	//swi #1
// 0xfffffffc = thumb 0x80 lsl r0,r0[0xffffffff],#2


//disthumbcode(0xf1e5); //bl 1/2
//disthumbcode(0xf88c); //bl 2/2
//disthumbcode(0xaf02);

//push-pop test (thumb)
/*disthumbcode(0xb5f0); //push r4-r7,lr
for(i=0;i<16;i++){
	if(i!=0x2 )
		exRegs[i]=0x0;
}
disthumbcode(0xbdf0); //pop {r4-r7,pc}
*/

//disthumbcode(0xb5f0);

//disthumbcode(0xf1e5f88c); //poksapp bl thumb


//disthumbcode(0xf1e5); // bl 1 / 2
//disthumbcode(0xf88c); // bl 2 / 2
//thumb test
			//0x80 lsl r0,r0[8],#2  / 0x880 lsr r0,r0[0x20],#2 / 0x1080 asr r0,r0[0xffffffff], #3 (5.1) hw CPSR OK!
			//0x188b add r3,r1,r2 / 0x1a8b sub r3,r1[0x10],r2[0xc4] / 0x1dcb add r3,r1[0x10],#7 / 0x1fcb sub r3,r1[0x10],#0x7 (5.2) hw CPSR OK!
			//0x23fe mov r3,#0xfe / 0x2901 cmp r1,#0x1 / 0x3120 add r1,#0x20 / 0x3c01 sub r4,#1 = 0xffffffff (5.3) hw CPSR OK!
			//0x400a and r2,r1   /0x404a eor r2,r1 / 0x408a lsl r2,r1 / 0x40da lsr r2,r3 / 0x4111 asr r1,r2 / 0x4151 adc r1,r2 / 0x4191 sbc r1,r2 / 0x41d1 ror r1,r2 / 0x4211 tst r1,r2 / 0x424a neg r2,r1 / 0x4291 cmp r1,r2 / 0x42d1 cmn r1,r2 / 0x4311 orr r2,r2,r1 / 0x4351 mul r1,r1,r2 / 0x4391 bic r1,r1,r2 / 0x43d1 mvn r1,r2  (5.4) hw CPSR OK!
			//0x4478 add r0,r15 / 0x449c add ip(r12),r3 / 0x44f6 add r14,r14 /0x456e cmp rd, hs / 0x459e cmp hd, rs / 0x45f7 cmp pc,lr / 0x4678 mov r1,pc / 0x4687 mov pc,r0  / 0x46f7 mov hd,hs / 0x4708 bx r1 (arm) / 0x4709 (thumb) / 0x4770 bx lr (arm) / 0x4771 bx lr (thumb) / (5.5) hw CPSR OK!. pending branch opcodes because ARM disassembler is missing..!
			//0x4820 ldr r0,[pc,#0x20] (5.6)  hw CPSR OK!.
			//0x5158 str r0,[r3,r5] / 0x5558 strb r0,[r3,r5] / 0x58f8 ldr r0,[r7,r3] / 0x5d58 ldrb r0,[r3,r5]  (5.7) hw CPSR OK!
			//0x5358 strh r0,[r5,r3] / 0x5758 ldrh r0,[r3,r5] / 0x5b58 ldsb r0,[r3,r5] / 0x5f58 ldsh r0,[r3,r5] (5.8) hw CPSR OK!
			//0x6118 str r0,[r3,#0x10]  / 0x6858 ldr r0,[r3,#0x4] / 0x7058 strb r0,[r3,#0x4] /0x7858 ldrb r0,[r3,#0x4] (5.9) OK
			//0x8098 strh r0,[r3,#0x4] / 0x8898 ldrh r0,[r3,#0x8] (5.10) 
			//0x9302 str r3,[sp,#0x8] / 0x9b02 ldr r3,[sp,#0x8] (5.11) OK
			//0xaf02 add r0,[sp,#0x8] / 0xa002 add r0,[pc,#0x8] (5.12) OK
			
			//0xb01a add sp,#imm / 0xb09a subs sp,#imm(5.13) OK
			//b4ff push r0-r7 / 0xb5ff push r0-r7,LR / 0xbcff pop r0-r7 / 0xbdff pop r0-r7,PC OK
			//0xc3ff stmia r3!, {r0-r7} / 0xcfff ldmia rb!,{r0-r7} (5.15)
			
			//(5.16)
			//0xd0ff BEQ label / 0xd1ff BNE label / 0xd2ff BCS label / 0xd3ff BCC label 
			//0xd4ff BMI label / 0xd5ff BPL label / 0xd6ff BVS label / 0xd7ff BVC label
			//0xd8ff BHI label / 0xd9ff BLS label / 0xdaff BGE label / 0xdbff BLT label
			//0xdcff BGT label / 0xddff BLE label
			
			//0xdfff swi #value8 (5.17)
			//0xe7ff b(al) PC-relative area (5.18)
			//0xf7ff 1/2 long branch
			//0xffff 2/2 long branch (5.19)
//////////////////////////////////////////////////////////////////////////////////////////////////
//ARM opcode test

//other conditional
//z_flag=0;
//disarmcode(0x00810002); //addeq r0,r1,r2
//disarmcode(0x10810002); //addne r0,r1,r2
//disarmcode(0x05910000); //ldreq r0,[r1]
//disarmcode(0x05810000); //streq r0,[r1]

//5.2 BRANCH CONDITIONAL
//z_flag=1;
//disarmcode(0x0afffffe); //beq label
//disarmcode(0x1afffffd); //bne label
//disarmcode(0x2afffffc); //bcs label
//disarmcode(0x3afffffb); //bcc label
//disarmcode(0x4afffffa); //bmi label
//disarmcode(0x5afffff9); //bpl label
//disarmcode(0x6afffff8); //bvs label
//disarmcode(0x7afffff7); //bvc label
//disarmcode(0x8afffff6); //bhi label
//disarmcode(0x9afffff5); //bls label
//disarmcode(0xaafffff4); //bge label
//disarmcode(0xbafffff3); //blt label
//disarmcode(0xcafffff2); //bgt label
//disarmcode(0xdafffff1); //ble label
//disarmcode(0xeafffff0); //b label

//5.3 BRANCH WITH LINK
//z_flag=1;
//disarmcode(0x012fff31); //blxeq r1
//disarmcode(0x112fff31); //blxne r1
//disarmcode(0xe12fff31); //blx r1

//disarmcode(0xe12fff11); //bx r1 pokesapp

//5.4
//AND 
//#Imm with rotate
//disarmcode(0xe0000001); //and r0,r0,r1
//disarmcode(0xe20004ff); //and r0,r0,#0xff000000

//shift with either #imm or register
//disarmcode(0xe0011082); //and r1,r1,r2,lsl #0x1
//disarmcode(0xe0011412);  //and r1,r2,lsl r4
//disarmcode(0xe0011a12); //and r1,r2,lsl r10

//disarmcode(0xe00110a2); //and r1,r2,lsr #1
//disarmcode(0xe0011a32);	//and r1,r2,lsr r10
//disarmcode(0xe00110c2);	//and r1,r2,asr #1
//disarmcode(0xe0011a52);	//and r1,r2,asr r10
//disarmcode(0xe00110e2); //and r1,r2,ror #1
//disarmcode(0xe0011a72); //and r1,r2,ror r10

//EOR
//disarmcode(0xe2211001);  //eor r1,#1
//disarmcode(0xe0211002);  //eor r1,r2

//disarmcode(0xe0211082); //eor r1,r2,lsl #1
//disarmcode(0xe0211a12); //eor r1,r2,lsl r10
//disarmcode(0xe02110a2); //eor r1,r2,lsr #1
//disarmcode(0xe0211a32); //eor r1,r2,lsr r10
//disarmcode(0xe02110c2); //eor r1,r2,asr #1
//disarmcode(0xe0211a52); //eor r1,r2,asr r10
//disarmcode(0xe02110e2); //eor r1,r2,ror #1
//disarmcode(0xe0211a72); //eor r1,r2,ror r10

//SUB
//disarmcode(0xe2411001); //sub r1,#1
//disarmcode(0xe0411002); //sub r1,r2
//disarmcode(0xe0411082);	//sub r1,r2,lsl #1
//disarmcode(0xe0411a12); //sub r1,r2,lsl r10
//disarmcode(0xe04110a2); //sub r1,r2,lsr #1
//disarmcode(0xe0411a32); //sub r1,r2,lsr r10
//disarmcode(0xe04110c2); //sub r1,r2,asr #1
//disarmcode(0xe0411a52); //sub r1,r2,asr r10
//disarmcode(0xe04110e2); //sub r1,r2,ror #1
//disarmcode(0xe0411a72); //sub r1,r2,ror sl

//RSB
//disarmcode(0xe2611001); //rsb r1,#1
//disarmcode(0xe0611002); //rsb r1,r2
//disarmcode(0xe0611082); //rsb r1,r2,lsl #1
//disarmcode(0xe0611a12); //rsb r1,r2,lsl r10
//disarmcode(0xe06110a2);	//rsb r1,r2,lsr #1
//disarmcode(0xe0611a32);	//rsb r1,r2,lsr r10
//disarmcode(0xe06110c2); //rsb r1,r2, asr #1
//disarmcode(0xe0611a52); //rsb r1,r2, asr r10
//disarmcode(0xe06110e2);	//rsb r1,r2,ror #1
//disarmcode(0xe0611a72);	//rsb r1,r2,ror r10

//ADD
//disarmcode(0xe2811001); //add r1,#1
//disarmcode(0xe0811002); //add r1,r2
//disarmcode(0xe0811082); //add r1,r2,lsl #1
//disarmcode(0xe0811a12); //add r1,r2,lsl r10
//disarmcode(0xe08110a2); //add r1,r2,lsr #1 
//disarmcode(0xe0811a32); //add r1,r2,lsr r10
//disarmcode(0xe08110c2); //add r1,r2,asr #1
//disarmcode(0xe0811a52); //add r1,r2,asr r10
//disarmcode(0xe08110e2); //add r1,r2,ror #1
//disarmcode(0xe0811a72); //add r1,r2,ror r10
//disarmcode(0xe28f0018);	//add r0,=0x08000240

//disarmcode(0xe5810000); //str r0,[r1]

//ADC
//disarmcode(0xe2a11001); //adc r1,#1
//disarmcode(0xe0a11002); //adc r1,r2
//disarmcode(0xe0a11082); //adc r1,r2,lsl #1
//disarmcode(0xe0a11a12); //adc r1,r2,lsl r10
//disarmcode(0xe0a110a2); //adc r1,r2,lsr #1
//disarmcode(0xe0a11a32); //adc r1,r2,lsr r10
//disarmcode(0xe0a110c2); //adc r1,r2,asr #1
//disarmcode(0xe0a11a52); //adc r1,r2,asr r10
//disarmcode(0xe0a110e2); //adc r1,r2,ror #1
//disarmcode(0xe0a11a72); //adc r1,r2,ror r10

//SBC
//disarmcode(0xe2c11001); //sbc r1,#1
//disarmcode(0xe0c11002); //sbc r1,r2
//disarmcode(0xe0c11082); //sbc r1,r2,lsl #1
//disarmcode(0xe0c11a12); //sbc r1,r2,lsl r10
//disarmcode(0xe0c110a2); //sbc r1,r2,lsr #1
//disarmcode(0xe0c11a32); //sbc r1,r2,lsr r10
//disarmcode(0xe0c110c2); //sbc r1,r2,asr #1
//disarmcode(0xe0c11a52); //sbc r1,r2,asr r10
//disarmcode(0xe0c110e2); //sbc r1,r2,ror #1
//disarmcode(0xe0c11a72); //sbc r1,r2,ror r10

//RSC
//disarmcode(0xe2e11001); //rsc r1,#1
//disarmcode(0xe0e11002); //rsc r1,r2
//disarmcode(0xe0e11082); //rsc r1,r2,lsl #1
//disarmcode(0xe0e11a12); //rsc r1,r2,lsl r10
//disarmcode(0xe0e110a2); //rsc r1,r2,lsr #1
//disarmcode(0xe0e11a32); //rsc r1,r2,lsr r10
//disarmcode(0xe0e110c2); //rsc r1,r2,asr #1
//disarmcode(0xe0e11a52); //rsc r1,r2,asr r10
//disarmcode(0xe0e110e2); //rsc r1,r2,ror #1
//disarmcode(0xe0e11a72); //rsc r1,r2,ror r10

//TST
//disarmcode(0xe3110001); //tst r1,#1
//disarmcode(0xe1110002); //tst r1,r2
//disarmcode(0xe1110082); //tst r1,r2,lsl #1
//disarmcode(0xe1110a12); //tst r1,r2,lsl r10
//disarmcode(0xe11100a2); //tst r1,r2,lsr #1
//disarmcode(0xe1110a32); //tst r1,r2,lsr r10
//disarmcode(0xe11100c2); //tst r1,r2,asr #1
//disarmcode(0xe1110a52); //tst r1,r2,asr r10
//disarmcode(0xe11100e2); //tst r1,r2,ror #1
//disarmcode(0xe1110a72); //tst r1,r2,ror r10

//TEQ
//disarmcode(0xe3310001); //teq r1,#1
//disarmcode(0xe1310002); //teq r1,r2
//disarmcode(0xe1310082); //teq r1,r2,lsl #1
//disarmcode(0xe1310a12); //teq r1,r2, lsl r10
//disarmcode(0xe13100a2); //teq r1,r2,lsr #1
//disarmcode(0xe1310a32); //teq r1,r2,lsr r10
//disarmcode(0xe13100c2); //teq r1,r2,asr #1
//disarmcode(0xe1310a52); //teq r1,r2,asr r10
//disarmcode(0xe13100e2); //teq r1,r2,ror #1
//disarmcode(0xe1310a72); //teq r1,r2,ror r10

//CMP
//disarmcode(0xe3510001); //cmp r1,#1
//disarmcode(0xe1510002); //cmp r1,r2
//disarmcode(0xe1510082); //cmp r1,r2, lsl #1
//disarmcode(0xe1510a12); //cmp r1,r2,lsl r10
//disarmcode(0xe15100a2); //cmp r1,r2, lsr #1
//disarmcode(0xe1510a32); //cmp r1,r2, lsr r10
//disarmcode(0xe15100c2); //cmp r1,r2, asr #1
//disarmcode(0xe1510a52); //cmp r1,r2,asr r10
//disarmcode(0xe15100e2); //cmp r1,r2,ror #1
//disarmcode(0xe1510a72); //cmp r1,r2,ror r10

//CMN
//disarmcode(0xe3710001); //cmn r1,#1
//disarmcode(0xe1710002); //cmn r1,r2
//disarmcode(0xe1710082); //cmn r1,r2,lsl #1
//disarmcode(0xe1710a12); //cmn r1,r2, lsl r10
//disarmcode(0xe17100a2); //cmn r1,r2, lsr #1
//disarmcode(0xe1720a32); //cmn r1,r2,lsr r10
//disarmcode(0xe17100c2); //cmn r1,r2,asr #1
//disarmcode(0xe1710a52); //cmn r1,r2, asr r10
//disarmcode(0xe17100e2); //cmn r1,r2, ror #1
//disarmcode(0xe1710a72); //cmn r1,r2, ror r10

//ORR
//disarmcode(0xe3811001); //orr r1,#1
//disarmcode(0xe1811002); //orr r1,r2 
//disarmcode(0xe1811082); //orr r1,r2,lsl #1
//disarmcode(0xe1811a12); //orr r1,r2,lsl r10
//disarmcode(0xe18110a2); //orr r1,r2,lsr #1
//disarmcode(0xe1811a32); //orr r1,r2,lsr r10
//disarmcode(0xe18110c2); //orr r1,r2,asr #1
//disarmcode(0xe1811a52); //orr r1,r2,asr r10
//disarmcode(0xe18110e2); //orr r1,r2,ror #1
//disarmcode(0xe1811a72); //orr r1,r2,ror r10

//MOV
//disarmcode(0xe3a01001); //mov r1,#1
//disarmcode(0xe1a01002); //mov r1,r2
//disarmcode(0xe1b01082); //movs r1,r1,lsl #1
//disarmcode(0xe1b01a12); //movs r1,r1,lsl r10
//disarmcode(0xe1b010a2); //movs r1,r1,lsr #1
//disarmcode(0xe1b01a32); //movs r1,r1,lsr r10
//disarmcode(0xe1b010c2); //movs r1,r1,asr #1
//disarmcode(0xe1b01a52); //movs r1,r1,asr r10
//disarmcode(0xe1b010e2); //movs r1,r1,ror #1
//disarmcode(0xe1b01a72); //movs r1,r1,ror r10
//disarmcode(0xe1a0e00f); //mov r14,r15 (poke sapp)


//MVN
//disarmcode(0xe3e01001); //mvn r1,#1
//disarmcode(0xe1e01002); //mvn r1,r2
//disarmcode(0xe1e01082); //mvn r1,r2,lsl #1
//disarmcode(0xe1e01a12); //mvn r1,r2,lsl r10
//disarmcode(0xe1e010a2); //mvn r1,r2, lsr #1
//disarmcode(0xe1e01a32); //mvn r1,r2, lsr r10
//disarmcode(0xe1e010c2); //mvn r1,r2, asr #1
//disarmcode(0xe1e01a52); //mvn r1,r2, asr r10
//disarmcode(0xe1e010e2); //mvn r1,r2, ror #1
//disarmcode(0xe1e01a72); //mvn r1,r2,ror r10


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//disarmcode(0xe19000f1); 		//ldrsh r0,[r0,r1]
//disarmcode(0xe59f2064);		//ldr r2,[pc, #100]
//disarmcode(0xe3a01000);		//mov r1,#0
//disarmcode(0xe3b01000);		//bad opcode mov r1,#0 with S bit set

//disarmcode(0xe1b00801);	//lsl r0,r1,#0x10
//disarmcode(e1b00211);	//lsl r0,r1,r2

//disarmcode(0xe24000ff);		//sub r0,r0,#0xff
//disarmcode(0xe0400001);	//sub r0,r0,r1
//disarmcode(0xe27000ff); //rsbs r0,#0xff
//disarmcode(0xe0700001); //rsbs r0,r1
//disarmcode(0xe28004ff);		//add r0,#0xff000000
//disarmcode(0xe0800001); //add r0,r1
//disarmcode(0xe2a00333); //adc r0,#0xcc000000
//disarmcode(0xe0a00001);	//adc r0,r1
//disarmcode(0xe2d004dd);	//sbc r0,#0xdd000000
//disarmcode(0xe0d00001);	//sbc r0,r1
//disarmcode(0xe2e004dd);	//rsc r0,#0xdd000000
//disarmcode(0xe0e00001);	//rsc r0,r1
//disarmcode(0xe31004dd);	//tst r0,#0xdd000000
//disarmcode(0xe1100001);	//tst r0,r1
//disarmcode(0xe33004bb); //teq r0,#0xbb000000
//disarmcode(0xe1300001);	//teq r0,r1
//disarmcode(0xe35004aa);	//cmp r0,0xaa000000
//disarmcode(0xe1500001);	//cmp r0,r1
//disarmcode(0xe37004aa); 	//cmn r0,#0xaa000000
//disarmcode(0xe1700001);		//cmn r0,r1
//disarmcode(0xe38004aa); //orr r0,#0xaa000000
//disarmcode(0xe1800001); //orr r0,r1
//disarmcode(0xe3a004aa); //mov r0,#0xaa000000
//disarmcode(0xe1a00001); //mov r0,r1
//disarmcode(0xe3c004aa); //bic r0,#0xaa000000
//disarmcode(0xe1c00001); //bic r0,r1
//disarmcode(0xe3e004aa); //mvn r0,#0xaa000000
//disarmcode(0xe1e00001); //mvn r0,r1
//disarmcode(0xe1b00801); //lsl r0,r1,#0x10
//disarmcode(0xe1b00211); //lsl r0,r1,r2

//cpsr test
//disarmcode(0xe10f0000); //mrs r0,CPSR
//cpsrvirt=0;
//disarmcode(0xe129f000); //msr CPSR_fc, r0

//spsr test
//disarmcode(0xe14f0000); //mrs r0,spsr
//spsr_last=0;
//disarmcode(0xe169f000); //msr spsr,r0

//5.5
//CPSR
//disarmcode(0xe10f0000); //mrs r0,CPSR
//disarmcode(0xe129f000); //msr CPSR_fc, r0
//disarmcode(0xe329f20f); //msr CPSR_fc, #0xf0000000
//disarmcode(0xf328f004); //msr cpsr,#imm transfer to psr
//disarmcode(0xf128f00f); //msr cpsr,pc transfer to psr
//disarmcode(0xe129f000); //pokesapp

//SPSR
//disarmcode(0xe14f0000); //mrs r0,spsr
//disarmcode(0xe169f000); //msr spsr,r0
//disarmcode(0xe369f20f); //msr spsr,#0xf0000000
//disarmcode(0xf368f004); //msr spsr,#imm transfer to psr
//disarmcode(0xf168f00f); //msr spsr,pc transfer to psr

//5.6 MUL
//disarmcode(0xe00a0592); 	//mul r10,r2,r5 (no cpsr flags)
//disarmcode(0xe01a0592);		//mul r10,r2,r5 (cpsr enabled flag)

//disarmcode(0xe02a5192); //mla r10,r2,r1,r5 (rd = (rs*rn)+rm)
//untested but unused?
//disarmcode(0xe0100292); //muls r0,r2,r2
//disarmcode(0xb0020293); //mullt r2,r3,r2
//disarmcode(0x70388396); //mlasvc r8,r6,r3,r8
//disarmcode(0xe04a0592); //purposely broken mul r10,r2,r5

//5.7 STR/LDR
//disarmcode(0xe7b21004); //ldr r1,[r2,r4]!
//disarmcode(0xe7a21004); //str r1,[r2,r4]!
//disarmcode(0xe59f1150);	//ldr r1,=0x3007ffc (pokémon sapp compiler) (pc+0x150)
//disarmcode(0xe59f1148);	//ldr r1,=0x8000381

//disarmcode(0xe59f103c);	//ldr r1,=0x3007ffc (own compiler)


//disarmcode(0xe6921004); //ldr r1,[r2],r4
//disarmcode(0xe5821010); //str r1,[r2,#0x10]
//disarmcode(0xe7821103); //str r1,[r2,r3,lsl #2]
//disarmcode(0x05861005); //streq r1,[r6,#5]

//disarmcode(0xe6821004); //ldr r1,[r2,#0x10]
//disarmcode(0xe7921103); //ldr r1,[r2,r3,lsl #2]
//disarmcode(0x05961005); //ldreq r1,[r6,#0x5]
//disarmcode(0xe59f5040); //ldr r5,[pc, #0x40]

//5.8 LDM/STM

 //test ldm/stm 5.8 (requires (u32)&gbabios[0] on r0 first)
/*
disarmcode(0xe880ffff); //stm r0,{r0-r15}

//clean registers
for(i=0;i<16;i++){
	if(i!=0x0 )
		exRegs[i]=0x0;

}
exRegs[0x0]=(u32)&gbabios[0];
rom=0;
disarmcode(0xe890ffff); //ldm r0,{r0-r15}
*/

//5.8 ldm/stm with PC

//disarmcode(0xe92d8000);//stm r13!,{pc}
//rom=0x0;
//disarmcode(0xe97d8000);	//ldm sp!,{pc}^ //pre-indexed (sub)


//without PSR
//disarmcode(0xe880ffff); //stm r0,{r0-r15}
//disarmcode(0xe890ffff); //ldm r0,{r0-r15}

//with PSR
//disarmcode(0xe880ffff); //stm r0,{r0-r15}
//disarmcode(0xe8d0ffff); //ldm r0,{r0-r15}^

//stack test
/*
disarmcode(0xe92d0007);		//push {r0,r1,r2} (writeback enabled and post-bit enabled / add offset base)
disarmcode(0xe93d0007);		//pop {r0,r1,r2} (writeback enabled and pre-bit enabled // subs offset bast)
*/
//disarmcode(0xe8bd8000);	//pop {pc}

//disarmcode(0xe92d8000);//stm r13!,{pc}

//disarmcode(0xe8fd8000);	//ldm sp!,{pc}^ //post-indexed
//disarmcode(0xe9fd8000);	//ldm sp!,{pc}^ //pre-indexed (add)
//	disarmcode(0xe97d8000);		//ldm sp!,{pc}^ //pre-indexed (sub)

//disarmcode(0xe96d7fff);	//stmdb sp!,{r0-r14}^
//disarmcode(0xe82d400f);	//stmda sp!,{r0-r3,lr}

//disarmcode(0xe59f5048);	//ldr r5,[pc,#72]
//disarmcode(0xe8956013);	//ldm r5,{r0,r1,r4,sp,lr}
//disarmcode(0xe9bd800f);	//ldmib sp!,{r0,r1,r2,r3,pc}

//5.9 SWP
//disarmcode(0xe1020091); 	//swp r0,r1,[r2]
//disarmcode(0xe1420091);		//swpb r0,r1,[r2]
//disarmcode(0x01010090);	//swpeq r0,r0,[r1]

//5.10 swi / svc call
//z_flag=1;
//c_flag=0;
//disarmcode(0xef000010);	    //swi 0x10

//5.15 undefined
//disarmcode(0xe755553e);

			//other things!
			//iprintf("\x1b[0;0H sbrk(0) is at: %x ",(u32)sbrk(0));
			//debugDump();
			
			//psg noise test iprintf("pitch: %x \n",(u32)((*(buffer_input+0)>>16)&0xffff));iprintf("pitch2: %x \n",(u32)((*(buffer_input+0))&0xffff));iprintf("                         \n");
			//iprintf("timer:%d status:%x \n",0,(u32)TMXCNT_LR(0));

			//gbastack test u8
			/*
			for(i=0;i<8;i++){
				stru8asm((u32)&gbastack,i,i); //gbastack[i] = 0xc070;				
				//iprintf("%x \n",(u32)gbastack[i]); //iprintf("%x \n",(u32)**(&gbastack+i*2)); //non contiguous memory  //iprintf("%x \n",(u32)ldru16asm((u32)&gbastack,i*2));

				if(ldru8asm((u32)&gbastack,i)!=i){
					iprintf("failed writing @ %x \n",(u32)&gbastack+(i));
					while(1);
				}
	
				iprintf("%x ",(u32)ldru8asm((u32)&gbastack,i));
	
				stru8asm((u32)&gbastack,i,0x0);
			}
			*/

			//load store test
			//disthumbcode(0x683c);
			
			//iprintf("squareroot:%d",(int)sqrtasm(10000)); //%e double :p
			}
			
			
			if(0 == (REG_KEYINPUT & (KEY_A))){
			//need to fed timerX value to timer, will react on overflow and if timerX is started through cnt
			//TMXCNT_LW(u8 TMXCNT,int TVAL)
			//u16 TMXCNT_HW(u8 TMXCNT, u8 prescaler,u8 countup,u8 status) 
				//TMXCNT_LW(0,300);
				//TMXCNT_HW(0, 0,0,1);
			
			//swi call vector
			//swicaller(0x0); #number
			
			//debug stack (hardware chunk BIG stack for each CPU mode)
			/*
			//you must detect stack mode: for gbastackptr
			
			iprintf("\n gbastack @ %x",(u32)(u32*)gbastackptr);
			iprintf("\n //////contents//////: \n");
			
			for(i=0;i<16;i+=4){
			iprintf(" %d:[%x] ",i,(u32)ldru32asm((u32)&gbastack,i));
			//iprintf("%x \n",(u32)gbastack[i]);
			//iprintf("%x \n",(u32)gbastack[i+1]);
			
			if (i==9) iprintf("\n");
			
			}
			*/
			
			/* //tempbuffer1 
			iprintf("\n    tempbuffer @ %x",(u32)&tempbuffer);
			iprintf("\n ///////////contents/////////: \n");
			
			for(i=0;i<24;i+=4){
			iprintf(" %d[%x] ",i,(u32)ldru32asm((u32)tempbuffer,i));
			
			if (i==12) iprintf("\n");
			
			}
			*/
			
			// 
			//iprintf("\n /// GBABIOS @ %x //",(unsigned int)(u8*)gba.bios);
			
			/*
			for(i=0;i<16;i++){
			iprintf(" %d:[%x] ",i,(unsigned int)*((u32*)gba.bios+i));//ldru32asm((u32)tempbuffer2,i));
			
				if (i==15) iprintf("\n");
			
			}
			*/
			
			int cntr=0;
			
			/*
			iprintf("rom contents @ 0x08000000 \n");
			for(cntr=0;cntr<4;cntr++){
			iprintf(" %d:[%x] ",cntr,(unsigned int)CPUReadMemory(0x08000000+(cntr*4)));//ldru32asm((u32)tempbuffer2,i));
			
				if (cntr==15) iprintf("\n");
			
			}
			*/
			iprintf("\n hardware r0-r15 stack (GBA) [MODE");
			
			if ((cpsrvirt&PSR_MODE) == (PSR_USR) || (cpsrvirt&PSR_MODE) == (PSR_SYS))
				iprintf(" USR/SYS STACK]");
			else if ((cpsrvirt&PSR_MODE)==(PSR_FIQ))
				iprintf(" FIQ STACK]");
			else if ((cpsrvirt&PSR_MODE)==(PSR_IRQ))
				iprintf(" IRQ STACK]");
			else if ((cpsrvirt&PSR_MODE)==(PSR_SVC))
				iprintf(" SVC STACK]");
			else if ((cpsrvirt&PSR_MODE)==(PSR_ABT))
				iprintf(" ABT STACK]");
			else if ((cpsrvirt&PSR_MODE)==(PSR_UND))
				iprintf(" UND STACK]");
			else
				iprintf(" STACK LOAD ERROR] CPSR: %x -> psr:%x",(unsigned int)cpsrvirt,(unsigned int)(cpsrvirt&PSR_MODE));
			iprintf("\n //////contents//////: \n");
			
			for(cntr=0;cntr<16;cntr++){
				//iprintf(" r%d :[%x] ",cntr,(unsigned int)ldru32asm((u32)&exRegs,cntr*4)); //byteswap reads
				if (cntr!=0xf) 
				iprintf(" r%d :[0x%x] ",cntr, (unsigned int)exRegs[cntr]);
				else iprintf(" PC(0x%x)",  (unsigned int)exRegs[0xf]); //works: (u32)&rom  // &gbavirtreg_usr[0xf] OK
			}
			
			iprintf("\n CPSR[%x] / SPSR:[%x] \n",(unsigned int)cpsrvirt,(unsigned int)spsr_last);
			iprintf("CPUvirtrunning:");
			if (armState==true) iprintf("[ARM]");
			else iprintf("[THMB]");
			iprintf("/ CPUmode:");
			//coto
			iprintf("\n shared CPU ticks:(%d) / gba.lcdticks:(%x) /  soft_vcount(cpuread): (%d)",
			/*(int)cputotalticks*/(int)SHARED_IPC->inst_globalemutick[0]->gbacore_emuticker,(int)lcdTicks,(int)CPUReadByte(0x04000006));
			
			iprintf("\n DMAEnbl: 0:(%d) | 1:(%d) | 2:(%d) | 3:(%d)",
			(int)((DM0CNT_H>>15)&0xf),(int)((DM1CNT_H>>15)&0xf),
			(int)((DM2CNT_H>>15)&0xf),(int)((DM3CNT_H>>15)&0xf)
			);
            
			iprintf("DAY:%d ",(int)gba_get_dayrtc());
			iprintf("MONTH:%d ",(int)gba_get_monthrtc());
			iprintf("YEAR:%d \n",(int)libnds_get_yearrtc(libnds_get_timeinst()));
			
			iprintf("HH:%d ",(int)gba_get_hourrtc());
			iprintf("MM:%d ",(int)gba_get_minrtc());
			iprintf("SS:%d \n",(int)gba_get_secrtc());
			
		/*
		int hours, seconds, minutes, day, month, year;
		
		time_t unixTime = time(NULL);
		struct tm* timeStruct = gmtime((const time_t *)&unixTime);

		hours = timeStruct->tm_hour;
		minutes = timeStruct->tm_min;
		seconds = timeStruct->tm_sec;
		day = timeStruct->tm_mday;
		month = timeStruct->tm_mon;
		year = timeStruct->tm_year +1900;

		printf("\x1b[2J%02i:%02i:%02i", hours, minutes, seconds);
		*/
		
		/*
		time_t time_inst = time(NULL);
		
		struct tm* tstr = gmtime((const time_t *)&time_inst);
		printf("%d:%d:%d",  tstr->tm_hour, tstr->tm_min, tstr->tm_sec);
		*/
		
		/*
		time_t time_inst = time(NULL);
		
		struct tm* tstr = gmtime((const time_t *)&time_inst);
		*/
		
		//printf("y:%d-m:%d-d:%d",  (int)get_yearrtc(), (int)get_monthrtc(), (int)get_dayrtc());
		
		
		/*
		volatile u16 datearr[6];
		get_datetime(datearr);
		
		printf("y:%d-m:%d-d:%d",  (int)datearr[0], (int)datearr[1], (int)datearr[2]);
		printf("h:%d-m:%d-s:%d",  (int)datearr[3], (int)datearr[4], (int)datearr[5]);
		*/
			} //a button
			
			
			if(0 == (REG_KEYINPUT & (KEY_B))){
			//iprintf("hi");
			
			//for testing stmiavirt
			/*
			for(i=0;i<16;i++){
			
				iprintf(" [%c] ",(unsigned int)*((u32*)&exRegs+i));
				if (i==15) iprintf("\n"); //sizeof(tempbuffer[0]) == 1 byte (u8)
			}
			*/
			
			//for testing tempbuffer
			/*
			for(i=0;i<16;i++){
			
			stru32asm( (u32)&tempbuffer, i*(4/(sizeof(tempbuffer[0]))) ,0xc0700000+i);
			
			//stru32asm((u32)&exRegs, i*4,ldru32asm((u32)&tempbuffer,i*4));
			
			}
			
			//clean tempbuffer
			for(i=0;i<64;i++){
			tempbuffer[i]=0;
			}
			
			ldmiavirt((u8*)&tempbuffer, (u32)&exRegs, 0xffff, 32, 1); //read using normal reading method
			
			for(i=0;i<16;i++){
				iprintf(" [%x] ",(unsigned int)ldru32asm((u32)&tempbuffer,i*4));
				
				if (i==15) iprintf("\n"); //sizeof(tempbuffer[0]) == 1 byte (u8)
			}
			*/
			
			//ldmiavirt((u8*)&tempbuffer, (u32)(u32*)gbastackptr, 0xf, 32, 0); //read using byteswap (recomended for stack management)
			
			//for two arguments
			//gbachunk=(u32)(0x08000000+ (rand() % romsize));
			//iprintf("\n gbarom(%x):[%x] ",(unsigned int)gbachunk,(unsigned int)ldru32inlasm((int)gbachunk));
			
			//for zero arguments, just arg passing
			//gbachunk=ldru32inlasm((int)(0x08000000+ ( (rand() % 0x0204)& 0xfffe )));
			//iprintf("romread:[%x] \n",(unsigned int)gbachunk);
			
			/*
			TMXCNT_HW(0, 0,0,0);
			iprintf("dtcm contents: \n");
			for(i=0;i<64;i++){
				i2=0x027C0000;
				i2+=i;
				iprintf("%08x ",(unsigned int)ldru32inlasm(i2));
			}
			iprintf("\n");
			*/
			
			//psg noise testiprintf("pitch2: %d \n",(*(buffer_input+0))&0xffff);temp5--;
		
		
		//load store test
		//disthumbcode(0x692a); //ldr r2,[r5,#0x10]
		//disthumbcode(0x603a); //str r2,[r7,#0]
		
			//tested
			//0x008b lsl rd,rs,#Imm (5.1)
			//0x1841 adds rd,rs,rn / 0x1a41 subs rd,rs,rn / #Imm (5.2)
			//0x3c01 subs r4,#1 (5.3) 
			//0x40da lsrs rd,rs (5.4)
			//0x4770 bx lr / 0x449c add ip,r3 (5.5)
			//0x4e23 LDR RD,[PC,#Imm] (5.6)
			//0x5158 str r0,[r3,r5] / 0x58f8 ldr r0,[r7,r3] (5.7)
			//0x5e94 ldsh r0,[r3,r4] (5.8)
			//0x69aa ldr r2,[r5,#24] / 0x601a str r2,[r3,#0] (5.9)
			//0x87DB ldrh r3,[r2,#3] // 0x87DB strh r3,[r2,#3] (5.10)
			//0x97FF str rd,[sp,#Imm] / 0x9FFF ldr rd,[sp,#Imm] (5.11)
			//0xa2e1 rd,[pc,#Imm] / 0xaae1 add rd,[sp,#Imm] (5.12)
			//0xb01a add sp,#imm / 0xb09a add sp,#-imm(5.13)
			//0xb4ff push {rlist} / 0xb5ff push {Rlist,LR} / 0xbcff pop {Rlist} / 0xbdff pop {Rlist,PC}
			//0xc7ff stmia rb!,{rlist} / 0xcfff ldmia rb!,{rlist} (5.15)
			
			//(5.16)
			//0xd0ff BEQ label / 0xd1ff BNE label / 0xd2ff BCS label / 0xd3ff BCC label 
			//0xd4ff BMI label / 0xd5ff BPL label / 0xd6ff BVS label / 0xd7ff BVC label
			//0xd8ff BHI label / 0xd9ff BLS label / 0xdaff BGE label / 0xdbff BLT label
			//0xdcff BGT label / 0xddff BLE label
			
			//0xdfff swi #value8 (5.17)
			//0xe7ff b(al) PC-relative area (5.18)
			//0xf7ff 1/2 long branch
			//0xffff 2/2 long branch (5.19)
			
			//stack for virtual environments
			/*
			iprintf("\n 	branchstack[%x]:[%x]",(unsigned int)&branch_stack[0],(unsigned int)(u32*)(branch_stackfp));
			iprintf("\n ///////////contents/////////: \n");
			
			for(i=0;i<16;i++){
			iprintf(" %d:[%x] ",i,(unsigned int)ldru32asm((u32)(u32*)(branch_stackfp),i*4));
			
			if (i==12) iprintf("\n");
			
			}
			*/
			
			}
			
			if(0 == (REG_KEYINPUT & (KEY_X))){
			}
			
			if(0 == (REG_KEYINPUT & (KEY_UP))){
				/*
				printf("\n");
				iprintf("readu32:(%d) \n",i2);
				iprintf("%08x ",(unsigned int)ichfly_readu32(i2));
				i2*=4;
				*/
			
			//rom=(u32)(0x08000000+ (rand() %  (romsize-0x11))); //causes undefined opcode if romsize unset ( overflow )
			
			//load store test
			//disthumbcode(0x6932); //ldr r2,[r6,#0x10]
		
			iprintf("\n");	
			
			//addspvirt((u32)(u32*)gbastackptr,0x4);
			
			//psg noise test iprintf("volume: %d\n",*(buffer_input+1));temp4++;
			//sbrk(0x800);
			}
			
			if(0 == (REG_KEYINPUT & (KEY_DOWN))){

			//for(i=0;i<255;i++){
			//*(gbastackptr+i)=0;
			//}
			
			/*
			exRegs[0xf]=rom=(u32)rom_entrypoint;
			
			//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
			updatecpuflags(1,cpsrvirt,0x12);
			
			//Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
			updatecpuflags(1,cpsrvirt,0x10);
			*/
			
			//cpu read/writes test
			/* //ok
			iprintf("writing 0xff @ 0x03007dfd \n");cpuwrite_byte(0x03007dfd,0xff);
			iprintf("read @ 0x03007dfd:[%x] \n",(unsigned int)cpuread_byte(0x03007dfd));
			*/
			
			/*
			iprintf("writing 0xc070 @ 0x03007dfd \n");cpuwrite_hword(0x03007dfd,0xc070);
			iprintf("read @ 0x03007dfd:[%x] \n",(unsigned int)CPUReadHalfWord(0x03007dfd));
			*/
			
            
			//iprintf("writing 0xc070c171 @ 0x03007dfd \n");CPUWriteMemory(0x03007dfd,0xc070c171);
			//iprintf("read @ 0x03007dfd:[%x] \n",(unsigned int)CPUReadMemory(0x03007dfd));
			
            //u32 read_val = (u32)0x08000000 + (u32)(rand()&0xffff);
            //iprintf("read @ %x:[%x] \n",read_val,(unsigned int)CPUReadByte(read_val));  //CPUReadHalfWord / CPUReadMemory  / CPUReadByte ok
			//iprintf("read @ %x:[%x] \n",(unsigned int)read_val,(unsigned int)cache_read32(read_val&0x1ffffff)); //works OK
			
            
			//set CPU <mode>
			//updatecpuflags(1,0x0,0x10);
			
			//load store test
			//tempbuffer[0]=0x0;
			//tempbuffer[1]=0x00000000;
			//tempbuffer[2]=(u32)&tempbuffer[2]; //address will be rewritten with gbaread / r2 value
			//stmiavirt( ((u8*)&tempbuffer[0]), (u32)exRegs[0], 0xe0, 32, 1);
	
			
			/* VBAM stats
			iprintf("\n *********************");
			iprintf("\n emu stats: ");
			iprintf("\n N_FLAG(%x) C_FLAG(%x) Z_FLAG(%x) V_FLAG(%x) \n",(unsigned int)gba.N_FLAG,(unsigned int)gba.C_FLAG,(unsigned int)gba.Z_FLAG,(unsigned int)gba.V_FLAG);
			iprintf("\n armstate(%x) frameskip(%x)  \n",(unsigned int)gba.armState,(unsigned int)gba.frameSkip);
			iprintf("\n DISPCNT(%x) DISPSTAT(%x) VCOUNT(%x)",(unsigned int)gba.DISPCNT,(unsigned int)gba.DISPSTAT,(unsigned int)gba.VCOUNT);
			iprintf("\n IE(%lu) IF(%x) IME(%x) EMULATING:(%x)",(unsigned int)gba.IE,(unsigned int)gba.IF,(unsigned int)gba.IME,(unsigned int)gba.emulating);
			iprintf("\n clockTicks(%x) cpuTotalTicks(%x) LCDTicks(%x)",(unsigned int)gba.clockTicks,(unsigned int)gba.cpuTotalTicks,(unsigned int)gba.lcdTicks);
			iprintf("\n frameCount(%x) count(%x) romSize(%x)",(unsigned int)gba.frameCount,(unsigned int)gba.count,(unsigned int)gba.romSize);
			*/
			
			//sbrk(-0x800);
			//psg noise test iprintf("volume: %d\n",*(buffer_input+1));temp4--;
			
			/*
			printf("\n");
				iprintf("readu32:(%d) \n",(i2*4));
				iprintf("%08x ",(unsigned int)ichfly_readu32(i2*4));
				i2/=4;
			*/
			//subspvirt((u32)(u32*)gbastackptr,0x4);
			
			}
				
			if(0 == (REG_KEYINPUT & (KEY_LEFT))){
			
			//printf("words found! (%d) \n",extract_word((int*)minigsf,"fade",55,output)); //return 0 if invalid, return n if words copied to *buf
			//for(i=0;i<10;i++) printf("%x ",output[i]);
			//TMXCNT_L(1,0xF0F0F0F0);
			//TMXCNT_M(timernum,div,cntup,enable,start)
			//TMXCNT_H(1,1,0,1,1);
			//rs_sappy_bin_size
			
			/* //fifo test 
			recvfifo(buffer_input,FIFO_SEMAPHORE_FLAGS);
			i=buffer_input[15];
			i+=0x10;
			if(i<0) i=0;
			else if (i>0x100000) i=0x100000;
			
			iprintf("\n value: %x",(unsigned int)i);
			
			buffer_output[0]=i;
			sendfifo(buffer_output);
			*/
			
			//branch_stackfp=cpubackupmode((u32*)branch_stackfp,exRegs,cpsrvirt);
			//iprintf("branch fp :%x \n",(unsigned int)(u32*)branch_stackfp);
			
			//psg noise test iprintf("pitch: %d\n",(*(buffer_input+0)>>16)&0xffff);temp3++;
			
			}

			if(0 == (REG_KEYINPUT & (KEY_RIGHT))){
			
			/* //fifo test 
			
			recvfifo(buffer_input,FIFO_SEMAPHORE_FLAGS);
			i=buffer_input[15];
			i-=0x10;
			if(i<0) i=0;
			else if (i>0x100000) i=0x100000;
			
			iprintf("\n value: %x",(unsigned int)i);
			
			buffer_output[0]=i;
			sendfifo(buffer_output);
			*/
			
			//branch_stackfp=cpurestoremode((u32*)branch_stackfp,&exRegs[0]);
			//iprintf("branch fp :%x \n",(unsigned int)(u32*)branch_stackfp);

			//psg noise test iprintf("pitch: %d\n",(*(buffer_input+0)>>16)&0xffff);temp3--;
			}
			
		
			if(0 == (REG_KEYINPUT & (KEY_SELECT))){ //ZERO
			
			//disthumbcode(0xb081); //add sp,#-0x4
			
			//swi GBAREAD test OK / BROKEN with libnds swi code
			/*
			gbachunk=(u32)(0x08000000+(rand() % 0xfffffe));
			iprintf("\n acs: (%x)",(unsigned int)gbachunk);
			swicaller(gbachunk);
			iprintf("=> (%x)",(unsigned int)gbachunk);
			*/
			
			//stress test (load/store through assembly inline)
			//for ewram 0x02000000 access
			//CPUWriteHalfWord(&gba, (u32) ( 0x02000000 + (rand() % 0x3FFFe) ) , (u16) rand() % 0xffff ); //OK
			
			//for Iram 03000000-03007FFF access
			//CPUWriteHalfWord(&gba, (u32) ( 0x03000000 + (rand() % 0x7FFE) ) , (u16) rand() % 0xffff ); //OK
			
			//for PaletteRAM 05000000-050003FF access
			//CPUWriteHalfWord(&gba, (u32) ( 0x05000000 + ((rand() % 0x3FE) & 0x3ff) ) , (u16) rand() % 0xffff ); //OK
			
			//for VRAM 06000000-06017FFF
			//CPUWriteHalfWord(&gba, (u32) ( 0x06000000 + ((rand() % 0x17FFF) & 0x17FFF) ) , (u16) rand() % 0xffff ); //OK
			
			//for OAM 07000000-070003FF
			//CPUWriteHalfWord(&gba, (u32) ( 0x07000000 + (rand() % 0x3FF) ) , (u16) rand() % 0xffff ); 
			
			//data abort GBAREAD test OK
			
			//rom
			//iprintf("rom pointer (%x)",(unsigned int)(u8*)rom);
			//entrypoint
			//iprintf("\n rom entrypoint: %x",(unsigned int)(u32*)rom_entrypoint);
			
			//gbaread latest
			//gbachunk=ldru32inlasm(0x08000000+ (rand() % romsize));
			//iprintf("romread:[%x] \n",(unsigned int)gbachunk);
			
			
			//stack cpu backup / restore test
			/*
			u32 * branchfpbu;
			
			branchfpbu=branch_stackfp;
			
			branchfpbu=cpubackupmode((u32*)(branchfpbu),&exRegs[0],cpsrvirt);
			
			stmiavirt((u8*)&cpsrvirt, (u32)(u32*)branchfpbu, 1 << 0x0, 32, 1);
			
			branchfpbu=(u32*)addspvirt((u32)(u32*)branchfpbu,1);
			
			iprintf("old cpsr: %x",(unsigned int)cpsrvirt);
			
			//flush workreg
			cpsrvirt=0;
			for(i=0;i<0x10;i++){
				*((u32*)exRegs[0]+i)=0x0;
			}
			
			
			branchfpbu=(u32*)subspvirt((u32)(u32*)branchfpbu,1);
			
			ldmiavirt((u8*)&cpsrvirt, (u32)(u32*)branchfpbu, 1 << 0x0, 32, 1);
			
			iprintf("restored cpsr: %x",(unsigned int)cpsrvirt);
			
			branchfpbu=cpurestoremode((u32*)(branchfpbu),&exRegs[0]);
			
			updatecpuflags(1,cpsrvirt,cpsrvirt&0x1f);
			*/
			
			
			//branch stack test
			//branch_stackfp=branch_test(branch_stackfp);
			//iprintf("\n branch fp ofs:%x",(unsigned int)(u32*)branch_stackfp);
			
			//iprintf("size branchtable: 0x%d",(int)gba_branch_table_size); //68 bytes each
			
			//iprintf("bytes per branchblock: 0x%d",(int)gba_branch_block_size); //68 bytes each
			
			//stmiavirt((u8*)&tempbuffer, (u32)(u32*)gbastackptr, 0xf, 32, 0); //for storing to stack
			
			//iprintf("offset:%x per each block size:%x",(unsigned int)ofset,(int)gba_branch_block_size);
			
			//manual execution routine
			cpu_fdexecute();
			
			
			}
			
			if(0 == (REG_KEYINPUT & (KEY_L))){
			
			//DO NOT MODIFY CPSR WHEN CHANGING CPU MODES.
				//updatecpuflags(1,cpsrvirt,0x12); //swap CPU-stack to usermode
				//iprintf("switch to 0x10");
			//int execute_pc_until = 0x080003a0;
			//iprintf("executed %d opcodes until",(int)executeuntil_pc(execute_pc_until));
			
			
			//testing ARM codes!
			//#define DEBUGEMU
			//disarmcode(0xE0E010B2);
			
            //JUMP TO DESIRED PC ADDRESS!
            u32 address = (u32)0x08000400; //0x08000400: PSR flags bugged -
            executeuntil_pc(address);
            
			}
			
			if(0 == (REG_KEYINPUT & (KEY_R))){
				//updatecpuflags(1,cpsrvirt,0x13); //swap stack to sys
				//iprintf("switch to 0x11");
				u32 tempaddr=((0x08000000)+((u16)rand()&0xffff));
				iprintf("gba:%x:->[%x] \n",(unsigned int)tempaddr,(unsigned int)CPUReadMemory(tempaddr));
			}
			
			
//psg noise test if (temp3<0) temp3 = 0; else if (temp3>0x500) temp3 = 0x500;
//psg noise test if (temp4<0) temp4 = 0; else if (temp4>70) temp4 = 70;
//psg noise test if (temp5<0) temp5 = 0; else if (temp5>300) temp5 = 300;

//psg noise test *(buffer_output+0)=(temp3<<16)|((temp5<<0)&0xffff); *(buffer_output+1)=temp4;
//psg noise test sendfifo(buffer_output);

//scanKeys();

}

void hblank_thread(){

	//1) Spinlock scheduled threads
	#ifdef SPINLOCK_CODE
		spinlock_sched(); //spinlock handler (and where all threads executes) (useful for lockingdown threads/events)
	#endif
	
	//2) Interrupts and GBA Core CPU handle
	//Interrupts should be here as well : generate threads based time (IF/IE/IO/DMA/ETC)
	//cpu_calculate();
	
	//3) GBA Core Fetch Decode Execute Routine
	//cpu_fdexecute();
	
	//n) GBA Core CPU ticks
	//DTCM mechanical refresh method
	hblank_ticks++;
}

void vcount_thread(){
    
    if(VCOUNT > 160)
        VCOUNT = 0;
    else
        VCOUNT++;
    
}
