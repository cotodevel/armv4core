#include <nds.h>
#include <fat.h>

#include "fatmore.h"
#include "fatfile.h"
#include "file_allocation_table.h"
#include "disc.h"
#include "partition.h"

#include "../settings.h"
#include "../gbacore/gba.arm.core.h"
#include "../rom/puzzle_original.h"

//gbaemu4ds file stream code

FILE* ichflyfilestream;
volatile int ichflyfilestreamsize=0;


volatile u32 *sectortabel;
void * lastopen;
void * lastopenlocked;

PARTITION* partitionlocked;
FN_MEDIUM_READSECTORS	readSectorslocked;
u32 current_pointer = 0;
u32 allocedfild[buffslots];
u8* greatownfilebuffer;


__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u8 ichfly_readu8extern(int pos){
return ichfly_readu8(pos); //ichfly_readu8(pos);
}

__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u16 ichfly_readu16extern(int pos){
return ichfly_readu16(pos); //ichfly_readu16(pos);
}

__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u32 ichfly_readu32extern(int pos){
return ichfly_readu32(pos); //ichfly_readu32(pos);
}

//gbaemu4ds ichfly stream code

__attribute__ ((hot))
__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u8 ichfly_readu8(int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	int sectoroffset = pos % sectorsize;
	int mappoffset = pos / sectorsize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2 + 1]);
	
	if(asd != (u8*)0x0){
		return asd[sectoroffset]; //found exit here
	}
	else{
		sectortabel[allocedfild[current_pointer]] = 0x0; //reset

		allocedfild[current_pointer] = mappoffset*2 + 1; //set new slot
		asd = greatownfilebuffer + current_pointer * sectorsize;
		sectortabel[mappoffset*2 + 1] = (u32)asd;

		readSectorslocked(sectortabel[mappoffset*2], sectorscale, asd);
		current_pointer++;
		if(current_pointer == buffslots)
			current_pointer = 0;
	}
	return (asd[sectoroffset]);
}

__attribute__ ((hot))
__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u16 ichfly_readu16(int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	int sectoroffset = pos % sectorsize;
	int mappoffset = pos / sectorsize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2 + 1]);
	
	if(asd != (u8*)0x0){
		return *(u16*)(&asd[sectoroffset]); //found exit here
	}
	else{
		sectortabel[allocedfild[current_pointer]] = 0x0; //clear old slot

		allocedfild[current_pointer] = mappoffset*2 + 1; //set new slot
		asd = greatownfilebuffer + current_pointer * sectorsize;
		sectortabel[mappoffset*2 + 1] = (u32)asd;
	
		readSectorslocked(sectortabel[mappoffset*2], sectorscale, asd);

		current_pointer++;
		if(current_pointer == buffslots)
			current_pointer = 0;
	}
	return *(u16*)(&asd[sectoroffset]);
}
__attribute__ ((hot))
__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
u32 ichfly_readu32(int pos) //need lockup
{
	// Calculate the sector and byte of the current position,
	// and store them
	int sectoroffset = pos % sectorsize;
	int mappoffset = pos / sectorsize;
	
	u8* asd = (u8*)(sectortabel[mappoffset*2 + 1]);
	
	if(asd != (u8*)0x0){
		return *(u32*)(&asd[sectoroffset]); //found exit here
	}
	else{
		sectortabel[allocedfild[current_pointer]] = 0x0;

		allocedfild[current_pointer] = mappoffset*2 + 1; //set new slot
		asd = greatownfilebuffer + current_pointer * sectorsize;
		sectortabel[mappoffset*2 + 1] = (u32)asd;
	
		readSectorslocked(sectortabel[mappoffset*2], sectorscale, asd);
		current_pointer++;
		if(current_pointer == buffslots)
			current_pointer = 0;
	}
	return *(u32*)(&asd[sectoroffset]);
}

__attribute__ ((hot))
__attribute__((section(".itcm")))
__attribute__ ((aligned (4)))
void ichfly_readdma_rom(u32 pos,u8 *ptr,u32 c,int readal) //need lockup only alined is not working 
{
	// Calculate the sector and byte of the current position,
	// and store them
	int sectoroffset = 0;
	int mappoffset = 0;
	int currsize = 0;

	if(readal == 4) //32 Bit
	{
		while(c > 0)
		{
			mappoffset = pos / sectorsize;
			sectoroffset = (pos % sectorsize) /4;
			currsize = (sectorsize / 4) - sectoroffset;
			if(currsize == 0)
				currsize = sectorsize / 4;
			if(currsize > c) 
				currsize = c;
				
			u32* asd = (u32*)(sectortabel[mappoffset*2 + 1]);
			
			if(asd != (u32*)0x0)//found exit here (block has data)
			{
				int i = 0; //copy
				while(currsize > i)
				{
					*(u32*)(&ptr[i*4]) = asd[sectoroffset + i];
					i++;
				}
				
				c -= currsize;
				pos += (currsize * 4);
				ptr += (currsize * 4);
				//continue;
			}
			else{
				sectortabel[allocedfild[current_pointer]] = 0x0;
				allocedfild[current_pointer] = mappoffset*2 + 1; //set new slot
				asd = (u32*)(greatownfilebuffer + current_pointer * sectorsize);
				sectortabel[mappoffset*2 + 1] = (u32)asd;
				readSectorslocked(sectortabel[mappoffset*2], sectorscale, asd);
				current_pointer++;
				if(current_pointer == buffslots)
					current_pointer = 0;
				
                int i = 0; //copy
				while(currsize > i)
				{
					*(u32*)(&ptr[i*4]) = asd[sectoroffset + i];
					i++;
				}
				
				c -= currsize;
				pos += (currsize * 4);
				ptr += (currsize * 4);
			}
		}
	}
	else //16 Bit
	{
		while(c > 0)
		{
			sectoroffset = (pos % sectorsize) / 2;
			mappoffset = pos / sectorsize;
			currsize = (sectorsize / 2) - sectoroffset;
			if(currsize == 0)currsize = sectorsize / 2;
			if(currsize > c) currsize = c;

			u16* asd = (u16*)(sectortabel[mappoffset*2 + 1]);
			//iprintf("%X %X %X %X %X %X\n\r",sectoroffset,mappoffset,currsize,pos,c,sectorsize);
			if(asd != (u16*)0x0)//found exit here
			{
				//ori
				int i = 0; //copy
				while(currsize > i)
				{
					*(u16*)(&ptr[i*2]) = asd[sectoroffset + i];
					i++;
				}
				
				c -= currsize;
				ptr += (currsize * 2);
				pos += (currsize * 2);
				//continue;
			}
			else{
				sectortabel[allocedfild[current_pointer]] = 0x0;
				allocedfild[current_pointer] = mappoffset*2 + 1; //set new slot
				asd = (u16*)(greatownfilebuffer + current_pointer * sectorsize);
				sectortabel[mappoffset*2 + 1] = (u32)asd;
				
				readSectorslocked(sectortabel[mappoffset*2], sectorscale, asd);
				current_pointer++;
				if(current_pointer == buffslots)current_pointer = 0;
				
                int i = 0; //copy
				while(currsize > i)
				{
					*(u16*)(&ptr[i*2]) = asd[sectoroffset + i];
					i++;
				}
				
				c -= currsize;
				ptr += (currsize * 2);
				pos += (currsize * 2);
			}
		}
	}
}


void generatefilemap(int size)
{
	FILE_STRUCT* file = (FILE_STRUCT*)(lastopen);
	lastopenlocked = lastopen; //copy
	PARTITION* partition;
	uint32_t cluster;
	int clusCount;
	partition = file->partition;
	partitionlocked = partition;

	readSectorslocked = file->partition->disc->readSectors;
	iprintf("generating file map (size %d Byte)",((size/sectorsize) + 1)*8);
	sectortabel =(u32*)malloc(((size/sectorsize) + 1)*8); //alloc for size every Sector has one u32
	greatownfilebuffer =(u8*)malloc(sectorsize * buffslots);

	clusCount = size/partition->bytesPerCluster;
	cluster = file->startCluster;

	//setblanc
	int i = 0;
	while(i < (partition->bytesPerCluster/sectorsize)*clusCount+1)
	{
		sectortabel[i*2 + 1] = 0x0;
		i++;
	}
	i = 0;
	while(i < buffslots)
	{
		allocedfild[i] = 0x0;
		i++;
	}

	int mappoffset = 0;
	i = 0;
	while(i < (partition->bytesPerCluster/sectorsize))
	{
		sectortabel[mappoffset*2] = _FAT_fat_clusterToSector(partition, cluster) + i;
		
		//debugging (fat fs sector numbers of image rom) 
		//iprintf("(%d)[%x]",(int)i,(unsigned int)_FAT_fat_clusterToSector(partition, cluster) + i);
		
		mappoffset++;
		i++;
	}
	while (clusCount > 0) {
		clusCount--;
		cluster = _FAT_fat_nextCluster (partition, cluster);

		i = 0;
		while(i < (partition->bytesPerCluster/sectorsize))
		{
			sectortabel[mappoffset*2] = _FAT_fat_clusterToSector(partition, cluster) + i;
			mappoffset++;
			i++;
		}
	}

}


void getandpatchmap(u32 offsetgba,u32 offsetthisfile)
{
	FILE_STRUCT* file = (FILE_STRUCT*)(lastopen);
	PARTITION* partition;
	uint32_t cluster;
	int clusCount;
	partition = file->partition;

	clusCount = offsetthisfile/partition->bytesPerCluster;
	cluster = file->startCluster;

	int offset1 = (offsetthisfile/sectorsize) % partition->bytesPerCluster;

	int mappoffset = offsetthisfile/sectorsize;
	while (clusCount > 0) {
		clusCount--;
		cluster = _FAT_fat_nextCluster (partition, cluster);
	}
	sectortabel[mappoffset*2] = _FAT_fat_clusterToSector(partition, cluster) + offset1;
}

//ichfly end

//////////////////////////////////////////////cache system (streamed code)
//[nds] disk sector ram buffer
volatile u32 disk_buf32[(strm_buf_size)]; 	//32 reads //0x80
volatile u16 disk_buf16[(strm_buf_size)];	//16 reads
volatile u8 disk_buf8[(strm_buf_size)];		//16 reads

int startoffset32=0;	//start offset for disk_buf32 ,from gbabuffer fetch
int startoffset16=0;	//start offset for disk_buf32 ,from gbabuffer fetch
int startoffset8=0;	//start offset for disk_buf32 ,from gbabuffer fetch


int first32krom_size = sizeof(first32krom);
u8 first32krom[1024*32];

//reads: 0x08000000 - 0x09ffffff
u8 cache_read8(int offset){

#ifndef ROMTEST
	if(offset < first32krom_size){
        return *(u8*)&first32krom[offset];
    }
    else if 	( 	(offset < (int)(startoffset8+strm_buf_size)) //starts from zero, so startoffset+sectorsize is next element actually((n-1)+1).
			&&
			(offset	>=	startoffset8)
		){
		return disk_buf8[ ((offset - startoffset8) / u8size) ]; //OK
	}
	else{
		
        int i = 0;
        for (i = 0; i < strm_buf_size; i++){
            disk_buf8[i] = ichfly_readu8((u32)((offset&0x1ffffff) + i));
		}
        startoffset8=offset;
		return disk_buf8[0]; //OK (must be zero since element 0 of the newly filled gbarom buffer has offset contents)
	}
return 0;

#else
return *(u8*)&puzzle_original[offset];
#endif
}

u16 cache_read16(int offset){

#ifndef ROMTEST

    if(offset < first32krom_size){
        return *(u16*)&first32krom[offset];
    }
	else if 	( 	(offset < (int)(startoffset16+strm_buf_size)) //starts from zero, so startoffset+sectorsize is next element actually((n-1)+1).
			&&
			(offset	>=	startoffset16)
		){
		return disk_buf16[ ((offset - startoffset16) / u16size) ]; //OK
	}
	else{
        ichfly_readdma_rom((u32)(offset&0x1ffffff),(u8*)&disk_buf16[0],strm_buf_size,16);
		/*
        int i = 0;
        for (i = 0; i < sectorsize/2; i++){
            disk_buf16[i] = ichfly_readu16((u32)((offset&0x1ffffff) + (i*2)));
		}
        */
        startoffset16=offset;
		return disk_buf16[0]; //OK (must be zero since element 0 of the newly filled gbarom buffer has offset_start)
	}
return 0;
#else
return *(u16*)&puzzle_original[offset];
#endif

}


u32 cache_read32(int offset){
#ifndef ROMTEST
    if(offset < first32krom_size){
        return *(u32*)&first32krom[offset];
    }
	else if 	( 	(offset < (int)(startoffset32+strm_buf_size)) //starts from zero, so startoffset+sectorsize is next element actually((n-1)+1).
			&&
			(offset	>=	startoffset32)
		){
		return disk_buf32[ ((offset - startoffset32) / u32size) ]; //OK		
	}
	else{
        ichfly_readdma_rom((u32)(offset&0x1ffffff),(u8*)&disk_buf32[0],strm_buf_size,32);
        /*
        int i = 0;
        for (i = 0; i < sectorsize/4; i++){
            disk_buf32[i] = ichfly_readu32((u32)((offset&0x1ffffff) + (i*4)));
		}
        */
		startoffset32=offset;
		return disk_buf32[0]; //OK (must be zero since element 0 of the newly filled gbarom buffer has offset_start)
	}
return 0;

#else
return *(u32*)&puzzle_original[offset];
#endif

}
