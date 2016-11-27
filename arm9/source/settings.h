//#define CUSTOMIRQ		//enable for custom IRQs rather than just swiWaitForVBlank and default FIFO driver

//#define ROMTEST 		//enabled:
						//a default, and light gbarom that is mapped to 0x08000000	(GBA Rom entrypoint)
						//disabled:
						//arm7tdmi payload support! allows to execute a small payload before jumping to 0x08000000 (GBA Rom entrypoint)
						
#define SBRKCACHEOFFSET 0x40000 //256K for sbrk

#define libdir

#define DEBUGEMU	//enables LR tracing / debugging by printf

//#define BIOSHANDLER		//Activate this to jump to BIOS and do handling there (can cause problems if bad bios or corrupted) / require a gba.bios at root
							//Deactivating this will BX LR as soon as swi code is executed

//#define NONDS9HANDLERS //disable: set vectors @ 0xffff0000 // enable : set vectors @0x00000000

//#define MPURECONFIG //enable: set dcache and mpu map / disable: as libnds provides default NDS MPU setup 
	//<-raises exceptions..
	//useful for enable: you want to detect wrong accesses, but breaks printf output..
	//so 	for disable: you want printf output regardless wrong accesses

//#define PREFETCH_ENABLED //Enables prefetch for the emulator. (cpu_emulate()) //if debugging or jumping to unknown areas disable this

//#define SPINLOCK_CODE //Enables own SPINLOCK library to run under HBLANK (locks down threads)

//MEMORY {
//       rom     : ORIGIN = 0x08000000, LENGTH = 32M
//       gbarom  : ORIGIN = 0x02180000, LENGTH = 3M -512k
//       gbaew   : ORIGIN = 0x02000000, LENGTH = 256k
//       ewram   : ORIGIN = 0x02040000, LENGTH = 1M - 256k + 512k
//       dtcm    : ORIGIN = 0x027C0000, LENGTH = 16K
//       vectors : ORIGIN = 0x01FF8000, LENGTH = 16K
//       itcm    : ORIGIN = 0x01FFC000, LENGTH = 16K
//}
