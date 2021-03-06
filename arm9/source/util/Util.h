#ifndef utilGBAdefs
#define utilGBAdefs

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

#include <unistd.h>    // for sbrk()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BIT(n) (1 << (n))
#define base_factor 24

//ramtest roundup
#define ramshuffle7(n,m) ( (n* (rand() % m)) &0xfffff0) //int , top
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))	
#define alignw(n)(CHECK_BIT(n,0)==1?n+1:n)

//why get sizes over and over (and waste cycles), when you can just #define them once. same for each element size :o
#define gba_stack_usr_size sizeof(gbastck_usr)
#define gba_stack_usr_elemnt_size (sizeof(gbastck_usr[0]))

#define gba_stack_fiq_size sizeof(gbastck_fiq)
#define gba_stack_fiq_elemnt_size (sizeof(gbastck_fiq[0]))

#define gba_stack_irq_size sizeof(gbastck_irq)
#define gba_stack_irq_elemnt_size (sizeof(gbastck_irq[0]))

#define gba_stack_svc_size sizeof(gbastck_svc)
#define gba_stack_svc_elemnt_size (sizeof(gbastck_svc[0]))

#define gba_stack_abt_size sizeof(gbastck_abt)
#define gba_stack_abt_elemnt_size (sizeof(gbastck_abt[0]))

#define gba_stack_und_size sizeof(gbastck_und)
#define gba_stack_und_elemnt_size (sizeof(gbastck_und[0]))

#define gba_stack_sys_size sizeof(gbastck_sys)
#define gba_stack_sys_elemnt_size (sizeof(gbastck_sys[0]))


#define gba_branch_table_size sizeof(branch_stack)
#define gba_branch_block_size (int)((sizeof(branch_stack[0]))<<4)+(0x1*4) //17 elements 4 byte size each one
#define gba_branch_elemnt_size (sizeof(branch_stack[0])) //element size

//GBA stack
#define GBASTACKSIZE 0x400 //1K for now

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


/*
video ram notes:
All VRAM (and Palette, and OAM) can be written to only in 16bit and 32bit units 
(STRH, STR opcodes), 8bit writes are ignored (by STRB opcode). 
The only exception is "Plain <ARM7>-CPU Access" mode: The ARM7 CPU can use STRB to write 
to VRAM (the reason for this special feature is that, in GBA mode, two 128K VRAM blocks 
are used to emulate the GBA's 256K Work RAM).
*/
//#define internalRAM ((u8*)0x03000000)	//iram //allocated to ARM9 and served as Shared WRAM 03000000-03007FFF (IRAM)
//#define workRAM ((u8*)0x02000000)		//wram //02000000-0203FFFF WRAM - On-board Work RAM (256 KBytes) 2 Wait
//#define paletteRAM ((u8*)0x05000000)	//pallette //matches gba 05000000-050003FF   BG/OBJ Palette RAM  (1 Kbyte)
//#define vram ((u8*)0x06000000)			//vram //matches gba VRAM - Video RAM (96 KBytes)
//#define emuloam ((u8*)0x07000000) 		//oam emulated memory 
//gba 04000000-040003FE I/O, address patches occur on the VBAEMU core


#ifndef GBA_H
#define GBA_H

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION  SAVE_GAME_VERSION_10

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

//struct: map[index].address[(u8*)address]
typedef struct {
  u8 *address;
  u32 mask;
} memoryMap;

typedef union {
  struct {
#ifdef WORDS_BIGENDIAN
    u8 B3;
    u8 B2;
    u8 B1;
    u8 B0;
#else
    u8 B0;
    u8 B1;
    u8 B2;
    u8 B3;
#endif
  } B;
  struct {
#ifdef WORDS_BIGENDIAN
    u16 W1;
    u16 W0;
#else
    u16 W0;
    u16 W1;
#endif
  } W;
#ifdef WORDS_BIGENDIAN
  volatile u32 I;
#else
	u32 I;
#endif
} reg_pair;



#define R13_IRQ  18
#define R14_IRQ  19
#define SPSR_IRQ 20
#define R13_USR  26
#define R14_USR  27
#define R13_SVC  28
#define R14_SVC  29
#define SPSR_SVC 30
#define R13_ABT  31
#define R14_ABT  32
#define SPSR_ABT 33
#define R13_UND  34
#define R14_UND  35
#define SPSR_UND 36
#define R8_FIQ   37
#define R9_FIQ   38
#define R10_FIQ  39
#define R11_FIQ  40
#define R12_FIQ  41
#define R13_FIQ  42
#define R14_FIQ  43
#define SPSR_FIQ 44

struct GBASystem{
	//VERY IMPORTANT: These store a pointer to calloc'd different memory areas (array[0])
				//rom Program Counter; lives in supervisor.h (dtcm section)
	u8 *bios;	//bios;
	u8 *oam;	//oam;
	u8 *vidram; //vram;
	u8 *iomem;	//ioMem;
	u8 *intram;	//internalRAM;
	u8 *workram;//workRAM;
	u8 *palram;	//paletteRAM;
	u8 *caioMem;
	
    reg_pair reg[45];
    memoryMap map[256];
    bool ioreadable[0x400];

	//held on DTCM (translator.h)
	
    //bool N_FLAG;	
    //bool C_FLAG;
    //bool Z_FLAG;
    //bool V_FLAG;
	//bool I_FLAG;
	//bool F_FLAG;
	
    //bool armstate;	//0 disabled / 1 enabled
    //bool armirqstate; //0 disabled / 1 enabled
    //bool armswistate;
	
	 bool armirqenable;
	 
	int switicks; 		//some games may require swi or irq timings
    int irqticks;
	
    u32 stop;
    
	int savetype;
    bool usebios;
    bool skipbios;
    bool cpuismultiboot;
    
	int layersettings;
    int layerenable;
    
	int cpusavetype;
    bool cheatsenabled;
    bool mirroringEnable;
   
	bool skipsavegamebattery;
    bool skipsavegamecheats;

	u32 dummysrc;
	
    u16 GBADISPCNT;
    u16 GBADISPSTAT;
    u16 GBAVCOUNT;
    u16 GBABG0CNT;
    u16 GBABG1CNT;
    u16 GBABG2CNT;
    u16 GBABG3CNT;
    u16 GBABG0HOFS;
    u16 GBABG0VOFS;
    u16 GBABG1HOFS;
    u16 GBABG1VOFS;
    u16 GBABG2HOFS;
    u16 GBABG2VOFS;
    u16 GBABG3HOFS;
    u16 GBABG3VOFS;
    u16 GBABG2PA;
    u16 GBABG2PB;
    u16 GBABG2PC;
    u16 GBABG2PD;
    u16 GBABG2X_L;
    u16 GBABG2X_H;
    u16 GBABG2Y_L;
    u16 GBABG2Y_H;
    u16 GBABG3PA;
    u16 GBABG3PB;
    u16 GBABG3PC;
    u16 GBABG3PD;
    u16 GBABG3X_L;
    u16 GBABG3X_H;
    u16 GBABG3Y_L;
    u16 GBABG3Y_H;
    u16 GBAWIN0H;
    u16 GBAWIN1H;
    u16 GBAWIN0V;
    u16 GBAWIN1V;
    u16 GBAWININ;
    u16 GBAWINOUT;
    u16 GBAMOSAIC;
    u16 GBABLDMOD;
    u16 GBACOLEV;
    u16 GBACOLY;
    u16 GBADM0SAD_L;
    u16 GBADM0SAD_H;
    u16 GBADM0DAD_L;
    u16 GBADM0DAD_H;
    u16 GBADM0CNT_L;
    u16 GBADM0CNT_H;
    u16 GBADM1SAD_L;
    u16 GBADM1SAD_H;
    u16 GBADM1DAD_L;
    u16 GBADM1DAD_H;
    u16 GBADM1CNT_L;
    u16 GBADM1CNT_H;
    u16 GBADM2SAD_L;
    u16 GBADM2SAD_H;
    u16 GBADM2DAD_L;
    u16 GBADM2DAD_H;
    u16 GBADM2CNT_L;
    u16 GBADM2CNT_H;
    u16 GBADM3SAD_L;
    u16 GBADM3SAD_H;
    u16 GBADM3DAD_L;
    u16 GBADM3DAD_H;
    u16 GBADM3CNT_L;
    u16 GBADM3CNT_H;
    u16 GBATM0D;
    u16 GBATM0CNT;
    u16 GBATM1D;
    u16 GBATM1CNT;
    u16 GBATM2D;
    u16 GBATM2CNT;
    u16 GBATM3D;
    u16 GBATM3CNT;
    u16 GBAP1;
	
    //u16 GBAIE;	//replaced with iemasking
    //u16 GBAIF;	//replaced with ifmasking
    //u16 GBAIME;  //replaced with imemasking
	
	u8 cpustate;	//1 virtualizing / 0 halt
	
    int gfxbg2changed;
    int gfxbg3changed;
    int eeprominUse;

    u32 mastercode;
    int layerenabledelay;
    bool busprefetch;
    bool busprefetchenable;
    u32 busprefetchcount;
    
	bool intstate;  //between cpu state
    bool stopstate; //active or waitstate
    bool holdstate; //current waitstate status
    int holdtype;	//current wait state mode
	
	int cpuremainingdmaticks;
    int cpudmacount; //c -- every DMA access total count
    
	bool cpudmahack;
	u32 cpudmalast;
	
	int cpudmatickstoupdate;
	
    bool cpubreakloop;
    int cpunextevent;

    int clockticks;

    int gbasavetype;
    
	bool cpusramenabled;
    bool cpuflashenabled;
    bool cpueepromenabled;
    bool cpueepromsensorenabled;

    u32 cpuprefarm[2]; //ARM prefetch stores for ARM CPU 
	u16 cpuprefthmb[2]; //thumb prefetch stores for ARM CPU
	
	//int cputotalticks; //moved to DTCM because fast

    int lcdticks;
    
	u8 timeronoffdelay; // timerdeltadelay;
    u16 timer0value;
    bool timer0on;
    int timer0ticks;
    int timer0reload;
    int timer0clockreload;
    
	u16 timer1value;
    bool timer1on;
    int timer1ticks;
    int timer1reload;
    int timer1clockreload;
    u16 timer2value;
    bool timer2on;
    int timer2ticks;
    int timer2reload;
    int timer2clockreload;
    u16 timer3value;
    bool timer3on;
    int timer3ticks;
    int timer3reload;
    int timer3clockreload;
    u32 dma0source;
    u32 dma0dest;
    u32 dma1source;
    u32 dma1dest;
    u32 dma2source;
    u32 dma2dest;
    u32 dma3source;
    u32 dma3dest;

    bool fxon;
    bool windowon;
    int framecount;

    u32 lasttime;
    int count;

    int capture;
    int captureprevious;
    int capturenumber;

    u8 memorywait[16];
    u8 memorywait32[16];
    u8 memorywaitseq[16];
    u8 memorywaitseq32[16];

    u8 biosprotected[4];

    #ifdef WORDS_BIGENDIAN
    bool cpubioswwapped;
    #endif

    int romsize;

    // Sound settings
    bool sounddeclicking;
    long  soundsamplerate;

    u16   soundfinalwave [1600];
    bool  soundpaused;

    int   sound_clock_ticks;
    int   soundticks;

    float soundvolume;
    int soundenableflag;
    float soundfiltering;
    float soundVolume;
};

#endif // GBA_H
extern struct GBASystem GBASYS;
extern struct variable_desc savestr;

//LUT table for fast-seeking 32bit depth unsigned values
extern const  u8 minilut[0x10];

//slot 2 bus 
extern const u32  objtilesaddress [];
extern const u8  gamepakramwaitstate[];
extern const u8  gamepakwaitstate[];
extern const u8  gamepakwaitstate0[];
extern const u8  gamepakwaitstate1[];
extern const u8  gamepakwaitstate2[];

#ifdef __cplusplus
extern "C" {
#endif

//lookup calls
u8 lutu16bitcnt(u16 x);
u8 lutu32bitcnt(u32 x);

void initemu(struct GBASystem *gba);
int utilload(const char *file,u8 *data,int size,bool extram);
int loadrom(struct GBASystem * gba,const char *filename,bool extram);

extern int i;


int utilReadInt2(FILE *);
int utilReadInt3(FILE *);
void utilGetBaseName(const char *,char *);
void utilGetBasePath(const char *, char *);
int LengthFromString(const char *);
int VolumeFromString(const char *);
int unzip(char *, void *, uLong);
int utilLoad(const char *file,u8 *data,int size,bool extram);
int setregbasesize(u32, u8);
u32 bit(u32 val);

inline u16 swap16(u16 value);

int getphystacksz(u32 * curr_stack);

extern void initmemory(struct GBASystem * gba); //test memory & init
u32 * stack_test(u32 * branch_stackfp,int size, u8 testmode); //test GBA cpu stacks
u32 * updatestackfp(u32 * currstack_fp, u32 * stackbase);

u32 dummycall(u32 arg);

extern void __libnds_mpu_setup();


//lookup calls
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);

int utilload(const char *file,u8 *data,int size,bool extram);
int CPULoadRom(const char *szFile,bool extram);

int utilReadInt2(FILE *);
int utilReadInt3(FILE *);
void utilGetBaseName(const char *,char *);
void utilGetBasePath(const char *, char *);
int LengthFromString(const char *);
int VolumeFromString(const char *);
int unzip(char *, void *, uLong);
int utilLoad(const char *file,u8 *data,int size,bool extram);
int setregbasesize(u32, u8);
u32 bit(u32 val);

//more GBAVirt cpu opcodes
u16 swap16(u16 value);

//direct memory reads
void u32store(u32 address, u32 value);
void u16store(u32 address, u16 value);
void u8store(u32 address, u8 value);

u32 u32read(u32 address);
u16 u16read(u32 address);
u8 u8read(u32 address);

u32 READ32LE(u8 * addr);
u16 READ16LE(u8 * addr);

void WRITE32LE(u8 * x,u32 v);
void WRITE16LE(u8 * x,u16 v);

u8	clzero(u32);

extern u32 gba_entrypoint;

//debug
extern void isdebugemu_defined();
extern bool setarmstate(u32 psr);  //set arm state and cpsr bits from any psr / ret false:ARM | true:THUMB

extern int executeuntil_pc(u32 target_pc);


extern int ram2file_nds(char * fname,u8 * buffer,int size);

extern void CPUInit(const char *biosFileName, bool useBiosFile,bool extram);
extern int utilLoad(const char *file,u8 *data,int size,bool extram);

extern u32 gba_setup();

extern u32 UPDATE_REG(u32 address, u32 value);

#ifdef __cplusplus
}
#endif

#endif
