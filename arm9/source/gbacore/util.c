#include "util.h"
#include "opcode.h"
#include "..\main.h"
#include "..\disk\fatmore.h"
#include "..\pu\pu.h"
#include "..\pu\supervisor.h"
#include ".\gba.arm.core.h"
#include ".\translator.h"	//for patches setup
#include "..\disk\file_browse.h"
#include "..\settings.h"

#include ".\EEprom.h"
#include ".\Flash.h"
#include ".\Sram.h"
#include ".\System.h"


#include "../rom/puzzle_original.h"


//works FINE
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

//#define debuggerReadHalfWord(addr)  READ16LE(((u8*)&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))


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




// returns unary(decimal) ammount of bits using the Hamming Weight approach 

//8 bit depth Lookuptable 
//	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f
const u8 minilut[0x10] = {
	0,	1,	1,	2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,		//0n
};

u8 lutu16bitcnt(u16 x){
	return (minilut[x &0xf] + minilut[(x>>4) &0xf] + minilut[(x>>8) &0xf] + minilut[(x>>12) &0xf]);
}

u8 lutu32bitcnt(u32 x){
	return (lutu16bitcnt(x & 0xffff) + lutu16bitcnt(x >> 16));
}

const u32  objtilesaddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakramwaitstate[4] = { 4, 3, 2, 8 };
const u8 gamepakwaitstate[4] =  { 4, 3, 2, 8 };
const u8 gamepakwaitstate0[2] = { 2, 1 };
const u8 gamepakwaitstate1[2] = { 4, 1 };
const u8 gamepakwaitstate2[2] = { 8, 1 };


// swaps a 16-bit value
inline u16 swap16(u16 v){
	return (v<<8)|(v>>8);
}


//__attribute__((section(".itcm")))
void UPDATE_REG(u16 address, u16 value){
	WRITE16LE(((u8*)&ioMem[address]),value);
}


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


void CPUInit(const char *biosFileName, bool useBiosFile,bool extram)
{
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
        iprintf("Invalid BIOS file size");
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
    FILE *f = fopen("gba.bios", "r");
    if(!f){ 
        iprintf("there is no gba.bios in root!"); while(1);
    }

    int fileSize=fread((void*)(u8*)bios, 1, 0x4000,f);

    fclose(f);
    if(fileSize!=0x4000){
        iprintf("failed bios copy @ %x! so far:%d bytes",(unsigned int)bios,fileSize);
        while(1);
    }

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

            //copy first32krom/ because gba4ds's first 32k sector is bugged (returns 0 reads)
            //fread(buffer, strlen(c)+1, 1, fp);
            fread((u8*)&first32krom[0],sizeof(first32krom),1,f);

            fseek(f,0,SEEK_END);
            fileSize = ftell(f);
            fseek(f,0,SEEK_SET);

            generatefilemap(fileSize);

            if(data == 0){ //null rom destination pointer? allocate space for it	
                
                //filesize
                romSize=fileSize;	//size readjusted for final alloc'd rom
                
                //gba header rom entrypoint
                //(u32*)(0x08000000 + ((&gbaheader)->entryPoint & 0x00FFFFFF)*4 + 8);
                
                //iprintf("entrypoint @ %x! ",(unsigned int)(u32*)rom);
            }

            ichflyfilestream = f; //pass the filestreampointer and make it global
            ichflyfilestreamsize = fileSize;
                
            iprintf("generated filemap! OK:\n");

            #else
                romSize = puzzle_original_size;
            #endif
            
            //set usual rom entrypoint
            rom=(u8*)(u32)0x08000000;	

return romSize; //rom buffer size
}




int CPULoadRom(const char *szFile,bool extram)
{

  //bios = (u8 *)calloc(1,0x4000);
  bios = gbabios;
  if(bios == NULL) {
    iprintf("Failed to allocate memory for %s",(char*)"BIOS");
    //CPUCleanUp();
    return 0;
  }    

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
  
	rom = 0;
	romSize = 0x40000;
  /*workRAM = (u8*)0x02000000;(u8 *)calloc(1, 0x40000);
  if(workRAM == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "WRAM");
    return 0;
  }*/

  u8 *whereToLoad = rom;
  if(cpuIsMultiBoot)whereToLoad = workRAM;

		if(!utilLoad(szFile,whereToLoad,romSize,extram))
		{
			return 0;
		}

  /*internalRAM = (u8 *)0x03000000;//calloc(1,0x8000);
  if(internalRAM == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "IRAM");
    //CPUCleanUp();
    return 0;
  }*/
  /*paletteRAM = (u8 *)0x05000000;//calloc(1,0x400);
  if(paletteRAM == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "PRAM");
    //CPUCleanUp();
    return 0;
  }*/      
  /*vram = (u8 *)0x06000000;//calloc(1, 0x20000);
  if(vram == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "VRAM");
    //CPUCleanUp();
    return 0;
  }*/      
  /*oam = (u8 *)0x07000000;calloc(1, 0x400); //ichfly test
  if(oam == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "oam");
    //CPUCleanUp();
    return 0;
  }      
  pix = (u8 *)calloc(1, 4 * 241 * 162);
  if(pix == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "PIX");
    //CPUCleanUp();
    return 0;
  }  */
  /*ioMem = (u8 *)calloc(1, 0x400);
  if(ioMem == NULL) {
    systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                  "IO");
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
            iprintf("OK exported %d bytes @ fname: %s \n",(int)sizewritten,(char*)fname);
            return 0;
        }
        else{
            iprintf("error, only copied %d bytes @ fname: %s \n",(int)sizewritten,(char*)fname);
        }
    }
    else{
        iprintf("file was not created: %s",fname);
    }
    return 1;
}