/*---------------------------------------------------------------------------------

	derived from the default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>
#include <nds/bios.h>
#include "arm7.h"
#include "opcode.h"
#include "gba_ipc.h"
#include "clock.h"

//Bit29-30  Format       (0=PCM8, 1=PCM16, 2=IMA-ADPCM, 3=PSG/Noise)
#define PCM8  0<<29 
#define PCM16 1<<29
#define ADPCM 2<<29
#define PSGNOISE 3<<29


/*---------------------------------------------------------------------------------*/

//---------------------------------------------------------------------------------
void HblankHandler(void) {
//---------------------------------------------------------------------------------
	
	//emulator heartbeat!
	SHARED_IPC->inst_globalemutick[0]=(Tgbacore_ticker*)(u32)var1;
	var1++;
}


//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
	
	// update clock tracking
	resyncClock();
	
	//own IPC clock transfer
	ipc_clock();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	readUserSettings();
    //ledBlink(0);

    irqInit();
	
	// Start the RTC tracking IRQ
    initClockIRQ(); //windwakr: Doesn't seem to work on 3DS.
    
    fifoInit();
	
	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();
	installSystemFIFO();

	irqSet(IRQ_HBLANK, HblankHandler);
	irqSet(IRQ_VBLANK, VblankHandler);
	irqSet(IRQ_VCOUNT, VcountHandler);
	
	irqEnable( IRQ_VBLANK | IRQ_HBLANK | IRQ_VCOUNT | IRQ_NETWORK);
	
	setPowerButtonCB(powerButtonCB);   
	
	//ARM7 vars init
	SHARED_IPC->inst_globalemutick[0]->gbacore_emuticker=0;
	
	// Keep the ARM7 mostly idle
	while (!exitflag) {
		swiIntrWait(1,IRQ_VBLANK | IRQ_HBLANK | IRQ_VCOUNT);
		//swiWaitForVBlank();
	}
	return 0;
}
