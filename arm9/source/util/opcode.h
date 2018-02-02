
#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

//fifo & other opcode parts
#ifndef OPCODE_PARTS

#define OPCODE_PARTS
//FIFO struct
struct fifo_semaphore{
u32 REG_FIFO_SENDEMPTY_STAT; 	//0<<0
u32 REG_FIFO_SENDFULL_STAT; 	//0<<1
u32 REG_FIFO_SENDEMPTY_IRQ; 	//0<<2
u32 REG_FIFO_SENDEMPTY_CLR; 	//0<<3
u32 REG_FIFO_RECVEMPTY; 		//0<<8
u32 REG_FIFO_RECVFULL; 			//0<<9
u32 REG_FIFO_RECVNOTEMPTY_IRQ; 	//0<<10
u32 REG_FIFO_ERRSENDRECVEMPTYFULL; //0<<14
u32 REG_FIFO_ENABLESENDRECV; 	//0<<15
};

#endif


#ifdef __cplusplus
extern "C"{
#endif

extern struct fifo_semaphore FIFO_SEMAPHORE_FLAGS;

//fifo
extern u32 buffer_input[0x10];
extern u32 buffer_output[0x10];

//IPC
void ipcidle();
u32 sendwordipc(uint8 word);
u32 recvwordipc();


u8	clzero(u32);
u32 nopinlasm();

#ifdef __cplusplus
}
#endif