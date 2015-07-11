
#ifndef SHARED_IPC_defs
#define SHARED_IPC_defs

#define SHAREDIPC_MAX_ARGS 4

__attribute__ ((aligned)) typedef struct _Tarm7commander{
	u32 command;
	
}Tarm7commander;

//Main ticking emulator [ARM7 drives this tick :) ]
__attribute__ ((aligned)) typedef struct _Tgbacore_ticker{
	u32 gbacore_emuticker;
}Tgbacore_ticker;


//because RTC on libnds acts weird
__attribute__ ((aligned)) typedef struct _Tarm7clock{
	//union {
	//inst[0]; //yy
	//inst[1]; //mm
	//inst[2]; //dd
	//inst[3]; //wat
	//inst[4]; //hh
	//inst[5]; //mm
	//inst[6]; //ss
	u32 clockdata; 
	//}un;
}Tarm7clock;


__attribute__ ((aligned)) typedef struct _Tshared_ipc {
	u8* pAudioProgram0;
	u32 audioProgramSize;
	u32 audioRomSize;
	u32 arm9FifoSent;
	u32 arm9FifoProcessed;
	u32 arm7FifoSent;
	u32 arm7FifoProcessed;
	u16* pAdpcmBuffer[7];
	
	Tarm7commander* arm7cmd_inst[7]; //commands from arm7 to arm9
	
	Tarm7clock * arm7clock_inst[7]; //clock
	
	Tgbacore_ticker * inst_globalemutick[1];
	
	s16 adpcmQueuePos7[7];
	s16 adpcmQueuePos9[7];
	u32 arm7Alive; //arm7 incs this every frame, helps arm9 know if arm7 crashed

	//for calling functions on other system
	u32 arm9Args[SHAREDIPC_MAX_ARGS];
	u32 arm9Return;

	u16 audioStreamCount; //inc by arm7 when time for audio to stream, dec by arm9 when audio actually streams
	
	u8 adpcmaFinished[7]; //set by arm9 when channel finished, read and cleared by arm7
	
	//u8 audioEnabled;
	u8 globalAudioEnabled;
	u8 audioCommand;
	u8 audioResult;
	u8 audioCommandPending;
	u32 misc; //used for random debugging things
} Tshared_ipc;

//regular IPC occurs at 0x027FF000, ld script reserves 4*KB,
//so we start ours 2*KB after
#define SHARED_ADDR 0x027FF400
#define SHARED_IPC ((volatile Tshared_ipc*)SHARED_ADDR)

#endif