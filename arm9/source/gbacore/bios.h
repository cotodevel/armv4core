#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>//BRK(); SBRK();
#include <string.h>

#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/memory.h>
//#include <nds/bios.h>
#include <nds/system.h>
#include "util.h"


#ifdef __cplusplus
extern "C"{
#endif

extern s16 sinetable[256];

extern void CPUReset();         //swi 0
extern void BIOS_RegisterRamReset(u32 flags);   //swi 1
extern void BIOS_ArcTan();
extern void BIOS_ArcTan2();
extern void BIOS_BitUnPack();
extern void BIOS_GetBiosChecksum();
extern void BIOS_BgAffineSet();
extern void BIOS_CpuSet();
extern void BIOS_CpuFastSet();
extern void BIOS_Diff8bitUnFilterWram();
extern void BIOS_Diff8bitUnFilterVram();
extern void BIOS_Diff16bitUnFilter();
extern void BIOS_Div();
extern void BIOS_DivARM();
extern void BIOS_HuffUnComp();
extern void BIOS_LZ77UnCompVram();
extern void BIOS_LZ77UnCompWram();
extern void BIOS_ObjAffineSet();
extern void BIOS_RLUnCompVram();
extern void BIOS_RLUnCompWram();
extern void BIOS_SoftReset();
extern void BIOS_Sqrt();
extern void BIOS_MidiKey2Freq();
extern void BIOS_SndDriverJmpTableCopy();

#ifdef __cplusplus
}
#endif