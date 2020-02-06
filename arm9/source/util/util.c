#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"
#include "Util.h"
#include "opcode.h"
#include "main.h"
#include "fatfslayerTGDS.h"
#include "InterruptsARMCores_h.h"
#include "ipcfifoTGDSUser.h"
#include "ff.h"
#include "reent.h"
#include "sys/types.h"
#include "consoleTGDS.h"
#include "utilsTGDS.h"
#include "devoptab_devices.h"
#include "posixHandleTGDS.h"
#include "zlib.h"
#include "xenofunzip.h"
#include "gbaemu4ds_fat_ext.h"
#include "pu.h"
#include "supervisor.h"
#include "buffer.h"
#include "translator.h"
#include "gba.arm.core.h"
#include "EEprom.h"
#include "Flash.h"
#include "RTC.h"
#include "Sram.h"
#include "System.h"

int i=0;
u32 gba_entrypoint = 0;

//VA + ( DTCMTOP - (stack_size)*4) - dtcm_reservedcode[end_of_usedDTCMstorage] (we use for the emu)  in a loop of CACHE_LINE size

struct gbaheader_t gbaheader;

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
//printf(" ammount of register bytes: %x",(unsigned int)lookupu16(thumbinstr&0xff));
//u32 temp=0x00000000;
//printf("lut:%d",lutu32bitcnt(temp));	//(thumbinstr&0xff)

const u32  objtilesaddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakramwaitstate[4] = { 4, 3, 2, 8 };
const u8 gamepakwaitstate[4] =  { 4, 3, 2, 8 };
const u8 gamepakwaitstate0[2] = { 2, 1 };
const u8 gamepakwaitstate1[2] = { 4, 1 };
const u8 gamepakwaitstate2[2] = { 8, 1 };
const bool isInRom [16]=
  { false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, false, false };


u32 dummycall(u32 arg){
	//printf ("hi i am a dummy call whose arg is: [%x] ",(unsigned int)arg);
	return arg;
}

// swaps a 16-bit value
u16 swap16(u16 v){
	return (v<<8)|(v>>8);
}


u32 UPDATE_REG(u32 address, u32 value){
	WRITE16LE(iomem[address],(u16)value);
return 0;
}


//get physical stack sz (curr stack *) //value: gbastack address base u32 *
int getphystacksz(u32 * curr_stack){

if((u32)(u32*)curr_stack == (u32)&gbastck_usr[0])
	return gba_stack_usr_size; //printf("stack usr! "); 
	
else if ((u32)(u32*)curr_stack == (u32)&gbastck_fiq[0])
	return gba_stack_fiq_size; //printf("stack fiq! ");
 
else if ((u32)(u32*)curr_stack == (u32)&gbastck_irq[0])
	return gba_stack_irq_size; //printf("stack irq! "); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_svc[0])
	return gba_stack_svc_size; //printf("stack svc! "); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_abt[0])
	return gba_stack_abt_size; //printf("stack abt! "); 

else if ((u32)(u32*)curr_stack == (u32)&gbastck_und[0])
	return gba_stack_und_size; //printf("stack und! "); 

//else if ((u32)(u32*)curr_stack == (u32)&gbastck_sys[0])
	//return gba_stack_sys_size; //printf("stack sys! "); 

else if ((u32)(u32*)curr_stack == (u32)&branch_stack[0])
	return gba_branch_table_size; //branch table size 
	
else
	return 0xdeaddead; //printf("ERROR STACK NOT DETECTED  ");
	
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
			//printf(" <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			//exRegs[j]=0xc070+(j<<8);
			//printf("stack: @ %x for save data",(unsigned int)((u32)(u32*)stackfpbu+(ctr*4)));
			cpuwrite_word((unsigned int)(((u32)stackfpbu)+(ctr*4)),0xc070+(j<<8));
			//printf("(%x)[%x]",(unsigned int)((u32)(u32*)stackfpbu+(ctr*4)), (unsigned int) cpuread_word((u32)((u32)(u32*)stackfpbu+(ctr*4))));
			j++;
		}
	
		//debug
		//if(ctr>15) { printf("halt"); while(1);}
	}
	j=0;
	// 2/2 reads and check
	for(ctr=0;ctr<(size/4);ctr++){

		//fill! (+1 because we want the real integer modulus) 
		if( ((ctr+1) % 0x10) == 0){
			//printf(" <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			
			if(cpuread_word( ((u32)stackfpbu+(ctr*4))) == (0xc070+(j<<8)) ){
				cpuwrite_word(((u32)(u32*)stackfpbu+(ctr*4)),0x0); //clr block
				j++;
			}
			else{
				printf(" [GBAStack] STUCK AT: base(%x)+(%x)",(unsigned int)(u32*)stackfpbu,(unsigned int)(ctr*4));
				printf(" [GBAStack] value: [%x]",(unsigned int)cpuread_word((((u32)stackfpbu+(ctr*4)))));
				
				while (1);
			}
		}
	
		//debug
		//if(ctr>15) { printf("halt"); while(1);}
	}
	
	
	return (u32*)size;
}
else if(testmode==1){

	for(ctr=0;ctr<((int)(size) - (0x4*17) );ctr++){

		//fill!
		if( (ctr % 0x10) == 0){
			//printf(" <r%d:[%x]>",j,(unsigned int)*((u32*)branchfpbu+j));
			j=0;
		}

		else {
			exRegs[j]=0xc070+(j<<8);
			j++;
		}

		if ( ((ctr % (gba_branch_block_size)) == 0) && (ctr != 0)) {
			stackfpbu=cpubackupmode((u32*)(stackfpbu),exRegs,cpsrvirt); //already increases fp
			//printf("b.ofset:%x ",(unsigned int)branchfpbu);
			//ofset+=0x1;
		}
	}

	//printf("1/2 stack test fp set to: %x ",(unsigned int)(u32*)branchfpbu);
	//flush workreg
	for(j=0;j<0x10;j++){
		*((u32*)(u32)&exRegs+j)=0x0;
	}

	//debug
	//branchfpbu=cpurestoremode((u32*)(u32)branchfpbu, &gbavirtreg[0]);
	//ldmiavirt((u8*)gbavirtreg[0]+(0x0), (u32)(u32*)(branchfpbu), 0xffff, 32, 0);

	//debug check if 16 regs address are recv OK
	//for(i=0;i<16;i++){
	//	printf(" REG%d [%x]",i,(unsigned int)*((u32*)gbavirtreg[0]+i));
	//}

	for(ctr=0;ctr<((int)(size) - (0x4*17));ctr++){

		if ( ((ctr % (gba_branch_block_size)) == 0)) {
			stackfpbu=cpurestoremode((u32*)(stackfpbu),exRegs);
			//printf("b.ofset->restore :%x ",(unsigned int)(u32*)branchfpbu);
			//ofset+=0x4;
		}

		//reset cnt!
		if( (ctr % 0x10) == 0){
			//printf(" <r%d:[%x]>",j,(unsigned int)exRegs[j]);
			j=0;
		}

		else {
			if ( exRegs[j] == (u32)(0xc070+(j<<8)))
				j++;
			else {
				//check why if 16 regs address are recv OK
				for(i=0;i<16;i++){
					//printf(" REG%d[%x]",i,(unsigned int)exRegs[i]);
				}
				printf(" [branchstack] STUCK AT: %x:%x",(unsigned int)(u32*)(stackfpbu+1),(unsigned int)exRegs[j]);
				while(1);
			}
		}
	}
	//printf("2/2 stack test fp set to: %x- stack tests OK ;) ",(u32)(u32*)branchfpbu);
	return stackfpbu;
}

else return (u32*)0x1;

}


u32 * updatestackfp(u32 * currstack_fp, u32 * stackbase){
	int stacksz=0;
	stacksz=getphystacksz(stackbase);
	
	//debug
	//printf(" stkfp_curr:%x->offset%x",(unsigned int)(u32*)currstack_fp,(int)((unsigned int)(u32*)currstack_fp-(u32)(u32*)stackbase));
	
	//if framepointer is OK
	if ( 	((int)((u32)(u32*)currstack_fp-(u32)(u32*)stackbase) >= 0) //MUST start from zero as ptr starts from zero
			&&
			((int)((u32)(u32*)currstack_fp-(u32)(u32*)stackbase) < stacksz)
	){
		//debug
		//printf("stack top: %x / stack_offset:%x ",stacksz , (int)((unsigned int)(u32*)currstack_fp-(unsigned int)(u32*)stackbase));
		return currstack_fp;
	}
	//if overflow stack, fix pointer and make it try again
	else if( ((int)(((u32)(u32*)currstack_fp-(u32)(u32*)stackbase))+0x4) >= stacksz) {
		//printf("stacktop!");			//debug
		gbastckfpadr_curr=currstack_fp-1; 
		return 0;
	}
	//else if underflow stack, fix pointer and make it try again
	else if (  (int)((u32*)currstack_fp-(u32)(u32*)stackbase) < (int)0){
		//printf("stack underflow!");	//debug
		gbastckfpadr_curr=currstack_fp+1;
		return 0;
	}
	else
		return (u32*)0;
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


void initmemory(){
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
  
	gbawram = (u8*)malloc(256*1024);
	palram = (u8*)malloc(0x400);
	gbabios = (u8*)malloc(0x4000);
	gbaintram = (u8*)malloc(0x8000);
	gbaoam = (u8*)malloc(0x400);
	gbacaioMem = (u8*)malloc(0x400);
	saveram = (u8*)malloc(128*1024);

	u8 vramofs=(0<<3);
	u8 vrammst=(1<<0);
	
	u32store(0x04000240,vrammst|vramofs|(1<<7)); //MST 1 , vram offset 0 (A), VRAM enable
	
	int ramtestsz=0;
	
	ramtestsz=wramtstasm((int)workRAM,256*1024);
	if(ramtestsz==alignw(256*1024))
		printf(" GBAWRAM tst: OK :[%x]!",(unsigned int)(u8*)workRAM); 
	else{
		printf(" FAILED ALLOCING GBAEWRAM[%x]:@%d (bytes: %d)",(unsigned int)(u8*)gbawram,ramtestsz,0x10000);
		while(1);
	}
	memset((void*)workRAM,0x0,0x10000);

	//OK
	ramtestsz=wramtstasm((int)iomem,0x400);
	if(ramtestsz==alignw(0x400))
		printf(" IOMEM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)iomem,ramtestsz);
	else 
		printf(" IOMEM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)iomem,ramtestsz);
	memset((void*)iomem,0x0,0x400);


	//this is OK
	ramtestsz=wramtstasm((int)bios,0x4000);
	if(ramtestsz==alignw(0x4000))
		printf(" BIOS tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)bios,ramtestsz);
	else 
		printf(" BIOS tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)bios,ramtestsz);
	memset((void*)bios,0x0,0x4000);

	//this is OK
	ramtestsz=wramtstasm((int)internalRAM,0x8000);
	if(ramtestsz==alignw(0x8000))
		printf(" IRAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)internalRAM,ramtestsz);
	else 
		printf(" IRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)internalRAM,ramtestsz);
	memset((void*)internalRAM,0x0,0x8000);

	//this is OK
	ramtestsz=wramtstasm((int)(u8*)palram,0x400);
	if(ramtestsz==alignw(0x400))
		printf(" PaletteRAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)palram,ramtestsz);
	else 
		printf(" PaletteRAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)palram,ramtestsz);
	memset((void*)palram,0x0,0x400);

	//this is OK
	ramtestsz=wramtstasm((int)oam,0x400);
	if(ramtestsz==alignw(0x400))
		printf(" OAM tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)oam,ramtestsz);
	else 
		printf(" OAM tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)oam,ramtestsz);
	memset((void*)oam,0x0,0x400);

	//this is OK
	ramtestsz=wramtstasm((int)gbacaioMem,0x400);
	if(ramtestsz==alignw(0x400))
		printf(" caioMem tst: OK (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gbacaioMem,ramtestsz);
	else 
		printf(" caioMem tst: FAIL at (%d bytes @ 0x%x+0x%x)",ramtestsz,(unsigned int)(u8 *)gbacaioMem,ramtestsz);
	memset((void*)gbacaioMem,0x0,0x400);

	//gba stack test
	for(i=0;i<sizeof(gbastck_usr)/2;i++){
		
		u16store((u32)&gbastck_usr[0]+(i*2),0xc070+i);	//gbastack[i] = 0xc070;	
		
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_usr[0]+i*2)!=0xc070+i){
			printf("stackusr:failed writing @ %x ",(unsigned int)&gbastck_usr+(i*2));
			printf("stackusr:reads: %x",(unsigned int)u16read((u32)&gbastck_usr[0]+i*2));
			printf("stackusr:base: %x",(unsigned int)&gbastck_usr[0]);
			while(1);
		}
	
		u16store((u32)&gbastck_usr[0]+i*2,0x0);
	}

	for(i=0;i<sizeof(gbastck_fiq)/2;i++){
		u16store((u32)&gbastck_fiq[0]+i*2,0xc070+i); //gbastack[i] = 0xc070;
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_fiq[0]+i*2)!=0xc070+i){
			printf("stackfiq:failed writing @ %x ",(unsigned int)&gbastck_fiq+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_fiq[0]+i*2,0x0);
	}

	for(i=0;i<sizeof(gbastck_irq)/2;i++){
		u16store((u32)&gbastck_irq[0]+i*2,0xc070+i); //gbastack[i] = 0xc070;
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_irq[0]+i*2)!=0xc070+i){
			printf("stackirq:failed writing @ %x ",(unsigned int)&gbastck_irq+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_irq[0]+i*2,0x0);
	}

	for(i=0;i<sizeof(gbastck_svc)/2;i++){
		u16store((u32)&gbastck_svc[0]+i*2,0xc070+i); //gbastack[i] = 0xc070;				
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_svc[0]+i*2)!=0xc070+i){
			printf("stacksvc:failed writing @ %x ",(unsigned int)&gbastck_svc+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_svc[0]+i*2,0x0);
	}

	for(i=0;i<sizeof(gbastck_abt)/2;i++){
		u16store((u32)&gbastck_abt[0]+i*2,0xc070+i); //gbastack[i] = 0xc070;				
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory 

		if(u16read((u32)&gbastck_abt[0]+i*2)!=0xc070+i){
			printf("stackabt:failed writing @ %x ",(unsigned int)&gbastck_abt+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_abt[0]+i*2,0x0);
	}

	for(i=0;i<sizeof(gbastck_und)/2;i++){
		u16store((u32)&gbastck_und[0]+i*2,0xc070+i); //gbastack[i] = 0xc070;				
		//printf("%x ",(unsigned int)gbastack[i]); //printf("%x ",(unsigned int)**(&gbastack+i*2)); //non contiguous memory

		if(u16read((u32)&gbastck_und[0]+i*2)!=0xc070+i){
			printf("stackund:failed writing @ %x ",(unsigned int)&gbastck_und+(i*2));
			while(1);
		}
	
		u16store((u32)&gbastck_und[0]+i*2,0x0);
	}
	
	//usr/sys are same stacks, so removed
}

void initemu(){

	sound_clock_ticks = 167772; // 1/100 second
	
	//refresh jump opcode in biosProtected vector
	biosProtected[0] = 0x00;
	biosProtected[1] = 0xf0;
	biosProtected[2] = 0x29;
	biosProtected[3] = 0xe1;
	  

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

	initmemory();
	printf("init&teststacks OK");

	//GBA address MAP setup
	for(i = 0; i < 256; i++){
		map[i].address = (u8 *)(u32)0;
		map[i].mask = 0;
	}

	for(i = 0; i < 0x400; i++)
		ioReadable[i] = true;
	for(i = 0x10; i < 0x48; i++)
		ioReadable[i] = false;
	for(i = 0x4c; i < 0x50; i++)
		ioReadable[i] = false;
	for(i = 0x54; i < 0x60; i++)
		ioReadable[i] = false;
	for(i = 0x8c; i < 0x90; i++)
		ioReadable[i] = false;
	for(i = 0xa0; i < 0xb8; i++)
		ioReadable[i] = false;
	for(i = 0xbc; i < 0xc4; i++)
		ioReadable[i] = false;
	for(i = 0xc8; i < 0xd0; i++)
		ioReadable[i] = false;
	for(i = 0xd4; i < 0xdc; i++)
		ioReadable[i] = false;
	for(i = 0xe0; i < 0x100; i++)
		ioReadable[i] = false;
	for(i = 0x110; i < 0x120; i++)
		ioReadable[i] = false;
	for(i = 0x12c; i < 0x130; i++)
		ioReadable[i] = false;
	for(i = 0x138; i < 0x140; i++)
		ioReadable[i] = false;
	for(i = 0x144; i < 0x150; i++)
		ioReadable[i] = false;
	for(i = 0x15c; i < 0x200; i++)
		ioReadable[i] = false;
	for(i = 0x20c; i < 0x300; i++)
		ioReadable[i] = false;
	for(i = 0x304; i < 0x400; i++)
		ioReadable[i] = false;

	//vba core init
	GBADISPCNT  = 0x0080;
	GBADISPSTAT = 0x0000;

	#ifdef NOBIOS
		GBAVCOUNT   =  0 ;
	#else
		GBAVCOUNT 	=  0x007E;
	#endif

	GBABG0CNT   = 0x0000;
	GBABG1CNT   = 0x0000;
	GBABG2CNT   = 0x0000;
	GBABG3CNT   = 0x0000;
	GBABG0HOFS  = 0x0000;
	GBABG0VOFS  = 0x0000;
	GBABG1HOFS  = 0x0000;
	GBABG1VOFS  = 0x0000;
	GBABG2HOFS  = 0x0000;
	GBABG2VOFS  = 0x0000;
	GBABG3HOFS  = 0x0000;
	GBABG3VOFS  = 0x0000;
	GBABG2PA    = 0x0100;
	GBABG2PB    = 0x0000;
	GBABG2PC    = 0x0000;
	GBABG2PD    = 0x0100;
	GBABG2X_L   = 0x0000;
	GBABG2X_H   = 0x0000;
	GBABG2Y_L   = 0x0000;
	GBABG2Y_H   = 0x0000;
	GBABG3PA    = 0x0100;
	GBABG3PB    = 0x0000;
	GBABG3PC    = 0x0000;
	GBABG3PD    = 0x0100;
	GBABG3X_L   = 0x0000;
	GBABG3X_H   = 0x0000;
	GBABG3Y_L   = 0x0000;
	GBABG3Y_H   = 0x0000;
	GBAWIN0H    = 0x0000;
	GBAWIN1H    = 0x0000;
	GBAWIN0V    = 0x0000;
	GBAWIN1V    = 0x0000;
	GBAWININ    = 0x0000;
	GBAWINOUT   = 0x0000;
	GBAMOSAIC   = 0x0000;
	GBABLDMOD   = 0x0000;
	GBACOLEV    = 0x0000;
	GBACOLY     = 0x0000;
	GBADM0SAD_L = 0x0000;
	GBADM0SAD_H = 0x0000;
	GBADM0DAD_L = 0x0000;
	GBADM0DAD_H = 0x0000;
	GBADM0CNT_L = 0x0000;
	GBADM0CNT_H = 0x0000;
	GBADM1SAD_L = 0x0000;
	GBADM1SAD_H = 0x0000;
	GBADM1DAD_L = 0x0000;
	GBADM1DAD_H = 0x0000;
	GBADM1CNT_L = 0x0000;
	GBADM1CNT_H = 0x0000;
	GBADM2SAD_L = 0x0000;
	GBADM2SAD_H = 0x0000;
	GBADM2DAD_L = 0x0000;
	GBADM2DAD_H = 0x0000;
	GBADM2CNT_L = 0x0000;
	GBADM2CNT_H = 0x0000;
	GBADM3SAD_L = 0x0000;
	GBADM3SAD_H = 0x0000;
	GBADM3DAD_L = 0x0000;
	GBADM3DAD_H = 0x0000;
	GBADM3CNT_L = 0x0000;
	GBADM3CNT_H = 0x0000;
	GBATM0D     = 0x0000;
	GBATM0CNT   = 0x0000;
	GBATM1D     = 0x0000;
	GBATM1CNT   = 0x0000;
	GBATM2D     = 0x0000;
	GBATM2CNT   = 0x0000;
	GBATM3D     = 0x0000;
	GBATM3CNT   = 0x0000;
	GBAP1       = 0x03FF;
	GBAIE=0x0000;			//  IE       = 0x0000;
	GBAIF=0x0000;			//  IF       = 0x0000;
	GBAIME=0x0000;			// IME      = 0x0000;

	cpu_updateregisters(0x00, GBADISPCNT);	//UPDATE_REG(0x00, GBADISPCNT);
	cpu_updateregisters(0x06, GBAVCOUNT);	//UPDATE_REG(0x06, VCOUNT);
	cpu_updateregisters(0x20, GBABG2PA);		//UPDATE_REG(0x20, GBABG2PA);
	cpu_updateregisters(0x26, GBABG2PD);		//UPDATE_REG(0x26, GBABG2PD);
	cpu_updateregisters(0x30, GBABG3PA);		//UPDATE_REG(0x30, GBABG3PA);
	cpu_updateregisters(0x36, GBABG3PD);		//UPDATE_REG(0x36, GBABG3PD);
	cpu_updateregisters(0x130, GBAP1);		//UPDATE_REG(0x130, GBAP1);
	cpu_updateregisters(0x88, 0x200);			//UPDATE_REG(0x88, 0x200);

	#ifndef NOBIOS
	lcdTicks = 1008;
	#else
	lcdTicks = 208;
	#endif

	timer0On = false;
	timer0Ticks = 0;
	timer0Reload = 0;
	timer0ClockReload  = 0;
	timer1On = false;
	timer1Ticks = 0;
	timer1Reload = 0;
	timer1ClockReload  = 0;
	timer2On = false;
	timer2Ticks = 0;
	timer2Reload = 0;
	timer2ClockReload  = 0;
	timer3On = false;
	timer3Ticks = 0;
	timer3Reload = 0;
	timer3ClockReload  = 0;
	dma0Source = 0;
	dma0Dest = 0;
	dma1Source = 0;
	dma1Dest = 0;
	dma2Source = 0;
	dma2Dest = 0;
	dma3Source = 0;
	dma3Dest = 0;
	windowOn = false;
	frameCount = 0;
	saveType = 0;
	layerEnable = GBADISPCNT & layerSettings;

	//OK so far
	map[0].address = bios;
	map[0].mask = 0x3FFF;
	map[2].address = workRAM;
	map[2].mask = 0x3FFFF;
	map[3].address = internalRAM; 
	map[3].mask = 0x7FFF;
	map[4].address = iomem;
	map[4].mask = 0x3FF;
	map[5].address = palram;
	map[5].mask = 0x3FF;
	map[6].address = vram;
	map[6].mask = 0x1FFFF;
	map[7].address = oam;
	map[7].mask = 0x3FF;
	map[8].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	 //ROM entrypoint
	map[8].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/ //MASK for GBA addressable ROMDATA
	map[9].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[9].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
	map[10].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[10].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/
	map[12].address = (u8*)(u32)(exRegs[0xf]&0xfffffffe);	// rom; 	/
	map[12].mask = 0x1FFFFFF;			// 0x1FFFFFF; 	/


	//setup patches for gbaoffsets into emulator offsets 
	addrfixes[0]=(u32)(u8*)bios;	//@bios
	addrfixes[1]=(u32)0x4000;
	addrfixes[2]=(u32)(u8*)workRAM;	//@ewram
	addrfixes[3]=(u32)0x10000;
	addrfixes[4]=(u32)(u8*)internalRAM;	///internal wram
	addrfixes[5]=(u32)0x8000;
	addrfixes[6]=(u32)(u8*)iomem;					//@GBA I/O map
	addrfixes[7]=(u32)0x00000800;
	addrfixes[8]=(u32)(u8*)palram;		//palram;				//@BG/OBJ Palette RAM
	addrfixes[9]=(u32)0x400;
	addrfixes[0xa]=(u32)(u8*)vram;	//@vram
	addrfixes[0xb]=(u32)(1024*128);
	addrfixes[0xc]=(u32)(u8*)oam;	//@object attribute memory
	addrfixes[0xd]=(u32)0x400;
	addrfixes[0xe]=(u32)0x0;						//(u32)0x08000000;		//@rom
	addrfixes[0xf]=(u32)0x0;						//romsize;				//@rom top

	//Cpu_Stack_USR EQU 0x03007F00 ; GBA USR stack adress
	//Cpu_Stack_IRQ EQU 0x03007FA0 ; GBA IRQ stack adress
	//Cpu_Stack_SVC EQU 0x03007FE0 ; GBA SVC stack adress

	//reg[13].I = 0x03007F00;
	//reg[15].I = 0x08000000;
	//reg[16].I = 0x00000000;
	//reg[R13_IRQ].I = 0x03007FA0;
	//reg[R13_SVC].I = 0x03007FE0;
	//armIrqEnable = true;

	//new stack setup
	//1) set stack base 2) to detect stack top just sizeof(gbastck_mode), framepointer has the current pos (from 0 to n) used so far
	gbastckadr_usr=(u32*)0x03007F00; //0x100 size & 0x10 as CPU <mode> start (usr/sys shared stack)
	exRegs[0xd]=exRegs_r13usr[0]=(u32)(u32*)gbastckadr_usr;

	gbastckfp_usr=(u32*)0x03007F00;
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_usr,0xff,0x0)==(int)0xff)
			printf("USR stack OK!");
		else
			printf("USR stack WRONG!");
	#endif

	gbastckadr_fiq=(u32*)(gbastckadr_usr-GBASTACKSIZE); //custom fiq stack
	exRegs_r13fiq[0]=(u32)(u32*)gbastckadr_fiq;

	gbastckfp_fiq=(u32*)(gbastckadr_usr-GBASTACKSIZE); //#GBASTACKSIZE size
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_fiq,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
			printf("FIQ stack OK!");
		else
			printf("FIQ stack WRONG!");
	#endif

	gbastckadr_irq=(u32*)0x03007FA0;
	exRegs_r13irq[0]=(u32)(u32*)gbastckadr_irq;

	gbastckfp_irq=(u32*)0x03007FA0;
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_irq,0xff,0x0)==(int)0xff)
			printf("IRQ stack OK!");
		else
			printf("IRQ stack WRONG!");
	#endif

	gbastckadr_svc=(u32*)0x03007FE0;
	exRegs_r13svc[0]=(u32)(u32*)gbastckadr_svc;

	gbastckfp_svc=(u32*)0x03007FE0;
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_svc,0xff,0x0)==(int)0xff)
			printf("SVC stack OK!");
		else
			printf("SVC stack WRONG!");
	#endif

	gbastckadr_abt=(u32*)(gbastckadr_fiq-GBASTACKSIZE); //custom abt stack
	exRegs_r13abt[0]=(u32)(u32*)gbastckadr_abt;

	gbastckfp_abt=(u32*)(gbastckadr_fiq-GBASTACKSIZE);
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_abt,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
			printf("ABT stack OK!");
		else
			printf("ABT stack WRONG!");
	#endif

	gbastckadr_und=(u32*)(gbastckadr_abt-GBASTACKSIZE); //custom und stack
	exRegs_r13und[0]=(u32)(u32*)gbastckadr_und;

	gbastckfp_und=(u32*)(gbastckadr_abt-GBASTACKSIZE);
	#ifdef STACKTEST
		if((int)stack_test(gbastckadr_und,(int)GBASTACKSIZE,0x0)==(int)GBASTACKSIZE)
			printf("UND stack OK!");
		else
			printf("UND stack WRONG!");
	#endif

}


int utilload(const char *file,u8 *data,int size,bool extram){ //*file is filename (.gba)
																//*data is pointer to store rom  / always ~256KB &size at load
	//printf("ewram top: %x ", (unsigned int)(((int)&__ewram_end) - 0x1));
	//while(1);
	#ifndef NOBIOS
	//bios copy to biosram
	FILE *f = fopen("0:/bios.bin", "r");
	if(!f){ 
		printf("there is no bios.bin in SD root!"); while(1);
	}

	int fileSize=fread((void*)(u8*)bios, 1, 0x4000,f);

	fclose(f);
	if(fileSize!=0x4000){
		printf("failed bios.bin copy @ %x! so far:%d bytes",(unsigned int)bios,fileSize);
		while(1);
	}
	
	printf("bios OK!");
	/*
		// tempbuffer2 
		printf(" /// GBABIOS @ %x //",(unsigned int)(u8*)bios);
			
		for(i=0;i<16;i++){
			printf(" %x:[%d] ",i,(unsigned int)*((u32*)gbabios+i));
			
			if (i==15) printf("");
			
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
		printf("Error opening image %s",file);
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
		printf(" error ret. gbaheader (size rd: %d)",temp);
		while(1);
	}

	//printf(" util.c filesize: %d ",fileSize);
	//printf(" util.c entrypoint: %x ",(unsigned int)0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
	*/
	
	generatefilemap(f,fileSize);
	if(data == 0){ //null rom destination pointer? allocate space for it	
		/*8K for futur alloc 0x2000 unused*/
		//romSize = (((int)&__ewram_end) - 0x1) - ((int)sbrk(0) + 0x5000 + 0x2000); // availablesize = NDSRAMuncSZ - c_progbrk + (20480 +  8192) : 20480 romsize
		
		//filesize
		romsize=fileSize;	//size readjusted for final alloc'd rom
		romsize=fileSize;
		
		//rom entrypoint
		rom_entrypoint=(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
		
		//set rom address
		exRegs[0xf]=(u32)0x08000000;	
		
		//printf("entrypoint @ %x! ",(unsigned int)(u32*)rom_entrypoint);
	}
	fclose(f);
	printf("generated filemap! OK:");
	return romsize; //rom buffer size
}

int loadrom(const char *filename, bool extram){	
	//#ifdef ROMTEST
	//rom = (u8*) &rom_pl_bin;
	//romsize = (int) rom_pl_bin_size;
	//#endif

	romsize = 0x40000; //256KB partial romsize

	u8 *whereToLoad;
	whereToLoad=(u8*)0;

	if(cpuIsMultiBoot) whereToLoad = workRAM;

	romsize = utilload(filename,whereToLoad,romsize,extram);
	if(romsize==0){ //set ups u8 * rom to point to allocated buffer and returns *partial* or full romSize
		printf("error retrieving romSize ");
		return 0;
	}
	return romsize;
}

void u32store(u32 address, u32 value){
	*(u32*)(address)=value;
}

void u16store(u32 address, u16 value){
	*(u16*)(address)=value;
}

void u8store(u32 addr, u8 value){
	*(u8*)(addr)=value;
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

u32 READ32LE(u8 * addr){
    return u32read((u32)(u8*)addr);
}

u16 READ16LE(u8 * addr){
    return u16read((u32)(u8*)addr);
}

void WRITE32LE(u8 * x,u32 v){
	u32store((u32)x,v);
}

void WRITE16LE(u8 * x,u16 v){
	u16store((u32)x,v);
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
bool setarmstate(u32 psr){
	//->switch to arm/thumb mode depending on cpsr for virtual env
	if( ((psr>>5)&1) )
		armState=false;
	else
		armState=true;
	
    cpsrvirt &= ~(1<<5);
    cpsrvirt |= (psr&(1<<5));
	return armState;
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
int executeuntil_pc(u32 target_pc){
	int opcodes_executed=0;
	
	#ifdef DEBUGEMU
	#undef DEBUGEMU
	#endif

	//execute until desired opcode:
	while(target_pc != exRegs[0xf]){
		cpu_fdexecute();
		opcodes_executed++;
	}

	if(was_debugemu_enabled==1){
		#define DEBUGEMU
	}

	return opcodes_executed;
}

void CPUInit(const char *biosFileName, bool useBiosFile,bool extram)
{
	initemu();
	
#ifdef WORDS_BIGENDIAN
  if(!cpuBiosSwapped) {
    for(unsigned int i = 0; i < sizeof(myROM)/4; i++) {
      WRITE32LE((u8*)&myROM[i], myROM[i]);
    }
    cpuBiosSwapped = true;
  }
#endif
  gbaSaveType = 0;
  eepromInUse = 0;
  saveType = 0;
  useBios = false;
  
  if(useBiosFile) {
    int size = 0x4000;
    if(utilLoad(biosFileName,
                bios,
                size,extram)) {
      if(size == 0x4000)
        useBios = true;
      else
        printf("Invalid BIOS file size");
    }
  }
  
  if(!useBios) {
    memcpy(bios, myROM, sizeof(myROM));
  }

  int i = 0;

  biosProtected[0] = 0x00;
  biosProtected[1] = 0xf0;
  biosProtected[2] = 0x29;
  biosProtected[3] = 0xe1;

  for(i = 0; i < 256; i++) {
    int count = 0;
    int j;
    for(j = 0; j < 8; j++)
      if(i & (1 << j))
        count++;
    
    cpuBitsSet[i] = count;
    
    for(j = 0; j < 8; j++)
      if(i & (1 << j))
        break;
    cpuLowestBitSet[i] = j;
  }

  for(i = 0; i < 0x400; i++)
    ioReadable[i] = true;
  for(i = 0x10; i < 0x48; i++)
    ioReadable[i] = false;
  for(i = 0x4c; i < 0x50; i++)
    ioReadable[i] = false;
  for(i = 0x54; i < 0x60; i++)
    ioReadable[i] = false;
  for(i = 0x8c; i < 0x90; i++)
    ioReadable[i] = false;
  for(i = 0xa0; i < 0xb8; i++)
    ioReadable[i] = false;
  for(i = 0xbc; i < 0xc4; i++)
    ioReadable[i] = false;
  for(i = 0xc8; i < 0xd0; i++)
    ioReadable[i] = false;
  for(i = 0xd4; i < 0xdc; i++)
    ioReadable[i] = false;
  for(i = 0xe0; i < 0x100; i++)
    ioReadable[i] = false;
  for(i = 0x110; i < 0x120; i++)
    ioReadable[i] = false;
  for(i = 0x12c; i < 0x130; i++)
    ioReadable[i] = false;
  for(i = 0x138; i < 0x140; i++)
    ioReadable[i] = false;
  for(i = 0x144; i < 0x150; i++)
    ioReadable[i] = false;
  for(i = 0x15c; i < 0x200; i++)
    ioReadable[i] = false;
  for(i = 0x20c; i < 0x300; i++)
    ioReadable[i] = false;
  for(i = 0x304; i < 0x400; i++)
    ioReadable[i] = false;

  
  //if(romSize < 0x1fe2000) {
  //  *((u16 *)&rom[0x1fe209c]) = 0xdffa; // SWI 0xFA
  //  *((u16 *)&rom[0x1fe209e]) = 0x4770; // BX LR
  //} 
  //else {
    //agbPrintEnable(false);
  //}
}


int utilLoad(const char *file,u8 *data,int size,bool extram){ //*file is filename (.gba)
																//*data is pointer to store rom  / always ~256KB &size at load

#ifndef ROMTEST

#ifdef BIOSHANDLER
    //bios copy to biosram
    FILE *f = fopen("0:/bios.bin", "r");
    if(!f){ 
        printf("there is no bios.bin in root!"); while(1);
    }

    int fileSize=fread((void*)(u8*)bios, 1, 0x4000,f);

    fclose(f);
    if(fileSize!=0x4000){
        printf("failed bios copy @ %x! so far:%d bytes",(unsigned int)bios,fileSize);
        while(1);
    }

#else
    int fileSize=0;
    FILE *f;
#endif

	//gbarom setup
	f = fopen(file, "rb");
	if(!f) {
		printf("Error opening image %s",file);
		return 0;
	}

	//copy first32krom/ because gba4ds's first 32k sector is bugged (returns 0 reads)
	//fread(buffer, strlen(c)+1, 1, fp);
	fread((u8*)&first32krom[0],sizeof(first32krom),1,f);

	fseek(f,0,SEEK_END);
	fileSize = ftell(f);
	fseek(f,0,SEEK_SET);

	generatefilemap(f,fileSize);

	if(data == 0){ //null rom destination pointer? allocate space for it	
		
		//filesize
		romSize=fileSize;	//size readjusted for final alloc'd rom
		
		//gba header rom entrypoint
		//(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
		
		//printf("entrypoint @ %x! ",(unsigned int)(u32*)rom);
	}

	ichflyfilestream = f; //pass the filestreampointer and make it global
	ichflyfilestreamsize = fileSize;
		
	printf("generated filemap! OK:");

	#else
		romSize = puzzle_original_size;
	#endif
	
	//set usual rom entrypoint
	exRegs[0xf]=(u8*)(u32)0x08000000;	

	return romSize; //rom buffer size
}




int CPULoadRom(const char *szFile,bool extram){
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
	exRegs[0xf] = 0;
	romSize = 0x40000;
	/*workRAM = (u8*)0x02000000;(u8 *)calloc(1, 0x40000);
	if(workRAM == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
					  "WRAM");
		return 0;
	}*/

	u8 *whereToLoad = exRegs[0xf];
	if(cpuIsMultiBoot)whereToLoad = workRAM;
	if(!utilLoad(szFile,whereToLoad,romSize,extram))
	{
		return 0;
	}

	/*internalRAM = (u8 *)0x03000000;//calloc(1,0x8000);
	if(internalRAM == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"IRAM");
		//CPUCleanUp();
		return 0;
	}*/
	/*paletteRAM = (u8 *)0x05000000;//calloc(1,0x400);
	if(paletteRAM == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"PRAM");
		//CPUCleanUp();
		return 0;
	}*/
	/*vram = (u8 *)0x06000000;//calloc(1, 0x20000);
	if(vram == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"VRAM");
		//CPUCleanUp();
		return 0;
	}*/      
	/*oam = (u8 *)0x07000000;calloc(1, 0x400); //ichfly test
	if(oam == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"oam");
		//CPUCleanUp();
		return 0;
	}   
	pix = (u8 *)calloc(1, 4 * 241 * 162);
	if(pix == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"PIX");
		//CPUCleanUp();
		return 0;
	}  */
	/*ioMem = (u8 *)calloc(1, 0x400);
	if(ioMem == NULL) {
		systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),"IO");
		//CPUCleanUp();
		return 0;
	}*/      
	
	flashInit();
	eepromInit();

	//CPUUpdateRenderBuffers(true);
	return romSize;
}


//saves data to file
int ram2file_nds(char * fname,u8 * buffer,int size){
    FILE * fhandle = fopen(fname,"w");
    if(fhandle){
        
        int sizewritten=fwrite((u8*)buffer, 1, size, fhandle); //2) perform read (512bytes read (128 reads))
        fclose(fhandle);
        if(size == sizewritten){
            printf("OK exported %d bytes @ fname: %s ",(int)sizewritten,(char*)fname);
            return 0;
        }
        else{
            printf("error, only copied %d bytes @ fname: %s ",(int)sizewritten,(char*)fname);
        }
    }
    else{
        printf("file was not created: %s",fname);
    }
    return 1;
}