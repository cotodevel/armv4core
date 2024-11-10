/*

			Copyright (C) 2017  Coto
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
USA
*/

//This file abstracts specific TGDS memory allocator code which allows for either default malloc or custom implementation malloc, by overriding this source code.

#include "posixHandleTGDS.h"
#include "xmem.h"
#include "dldi.h"
#include "dsregs.h"

////////[For custom Memory Allocator implementation]:////////
//You need to override getProjectSpecificMemoryAllocatorSetup():
//After that, TGDS project initializes the default/custom allocator automatically.


	////////[Custom Memory implementation: [NTR 4MB] [TWL 16MB] EWRAM malloc support ]////////

//Definition that overrides the weaksymbol expected from toolchain to init ARM9's TGDS memory allocation
#if (defined(__GNUC__) && !defined(__clang__))
__attribute__((optimize("O0")))
#endif

#if (!defined(__GNUC__) && defined(__clang__))
__attribute__ ((optnone))
#endif
struct AllocatorInstance * getProjectSpecificMemoryAllocatorSetup(bool isCustomTGDSMalloc) {
	struct AllocatorInstance * customMemoryAllocator = &CustomAllocatorInstance;
	memset((u8*)customMemoryAllocator, 0, sizeof(CustomAllocatorInstance));
	customMemoryAllocator->customMalloc = isCustomTGDSMalloc;
	
	if(__dsimode == true){
		//Enable 16M EWRAM (TWL) Malloc
		u32 SFGEXT9 = *(u32*)0x04004008;
		//14-15 Main Memory RAM Limit (0..1=4MB/DS, 2=16MB/DSi, 3=32MB/DSiDebugger)
		SFGEXT9 = (SFGEXT9 & ~(0x3 << 14)) | (0x2 << 14);
		*(u32*)0x04004008 = SFGEXT9;
		
		//We need to tell the dmalloc system we have 16M (TWL) RAM now
		physical_ewram_end = (u32)0x03000000;
	}
	else{
		//Otherwise Enable 4M EWRAM (NTR) Malloc
	}
	
	customMemoryAllocator->ARM9MallocStartaddress = (u32)sbrk(0);
	
	if ((int)customMemoryAllocator->ARM9MallocStartaddress == (int)-1){
		printf("TGDSAPP:Exit(); sbrk() has ran out of memory");
		while(1==1){}
	}
	if(__dsimode == true){
		customMemoryAllocator->memoryToAllocate = (1024*1024*14) + (768*1024);
	}
	else{
		customMemoryAllocator->memoryToAllocate = (1000*1024);
	}
	
	customMemoryAllocator->CustomTGDSMalloc9 = (TGDSARM9MallocHandler)&Xmalloc;
	customMemoryAllocator->CustomTGDSCalloc9 = (TGDSARM9CallocHandler)&Xcalloc;
	customMemoryAllocator->CustomTGDSFree9 = (TGDSARM9FreeHandler)&Xfree;
	customMemoryAllocator->CustomTGDSMallocFreeMemory9 = (TGDSARM9MallocFreeMemoryHandler)&XMEM_FreeMem;
	
	//Init XMEM (let's see how good this one behaves...)
	u32 xmemsize = XMEMTOTALSIZE = customMemoryAllocator->memoryToAllocate;
	xmemsize = xmemsize - (xmemsize/XMEM_BS) - 1024;
	xmemsize = xmemsize - (xmemsize%1024);
	XmemSetup(xmemsize, XMEM_BS);
	XmemInit(customMemoryAllocator->ARM9MallocStartaddress, (u32)customMemoryAllocator->memoryToAllocate);
	
	//Memory Setup: ARM7 TGDS 96K = 0x037f8000 ~ 0x03810000. TGDS Sound Streaming code: Disabled
	WRAM_CR = WRAM_0KARM9_32KARM7;
	return customMemoryAllocator;
}
