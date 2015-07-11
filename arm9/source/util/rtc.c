#include <nds.h>
#include "rtc.h"
#include "util.h"
#include "clock.h"

//Definitions of RTC callbacks

//
//slot: 0 = c4 | 1 = c6 | 2 = c8 
//

u8 gpioread8(struct GBASystem * gba_inst, u8 slot){
	return (u8)gba_inst->gpio[slot];
}

u16 gpioread16(struct GBASystem * gba_inst, u8 slot){
	return (u16)gba_inst->gpio[slot];
}

u32 gpioread32(struct GBASystem * gba_inst,u8 slot){
	u32 out;
	
	out = (gba_inst->gpio[slot]);
	
	if((slot+1)<(sizeof(gba_inst->gpio)))
		out = ((gba_inst->gpio[slot+1])<<16);
	
	return (u32)out;
}


void gpiowrite8(struct GBASystem * gba_inst,u8 slot,u8 value){
	gba_inst->gpio[slot] = (gba_inst->gpio[slot]&0xff00) | value;
}

void gpiowrite16(struct GBASystem * gba_inst,u8 slot,u16 value){
	gba_inst->gpio[slot] = value;
}

void gpiowrite32(struct GBASystem * gba_inst,u8 slot,u32 value){
	gba_inst->gpio[slot] = (u32)((value)&0x0000ffff);
	
	if((slot+1)<(sizeof(gba_inst->gpio)))
		gba_inst->gpio[slot+1] = (u32)((value)&0xffff0000);
}


//these are checked on each rtc/gpio cpuread/writes:
void process_iocmd(struct GBASystem * gba_inst,u8 rtc_cmd){
	//which is the IO port we should check? (for commands)
	switch(rtc_cmd){
		//your RTC/etc command list here
		
		//2   2   7 bytes, date & time (year,month,day,day_of_week,hh,mm,ss)
		case(0x2):
			
			gba_inst->rtc_datetime[0] = gba_get_yearbytertc();
			gba_inst->rtc_datetime[1] = gba_get_monthrtc();
			gba_inst->rtc_datetime[2] = gba_get_dayrtc();
			gba_inst->rtc_datetime[3] = gba_get_dayofweekrtc();
			gba_inst->rtc_datetime[4] = gba_get_hourrtc();
			gba_inst->rtc_datetime[5] = gba_get_minrtc();
			gba_inst->rtc_datetime[6] = gba_get_secrtc();
			
		break;
	
	}
	
}


//1.0 GPIO Functionality (cpuread/writes has gpio execute)

//1.1 RTC:

//debug RTC cmds( // addr , GPIO command, read=0/write=1 / 8bit - 16bit - 32 bit)
u32 debugrtc_gbavirt(struct GBASystem * gba_inst,u32 address, u32 value_cmd, u8 access, u8 width){
	
	if(access==0){
		if(width==8){
			iprintf("[BYTE] Trying to read GPIO @ (%x):[%x] \n",(unsigned int)address,(unsigned int) gpioread8(gba_inst,0));
		}
		else if (width==16){
			iprintf("[HWORD] Trying to read RTC Cmd @ (%x):[%x] \n",(unsigned int)address,(unsigned int) gpioread16(gba_inst,0));
		}
		else if (width==32){
			iprintf("[WORD] Trying to read RTC Cmd @ (%x):[%x] \n",(unsigned int)address,(unsigned int) gpioread32(gba_inst,0));
		}
		else {
			return 0;
		}
	}
	else if(access==1){
		if(width==8){
			iprintf("[BYTE] trying to write RTC Cmd(%x) @ (%x)",(unsigned int)value_cmd,(unsigned int)address);
		}
		else if (width==16){
			iprintf("[HWORD] trying to write RTC Cmd(%x) @ (%x)",(unsigned int)value_cmd,(unsigned int)address);
		}
		else if (width==32){
			iprintf("[WORD] trying to write RTC Cmd(%x) @ (%x)",(unsigned int)value_cmd,(unsigned int)address);
		}
	}
	
	return 0;
}



//stores RTC from NDS7 into a buffer
void rtcread(u8 * buf){
	buf[0] = gba_get_yearbytertc();
	buf[1] = gba_get_monthrtc();
	buf[2] = gba_get_dayrtc();
	buf[3] = gba_get_dayofweekrtc();
	buf[4] = gba_get_hourrtc();
	buf[5] = gba_get_minrtc();
	buf[6] = gba_get_secrtc();
}


//writes to RTC from buffer
void rtcwrite(u8 * buf){

}