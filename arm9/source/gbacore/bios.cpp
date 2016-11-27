// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include <unistd.h>//BRK(); SBRK();
#include <fat.h>
#include <filesystem.h>
#include <dirent.h>

#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>

#include "../pu/supervisor.h"
#include "../pu/pu.h"
#include "gba.arm.core.h"
#include "opcode.h"
#include "util.h"
#include "translator.h"

#include "EEprom.h"
#include "Flash.h"
#include "Sram.h"
#include "System.h"
#include "RTC.h"

#include "bios.h"

s16 sinetable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};


//swi 0
void CPUReset()
{


  if(gbaSaveType == 0) {
    if(eepromInUse)
      gbaSaveType = 3;
    else
      switch(saveType) {
      case 1:
        gbaSaveType = 1;
        break;
      case 2:
        gbaSaveType = 2;
        break;
      }
  }
  rtcReset();


  // clean registers
  memset(&exRegs[0], 0, sizeof(exRegs));
  // clean OAM
  //memset(oam, 0, 0x400);
  // clean palette
  //memset(paletteRAM, 0, 0x400);
  // clean picture
  //memset(pix, 0, 4*160*240);
  // clean vram
  //memset(vram, 0, 0x20000);
  // clean io memory
  memset(ioMem, 0, 0x400);





  //DISPCNT  = 0x0000;
  DISPSTAT = 0x0000;
  VCOUNT   = (useBios && !skipBios) ? 0 :0x007E;
  BG0CNT   = 0x0000;
  BG1CNT   = 0x0000;
  BG2CNT   = 0x0000;
  BG3CNT   = 0x0000;
  BG0HOFS  = 0x0000;
  BG0VOFS  = 0x0000;
  BG1HOFS  = 0x0000;
  BG1VOFS  = 0x0000;
  BG2HOFS  = 0x0000;
  BG2VOFS  = 0x0000;
  BG3HOFS  = 0x0000;
  BG3VOFS  = 0x0000;
  BG2PA    = 0x0100;
  BG2PB    = 0x0000;
  BG2PC    = 0x0000;
  BG2PD    = 0x0100;
  BG2X_L   = 0x0000;
  BG2X_H   = 0x0000;
  BG2Y_L   = 0x0000;
  BG2Y_H   = 0x0000;
  BG3PA    = 0x0100;
  BG3PB    = 0x0000;
  BG3PC    = 0x0000;
  BG3PD    = 0x0100;
  BG3X_L   = 0x0000;
  BG3X_H   = 0x0000;
  BG3Y_L   = 0x0000;
  BG3Y_H   = 0x0000;
  WIN0H    = 0x0000;
  WIN1H    = 0x0000;
  WIN0V    = 0x0000;
  WIN1V    = 0x0000;
  WININ    = 0x0000;
  WINOUT   = 0x0000;
  MOSAIC   = 0x0000;
  BLDMOD   = 0x0000;
  COLEV    = 0x0000;
  COLY     = 0x0000;
  DM0SAD_L = 0x0000;
  DM0SAD_H = 0x0000;
  DM0DAD_L = 0x0000;
  DM0DAD_H = 0x0000;
  DM0CNT_L = 0x0000;
  DM0CNT_H = 0x0000;
  DM1SAD_L = 0x0000;
  DM1SAD_H = 0x0000;
  DM1DAD_L = 0x0000;
  DM1DAD_H = 0x0000;
  DM1CNT_L = 0x0000;
  DM1CNT_H = 0x0000;
  DM2SAD_L = 0x0000;
  DM2SAD_H = 0x0000;
  DM2DAD_L = 0x0000;
  DM2DAD_H = 0x0000;
  DM2CNT_L = 0x0000;
  DM2CNT_H = 0x0000;
  DM3SAD_L = 0x0000;
  DM3SAD_H = 0x0000;
  DM3DAD_L = 0x0000;
  DM3DAD_H = 0x0000;
  DM3CNT_L = 0x0000;
  DM3CNT_H = 0x0000;
  TM0D     = 0x0000;
  TM0CNT   = 0x0000;
  TM1D     = 0x0000;
  TM1CNT   = 0x0000;
  TM2D     = 0x0000;
  TM2CNT   = 0x0000;
  TM3D     = 0x0000;
  TM3CNT   = 0x0000;
  P1       = 0x03FF;
  IE       = 0x0000;
  IF       = 0x0000;
  IME      = 0x0000;

    //cpsrbits
  armMode = 0x1F;
  
  /*if(cpuIsMultiBoot) {
    reg[13].I = 0x03007F00;
    reg[15].I = 0x02000000;
    reg[16].I = 0x00000000;
    reg[R13_IRQ].I = 0x03007FA0;
    reg[R13_SVC].I = 0x03007FE0;
    armIrqEnable = true;
  } else {
    if(useBios && !skipBios) {
      reg[15].I = 0x00000000;
      armMode = 0x13;
      armIrqEnable = false;  
    } else {
      reg[13].I = 0x03007F00;
      reg[15].I = 0x08000000;
      reg[16].I = 0x00000000;
      reg[R13_IRQ].I = 0x03007FA0;
      reg[R13_SVC].I = 0x03007FE0;
      armIrqEnable = true;      
    }    
  }
  armState = true;
  C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;*/
  armState = true;
  UPDATE_REG(0x00, DISPCNT);
  UPDATE_REG(0x06, VCOUNT);
  UPDATE_REG(0x20, BG2PA);
  UPDATE_REG(0x26, BG2PD);
  UPDATE_REG(0x30, BG3PA);
  UPDATE_REG(0x36, BG3PD);
  UPDATE_REG(0x130, P1);
  UPDATE_REG(0x88, 0x200);

  // disable FIQ
  //reg[16].I |= 0x40;
  
  //CPUUpdateCPSR();
  
  //armNextPC = reg[15].I;
  //reg[15].I += 4;

  // reset internal state
  //holdState = false;
  //holdType = 0;
  
  biosProtected[0] = 0x00;
  biosProtected[1] = 0xf0;
  biosProtected[2] = 0x29;
  biosProtected[3] = 0xe1;
  
  lcdTicks = (useBios && !skipBios) ? 1008 : 208;
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
  cpuSaveGameFunc = flashSaveDecide;
  //renderLine = mode0RenderLine;
  fxOn = false;
  windowOn = false;
  frameCount = 0;
  saveType = 0;
  //layerEnable = DISPCNT & layerSettings;

  //CPUUpdateRenderBuffers(true);
  
  for(int i = 0; i < 256; i++) {
    map[i].address = (u8 *)&dummyAddress;
    map[i].mask = 0;
  }

  map[0].address = bios;
  map[0].mask = 0x3FFF;
  map[2].address = workRAM;
  map[2].mask = 0x3FFFF;
  map[3].address = internalRAM;
  map[3].mask = 0x7FFF;
  map[4].address = ioMem;
  map[4].mask = 0x3FF;
  map[5].address = paletteRAM;
  map[5].mask = 0x3FF;
  map[6].address = vram;
  map[6].mask = 0x1FFFF;
  map[7].address = oam;
  map[7].mask = 0x3FF;
  map[8].address = rom;
  map[8].mask = 0x1FFFFFF;
  map[9].address = rom;
  map[9].mask = 0x1FFFFFF;  
  map[10].address = rom;
  map[10].mask = 0x1FFFFFF;
  map[12].address = rom;
  map[12].mask = 0x1FFFFFF;
  map[14].address = flashSaveMemory;
  map[14].mask = 0xFFFF;

  eepromReset();
  flashReset();
  
  //soundReset(); //ichfly sound

  //CPUUpdateWindow0();
  //CPUUpdateWindow1();

  // make sure registers are correctly initialized if not using BIOS
  /*if(!useBios) {
    if(cpuIsMultiBoot)
      BIOS_RegisterRamReset(0xfe);
    else
      BIOS_RegisterRamReset(0xff);
  } else {
    if(cpuIsMultiBoot)
      BIOS_RegisterRamReset(0xfe);
  }*/ //ararar

  switch(cpuSaveType) {
  case 0: // automatic
    cpuSramEnabled = true;
    cpuFlashEnabled = true;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = false;
    saveType = gbaSaveType = 0;
    break;
  case 1: // EEPROM
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = false;
    saveType = gbaSaveType = 3;
    // EEPROM usage is automatically detected
    break;
  case 2: // SRAM
    cpuSramEnabled = true;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    cpuSaveGameFunc = sramDelayedWrite; // to insure we detect the write
    saveType = gbaSaveType = 1;
    break;
  case 3: // FLASH
    cpuSramEnabled = false;
    cpuFlashEnabled = true;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    cpuSaveGameFunc = flashDelayedWrite; // to insure we detect the write
    saveType = gbaSaveType = 2;
    break;
  case 4: // EEPROM+Sensor
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = true;
    // EEPROM usage is automatically detected
    saveType = gbaSaveType = 3;
    break;
  case 5: // NONE
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    // no save at all
    saveType = gbaSaveType = 5;
    break;
  } 

  //ARM_PREFETCH;

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

  cpuDmaHack = false;

  //lastTime = systemGetClock();

  updatecpuflags(1,cpsrvirt,armMode);
  
  //SWITicks = 0;
  rtcEnable(true); //coto: nds7 clock 
}

//swi 1
void BIOS_RegisterRamReset(u32 flags)
{
  // no need to trace here. this is only called directly from GBA.cpp
  // to emulate bios initialization

  CPUUpdateRegister(0x0, 0x80);
  if(flags) {
  
    if(flags & 0x01) {
      // clear work RAM
      memset(workRAM, 0, 0x40000);
    }
    if(flags & 0x02) {
      // clear internal RAM
      memset(internalRAM, 0, 0x7e00); // don't clear 0x7e00-0x7fff
    }
    if(flags & 0x04) {
      // clear palette RAM
      memset(paletteRAM, 0, 0x400);
    }
    if(flags & 0x08) {
      // clear VRAM
      memset(vram, 0, 0x18000);
    }
    if(flags & 0x10) {
      // clean OAM
      memset(oam, 0, 0x400);
    }

    if(flags & 0x80) {
      int i;
      for(i = 0; i < 0x10; i++)
        CPUUpdateRegister(0x200+i*2, 0);

      for(i = 0; i < 0xF; i++)
        CPUUpdateRegister(0x4+i*2, 0);

      for(i = 0; i < 0x20; i++)
        CPUUpdateRegister(0x20+i*2, 0);

      for(i = 0; i < 0x18; i++)
        CPUUpdateRegister(0xb0+i*2, 0);

      CPUUpdateRegister(0x130, 0);
      CPUUpdateRegister(0x20, 0x100);
      CPUUpdateRegister(0x30, 0x100);
      CPUUpdateRegister(0x26, 0x100);
      CPUUpdateRegister(0x36, 0x100);
    }
    
    if(flags & 0x20) {
      int i;
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x110+i*2, 0);
      CPUUpdateRegister(0x134, 0x8000);
      for(i = 0; i < 7; i++)
        CPUUpdateRegister(0x140+i*2, 0);
    }

    if(flags & 0x40) {
      int i;
      CPUWriteByte(0x4000084, 0);
      CPUWriteByte(0x4000084, 0x80);
      CPUWriteMemory(0x4000080, 0x880e0000);
      CPUUpdateRegister(0x88, CPUReadHalfWord(0x4000088)&0x3ff);
      CPUWriteByte(0x4000070, 0x70);
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x90+i*2, 0);
      CPUWriteByte(0x4000070, 0);
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x90+i*2, 0);
      CPUWriteByte(0x4000084, 0);
    }
  }
}


__attribute__ ((aligned (4)))	__attribute__((section(".dtcm")))
s16 sineTable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};

void BIOS_ArcTan()
{
#ifdef DEV_VERSION
    iprintf("ArcTan: %08x\n",
        exRegs[0]);
#endif

  s32 a =  -(((s32)(exRegs[0]*exRegs[0])) >> 14);
  s32 b = ((0xA9 * a) >> 14) + 0x390;
  b = ((b * a) >> 14) + 0x91C;
  b = ((b * a) >> 14) + 0xFB6;
  b = ((b * a) >> 14) + 0x16AA;
  b = ((b * a) >> 14) + 0x2081;
  b = ((b * a) >> 14) + 0x3651;
  b = ((b * a) >> 14) + 0xA2F9;
  a = ((s32)exRegs[0] * b) >> 16;
  exRegs[0] = a;

#ifdef DEV_VERSION
    iprintf("ArcTan: return=%08x\n",
        exRegs[0]);
#endif
}

void BIOS_ArcTan2()
{
#ifdef DEV_VERSION
    iprintf("ArcTan2: %08x,%08x \n",
        exRegs[0],
        exRegs[1]);
#endif
  
  s32 x = exRegs[0];
  s32 y = exRegs[1];
  u32 res = 0;
  if (y == 0) {
    res = ((x>>16) & 0x8000);
  } else {
    if (x == 0) {
      res = ((y>>16) & 0x8000) + 0x4000;
    } else {
		if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x<0) && (y<0))))) {
        exRegs[1] = x;
        exRegs[0] = y << 14;
        BIOS_Div();
        BIOS_ArcTan();
        if (x < 0)
          res = 0x8000 + exRegs[0];
        else
          res = (((y>>16) & 0x8000)<<1) + exRegs[0];
      } else {
        exRegs[0] = x << 14;
        BIOS_Div();
        BIOS_ArcTan();
        res = (0x4000 + ((y>>16) & 0x8000)) - exRegs[0];
      }
    }
  }
  exRegs[0] = res;
  
#ifdef DEV_VERSION
    iprintf("ArcTan2: return=%08x\n",
        exRegs[0]);
#endif
}  

void BIOS_BitUnPack()
{
#ifdef DEV_VERSION  
    iprintf("BitUnPack: %08x,%08x,%08x\n",
        exRegs[0],
        exRegs[1],
        exRegs[2]);
#endif

  u32 source = exRegs[0];
  u32 dest = exRegs[1];
  u32 header = exRegs[2];
  
  int len = CPUReadHalfWord(header);
    // check address
  if(((source & 0xe000000) == 0) ||
     ((source + len) & 0xe000000) == 0)
    return;

  int bits = CPUReadByte(header+2);
  int revbits = 8 - bits; 
  // u32 value = 0;
  u32 base = CPUReadMemory(header+4);
  bool addBase = (base & 0x80000000) ? true : false;
  base &= 0x7fffffff;
  int dataSize = CPUReadByte(header+3);

  int data = 0; 
  int bitwritecount = 0; 
  while(1) {
    len -= 1;
    if(len < 0)
      break;
    int mask = 0xff >> revbits; 
    u8 b = CPUReadByte(source); 
    source++;
    int bitcount = 0;
    while(1) {
      if(bitcount >= 8)
        break;
      u32 d = b & mask;
      u32 temp = d >> bitcount;
      if(d || addBase) {
        temp += base;
      }
      data |= temp << bitwritecount;
      bitwritecount += dataSize;
      if(bitwritecount >= 32) {
        CPUWriteMemory(dest, data);
        dest += 4;
        data = 0;
        bitwritecount = 0;
      }
      mask <<= bits;
      bitcount += bits;
    }
  }
}

void BIOS_GetBiosChecksum()
{
  exRegs[0]=0xBAAE187F;
}

void BIOS_BgAffineSet()
{
#ifdef DEV_VERSION  
    iprintf("BgAffineSet: %08x,%08x,%08x\n",
        exRegs[0],
        exRegs[1],
        exRegs[2]);
#endif
  
  u32 src = exRegs[0];
  u32 dest = exRegs[1];
  int num = exRegs[2];

  for(int i = 0; i < num; i++) {
    s32 cx = CPUReadMemory(src);
    src+=4;
    s32 cy = CPUReadMemory(src);
    src+=4;
    s16 dispx = CPUReadHalfWord(src);
    src+=2;
    s16 dispy = CPUReadHalfWord(src);
    src+=2;
    s16 rx = CPUReadHalfWord(src);
    src+=2;
    s16 ry = CPUReadHalfWord(src);
    src+=2;
    u16 theta = CPUReadHalfWord(src)>>8;
    src+=4; // keep structure alignment
    s32 a = sineTable[(theta+0x40)&255];
    s32 b = sineTable[theta];

    s16 dx =  (rx * a)>>14;
    s16 dmx = (rx * b)>>14;
    s16 dy =  (ry * b)>>14;
    s16 dmy = (ry * a)>>14;
    
    CPUWriteHalfWord(dest, dx);
    dest += 2;
    CPUWriteHalfWord(dest, -dmx);
    dest += 2;
    CPUWriteHalfWord(dest, dy);
    dest += 2;
    CPUWriteHalfWord(dest, dmy);
    dest += 2;

    s32 startx = cx - dx * dispx + dmx * dispy;
    s32 starty = cy - dy * dispx - dmy * dispy;
    
    CPUWriteMemory(dest, startx);
    dest += 4;
    CPUWriteMemory(dest, starty);
    dest += 4;
  }
}  

void BIOS_CpuSet()
{
#ifdef DEV_VERSION
    iprintf("CpuSet: 0x%08x,0x%08x,0x%08x\n", exRegs[0], exRegs[1],
        exRegs[2]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];
  u32 cnt = exRegs[2];

  if(((source & 0xe000000) == 0) ||
     ((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int count = cnt & 0x1FFFFF;

  // 32-bit ?
  if((cnt >> 26) & 1) {
    // needed for 32-bit mode!
    source &= 0xFFFFFFFC;
    dest &= 0xFFFFFFFC;
    // fill ?
    if((cnt >> 24) & 1) {
        u32 value = (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source));
      while(count) {
        CPUWriteMemory(dest, value);
        dest += 4;
        count--;
      }
    } else {
      // copy
      while(count) {
        CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source)));
        source += 4;
        dest += 4;
        count--;
      }
    }
  } else {
    // 16-bit fill?
    if((cnt >> 24) & 1) {
      u16 value = (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source));
      while(count) {
        CPUWriteHalfWord(dest, value);
        dest += 2;
        count--;
      }
    } else {
      // copy
      while(count) {
        CPUWriteHalfWord(dest, (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source)));
        source += 2;
        dest += 2;
        count--;
      }
    }
  }
}

void BIOS_CpuFastSet()
{
#ifdef DEV_VERSION
    iprintf("CpuFastSet: 0x%08x,0x%08x,0x%08x\n", exRegs[0], exRegs[1],
        exRegs[2]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];
  u32 cnt = exRegs[2];

  if(((source & 0xe000000) == 0) ||
     ((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
    return;

  // needed for 32-bit mode!
  source &= 0xFFFFFFFC;
  dest &= 0xFFFFFFFC;
  
  int count = cnt & 0x1FFFFF;
  
  // fill?
  if((cnt >> 24) & 1) {
    while(count > 0) {
      // BIOS always transfers 32 bytes at a time
      u32 value = (source>0x0EFFFFFF ? 0xBAFFFFFB : CPUReadMemory(source));
      for(int i = 0; i < 8; i++) {
        CPUWriteMemory(dest, value);
        dest += 4;
      }
      count -= 8;
    }
  } else {
    // copy
    while(count > 0) {
      // BIOS always transfers 32 bytes at a time
      for(int i = 0; i < 8; i++) {
        CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0xBAFFFFFB :CPUReadMemory(source)));
        source += 4;
        dest += 4;
      }
      count -= 8;
    }
  }
}

void BIOS_Diff8bitUnFilterWram()
{
#ifdef DEV_VERSION
    iprintf("Diff8bitUnFilterWram: 0x%08x,0x%08x\n", exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(	((source & 0xe000000) == 0) 
		
		||
    
		((((source + (header >> 8)) & 0x1fffff) & 0xe000000) == 0)
    )
	return;  

  int len = header >> 8;

  u8 data = CPUReadByte(source++);
  CPUWriteByte(dest++, data);
  len--;
  
  while(len > 0) {
    u8 diff = CPUReadByte(source++);
    data += diff;
    CPUWriteByte(dest++, data);
    len--;
  }    
}

void BIOS_Diff8bitUnFilterVram()
{
#ifdef DEV_VERSION
    iprintf("Diff8bitUnFilterVram: 0x%08x,0x%08x\n", exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  int len = header >> 8;

  u8 data = CPUReadByte(source++);
  u16 writeData = data;
  int shift = 8;
  int bytes = 1;
  
  while(len >= 2) {
    u8 diff = CPUReadByte(source++);
    data += diff;
    writeData |= (data << shift);
    bytes++;
    shift += 8;
    if(bytes == 2) {
      CPUWriteHalfWord(dest, writeData);
      dest += 2;
      len -= 2;
      bytes = 0;
      writeData = 0;
      shift = 0;
    }
  }  
}

void BIOS_Diff16bitUnFilter()
{
#ifdef DEV_VERSION
    iprintf("Diff16bitUnFilter: 0x%08x,0x%08x\n", exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  int len = header >> 8;

  u16 data = CPUReadHalfWord(source);
  source += 2;
  CPUWriteHalfWord(dest, data);
  dest += 2;
  len -= 2;
  
  while(len >= 2) {
    u16 diff = CPUReadHalfWord(source);
    source += 2;
    data += diff;
    CPUWriteHalfWord(dest, data);
    dest += 2;
    len -= 2;
  }
}

void BIOS_Div()
{
#ifdef DEV_VERSION
    iprintf("Div: 0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1]);
#endif
  
  int number = exRegs[0];
  int denom = exRegs[1];

  if(denom != 0) {
    exRegs[0] = number / denom;
    exRegs[1] = number % denom;
    s32 temp = (s32)exRegs[0];
    exRegs[3] = temp < 0 ? (u32)-temp : (u32)temp;
  }
#ifdef DEV_VERSION
    iprintf("Div: return=0x%08x,0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1],
        exRegs[3]);
#endif
}

void BIOS_DivARM()
{
#ifdef DEV_VERSION
    iprintf("DivARM: 0x%08x\n",
        exRegs[0]);
#endif
  
  u32 temp = exRegs[0];
  exRegs[0] = exRegs[1];
  exRegs[1] = temp;
  BIOS_Div();
}

void BIOS_HuffUnComp()
{
#ifdef DEV_VERSION
    iprintf("HuffUnComp: 0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  u8 treeSize = CPUReadByte(source++);

  u32 treeStart = source;

  source += ((treeSize+1)<<1)-1; // minus because we already skipped one byte
  
  int len = header >> 8;

  u32 mask = 0x80000000;
  u32 data = CPUReadMemory(source);
  source += 4;

  int pos = 0;
  u8 rootNode = CPUReadByte(treeStart);
  u8 currentNode = rootNode;
  bool writeData = false;
  int byteShift = 0;
  int byteCount = 0;
  u32 writeValue = 0;

  if((header & 0x0F) == 8) {
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);
      
      if(data & mask) {
        // right
        if(currentNode & 0x40)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos);
      }
      
      if(writeData) {
        writeValue |= (currentNode << byteShift);
        byteCount++;
        byteShift += 8;

        pos = 0;
        currentNode = rootNode;
        writeData = false;

        if(byteCount == 4) {
          byteCount = 0;
          byteShift = 0;
          CPUWriteMemory(dest, writeValue);
          writeValue = 0;
          dest += 4;
          len -= 4;
        }
      }
      mask >>= 1;
      if(mask == 0) {
        mask = 0x80000000;
        data = CPUReadMemory(source);
        source += 4;
      }
    }
  } else {
    int halfLen = 0;
    int value = 0;
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);

      if((data & mask)) {
        // right
        if(currentNode & 0x40)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos);
      }
      
      if(writeData) {
        if(halfLen == 0)
          value |= currentNode;
        else
          value |= (currentNode<<4);

        halfLen += 4;
        if(halfLen == 8) {
          writeValue |= (value << byteShift);
          byteCount++;
          byteShift += 8;
          
          halfLen = 0;
          value = 0;

          if(byteCount == 4) {
            byteCount = 0;
            byteShift = 0;
            CPUWriteMemory(dest, writeValue);
            dest += 4;
            writeValue = 0;
            len -= 4;
          }
        }
        pos = 0;
        currentNode = rootNode;
        writeData = false;
      }
      mask >>= 1;
      if(mask == 0) {
        mask = 0x80000000;
        data = CPUReadMemory(source);
        source += 4;
      }
    }    
  }
}

void BIOS_LZ77UnCompVram()
{
#ifdef DEV_VERSION
    iprintf("LZ77UnCompVram: 0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;    
  
  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);

    if(d) {
      for(int i = 0; i < 8; i++) {
        if(d & 0x80) {
          u16 data = CPUReadByte(source++) << 8;
          data |= CPUReadByte(source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest + byteCount - offset - 1;
          for(int i = 0; i < length; i++) {
            writeValue |= (CPUReadByte(windowOffset++) << byteShift);
            byteShift += 8;
            byteCount++;

            if(byteCount == 2) {
              CPUWriteHalfWord(dest, writeValue);
              dest += 2;
              byteCount = 0;
              byteShift = 0;
              writeValue = 0;
            }
            len--;
            if(len == 0)
              return;
          }
        } else {
          writeValue |= (CPUReadByte(source++) << byteShift);
          byteShift += 8;
          byteCount++;
          if(byteCount == 2) {
            CPUWriteHalfWord(dest, writeValue);
            dest += 2;
            byteCount = 0;
            byteShift = 0;
            writeValue = 0;
          }
          len--;
          if(len == 0)
            return;
        }
        d <<= 1;
      }
    } else {
      for(int i = 0; i < 8; i++) {
        writeValue |= (CPUReadByte(source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;      
          byteShift = 0;
          byteCount = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_LZ77UnCompWram()
{
#ifdef DEV_VERSION
    iprintf("LZ77UnCompWram: 0x%08x,0x%08x\n", exRegs[0], exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);

    if(d) {
      for(int i = 0; i < 8; i++) {
        if(d & 0x80) {
          u16 data = CPUReadByte(source++) << 8;
          data |= CPUReadByte(source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest - offset - 1;
          for(int i = 0; i < length; i++) {
            CPUWriteByte(dest++, CPUReadByte(windowOffset++));
            len--;
            if(len == 0)
              return;
          }
        } else {
          CPUWriteByte(dest++, CPUReadByte(source++));
          len--;
          if(len == 0)
            return;
        }
        d <<= 1;
      }
    } else {
      for(int i = 0; i < 8; i++) {
        CPUWriteByte(dest++, CPUReadByte(source++));
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_ObjAffineSet()
{
#ifdef DEV_VERSION
    iprintf("ObjAffineSet: 0x%08x,0x%08x,0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1],
        exRegs[2],
        exRegs[3]);
#endif
  
  u32 src = exRegs[0];
  u32 dest = exRegs[1];
  int num = exRegs[2];
  int offset = exRegs[3];

  for(int i = 0; i < num; i++) {
    s16 rx = CPUReadHalfWord(src);
    src+=2;
    s16 ry = CPUReadHalfWord(src);
    src+=2;
    u16 theta = CPUReadHalfWord(src)>>8;
    src+=4; // keep structure alignment

    s32 a = (s32)sineTable[(theta+0x40)&255];
    s32 b = (s32)sineTable[theta];

    s16 dx =  ((s32)rx * a)>>14;
    s16 dmx = ((s32)rx * b)>>14;
    s16 dy =  ((s32)ry * b)>>14;
    s16 dmy = ((s32)ry * a)>>14;
    
    CPUWriteHalfWord(dest, dx);
    dest += offset;
    CPUWriteHalfWord(dest, -dmx);
    dest += offset;
    CPUWriteHalfWord(dest, dy);
    dest += offset;
    CPUWriteHalfWord(dest, dmy);
    dest += offset;
  }
}


void BIOS_RLUnCompVram()
{
#ifdef DEV_VERSION
    iprintf("RLUnCompVram: 0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source & 0xFFFFFFFC);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  int len = header >> 8;
  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;

  while(len > 0) {
    u8 d = CPUReadByte(source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = CPUReadByte(source++);
      l += 3;
      for(int i = 0;i < l; i++) {
        writeValue |= (data << byteShift);
        byteShift += 8;
        byteCount++;

        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    } else {
      l++;
      for(int i = 0; i < l; i++) {
        writeValue |= (CPUReadByte(source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_RLUnCompWram()
{
#ifdef DEV_VERSION
    iprintf("RLUnCompWram: 0x%08x,0x%08x\n",
        exRegs[0],
        exRegs[1]);
#endif
  
  u32 source = exRegs[0];
  u32 dest = exRegs[1];

  u32 header = CPUReadMemory(source & 0xFFFFFFFC);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;  
  
  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = CPUReadByte(source++);
      l += 3;
      for(int i = 0;i < l; i++) {
        CPUWriteByte(dest++, data);
        len--;
        if(len == 0)
          return;
      }
    } else {
      l++;
      for(int i = 0; i < l; i++) {
        CPUWriteByte(dest++,  CPUReadByte(source++));
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_SoftReset()
{
#ifdef DEV_VERSION
    iprintf("SoftReset\n");
#endif

  armState = true;
  armMode = 0x1F;
  armIrqEnable = false;
  C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
  
  
  //Set CPSR virtualized bits & perform USR/SYS CPU mode change. & set stacks
  //exRegs[16] = 0x00000000;        //cpsr
  cpsrvirt = 0;
  
  updatecpuflags(1,cpsrvirt,PSR_USR);
  exRegs[13] = 0x03007F00;
  exRegs[14] = 0x00000000;
  
  updatecpuflags(1,cpsrvirt,PSR_IRQ);
  exRegs[13] = 0x03007FA0;
  exRegs[14] = 0x00000000;
  spsr_irq = 0;
  
  updatecpuflags(1,cpsrvirt,PSR_SVC);
  exRegs[13] = 0x03007FE0;  
  exRegs[14] = 0x00000000;
  spsr_irq = 0;
  
  
  u8 b = internalRAM[0x7ffa];
  memset(&internalRAM[0x7e00], 0, 0x200);

  if(b) {
    armNextPC = 0x02000000;
    exRegs[15] = 0x02000004;
  } else {
    armNextPC = 0x08000000;
    exRegs[15] = 0x08000004;
  }
}

void BIOS_Sqrt()
{
#ifdef DEV_VERSION
    iprintf("Sqrt: %08x\n",
        exRegs[0]);
#endif
  exRegs[0] = (u32)sqrt((double)exRegs[0]);
#ifdef DEV_VERSION
    iprintf("Sqrt: return=%08x\n",
        exRegs[0]);
#endif
}

void BIOS_MidiKey2Freq()
{
#ifdef DEV_VERSION
    iprintf("MidiKey2Freq: WaveData=%08x mk=%08x fp=%08x\n",
        exRegs[0],
        exRegs[1],
        exRegs[2]);
#endif
  int freq = CPUReadMemory(exRegs[0]+4);
  double tmp;
  tmp = ((double)(180 - exRegs[1])) - ((double)exRegs[2] / 256.f);
  tmp = pow((double)2.f, tmp / 12.f);
  exRegs[0] = (int)((double)freq / tmp);

#ifdef DEV_VERSION
    iprintf("MidiKey2Freq: return %08x\n",
        exRegs[0]);
#endif
}

void BIOS_SndDriverJmpTableCopy()
{
#ifdef DEV_VERSION
    iprintf("SndDriverJmpTableCopy: dest=%08x\n",
        exRegs[0]);
#endif
  for(int i = 0; i < 0x24; i++) {
    CPUWriteMemory(exRegs[0], 0x9c);
    exRegs[0] += 4;
  }
}

