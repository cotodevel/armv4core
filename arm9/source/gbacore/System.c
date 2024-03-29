// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

//filesystem
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
#include "xenofunzip.h"
#include "armv4coreFS.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "System.h"

#define __DOUTBUFSIZE 256

char __outstr[__DOUTBUFSIZE];

// Required vars, used by the emulator core
//
int  systemRedShift;
int  systemGreenShift;
int  systemBlueShift;
int  systemColorDepth;
int  systemDebug;
int  systemVerbose;
int  systemSaveUpdateCounter;
int  systemFrameSkip;
bool systemSoundOn;

bool debugger;
int  RGB_LOW_BITS_MASK;

// Extra vars, only used for the GUI
//
int systemRenderedFrames;
int systemFPS;

// Sound stuff
//
/*const  int         iSoundSamples  = 2048;
const  int         iSoundTotalLen = iSoundSamples * 4;
static u8          auiSoundBuffer[iSoundTotalLen];
static int         iSoundLen;
static SDL_cond *  pstSoundCond;
static SDL_mutex * pstSoundMutex;

inline VBA::Window * GUI()
{
  return VBA::Window::poGetInstance();
}
*/

int len=0;
void systemMessage(int _iId, const char * _csFormat, ...){
	va_list args;

	va_start(args, _csFormat);
	len=vsnprintf(__outstr,__DOUTBUFSIZE,_csFormat,args);
	va_end(args);

	printf(__outstr);//GUI()->vPopupErrorV(_(_csFormat), args);

	va_end(args);
}

void systemUpdateMotionSensor(){
}

int systemGetSensorX(){
  return 0;
}

int systemGetSensorY(){
  return 0;
}
