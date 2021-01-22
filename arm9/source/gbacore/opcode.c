#include "gba.arm.core.h"
#include "translator.h"
#include "ipcfifoTGDSUser.h"

/*
//ldr/str (inline)
u32 ldru32inlasm(u32 x1){
u32 y1;
__asm__ volatile(
				"ldr %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u16 ldru16inlasm(u32 x1){
u16 y1;
__asm__ volatile(
				"ldrh %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u8 ldru8inlasm(u32 x1){
u8 y1;
__asm__ volatile(
				"ldrb %[y1],[%[x1]]""\t"
				:[y1]  "=r" (y1) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1) //1st arg takes rw, 2nd and later ro
				); 
return y1;
}

u8 stru8inlasm(u32 x1,u32 x2,u8 y1){
u8 out;
__asm__ volatile(
				"strb %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

u16 stru16inlasm(u32 x1,u32 x2,u16 y1){
u16 out;
__asm__ volatile(
				"strh %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

u32 stru32inlasm(u32 x1,u32 x2,u32 y1){
u32 out;
__asm__ volatile(
				"str %[y1],[%[x1],%[x2]]""\t"
				"ldr %[out],[%[x1],%[x2]]""\t"
				
				:[out]  "=r" (out) // = write only (value+=value) / + read/write with output / & output only reg
				:[x1]  "r" (x1),[x2]  "r" (x2),[y1]  "r" (y1) //1st arg takes rw, 2nd and later ro
				); 
return out;
}

*/
u32 nopinlasm(){
__asm__ volatile(
				"nop""\t"
				: 	// = write only (value+=value) / + read/write with output / & output only reg
				:	//1st arg takes rw, 2nd and later ro
				:
				); 
return 0;
}


//extract_word(u32 * buffer (haystack), u32 word (needle) , int buffsize, u32 * buffer (output data),u32 delimiter_word), u8 type = 0 character string / 1 = hex
//extracts all values until "delimiter"
int extract_word(u32 * buffer,u32 word ,u32 * buffer_out,u32 delimiter,u8 type){ //strings shall be constant (not movables) for strlen
int i=0,temp=0,m=0;

//removed because code was fugly
//char string search
if (type==0){
	//printf("char string search!");
	/*
	//m=0;
	for(i=0;i<size;i++){
		k=0;
		for(j=m;j<strlen(word);j++){
		
			if  ( (( ( buffer[i]>>(k*8) )&0xff ) == (word[j]) ) ){ //j gets actual pos, does not always begin from 0
				temp++;
				//debug
				//printf(" %x_i - k(%d)",( ( buffer[i]>>(k*8) )&0xff ),k);
				//j=strlen(word);
				m++; //increase word segment only til reach top
			}
			
			//not found? then reset counter
			else {
				m=0;
				j=strlen(word);
			}
			
			//did we reach top bit 8*3?
			if(k==3) {
				k=0;
				j=strlen(word); //next i
			}
			else k++;
		}
		
		//found word? get everything starting from there until delimiter
		if (temp >= strlen(word)){
			m=0;
			//0 gets re used by m, twice, both for reset, and as a valid offset. this causes double valid word found.
			if (temp != strlen(word)) { 
				//k-=1;
			}
			else i++; //aligned means the current i index hasn't changed, next index is what we look for.
			
				//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				while( (u32)( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
				printf(" %x_i - k(%d)",(unsigned int)( ( buffer[i]>>(k*8) )&0xff ), (int)k);
				
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=( ( buffer[i]>>(k*8) ) );
				
				m++;
		
				if(k==3) {
					k=0;
					i++;
				}
				else k++;
				
			}
		break;
		}
	}
*/
}
//hex search
else if (type==32){
	//printf("hex search!");
	//printf("hex to look: %x",(unsigned int)word);
	int ret_val = 128*4;    //retries
    
    for(i=0;i<ret_val/4;i++){
		if  (buffer[i] == word ){ //j gets actual pos, does not always begin from 0
				temp++;
		}
        /*
		if(temp>0) {
            printf("found %x match!",word);
            while(1==1);
        }
        */
		//found word? get everything starting from there until delimiter
		if (temp > 0){
			//ori: while( ( ( buffer[i]>>(k*8) )&0xff ) != delimiter){
			while(buffer[m] != delimiter){
				//ori: *(buffer_out+m)=( ( buffer[i]>>(k*8) )&0xff );
				buffer_out[m]=buffer[m];
				m++;				
			}
			buffer_out[m]=buffer[m]; //and copy over delimiter's last opcode
			
			m++; //offset fix for natural numbers (at least 1 was found!)
			break; //done? kill main process
		}
        
	}
    
    if(temp == 0){
        return 0;
    }
}
return m;
}


//moves data from host to emulator map
void nds_2_gba_copy(u8 * source,u8 * dest,int size){
    int i = 0;
    for(i = 0 ; i < size ; i++){
        CPUWriteByte_stack(((u32)(u8*)dest)+i, source[i]);
    }
}

//moves data from emulator to host map
void gba_2_nds_copy(u8 * source,u8 * dest,int size){
    int i = 0;
    for(i = 0 ; i < size ; i++){
       dest[i] = CPUReadByte_stack(source[i]);
    }
}
