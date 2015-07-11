#include <nds.h>
#include <time.h>

#include "clock.h"
#include "gba_ipc.h"

//libnds's clock
//parse the array contents as integer.! if you want accurate time
/*
	buf[0]=(cur_time->tm_year + (1900));
	buf[1]=cur_time->tm_mon;
	buf[2]=cur_time->tm_mday;
	
	buf[3]=cur_time->tm_hour;
	buf[4]=cur_time->tm_min;
	buf[5]=cur_time->tm_sec;
	
	return 0;
*/

inline time_t libnds_get_timeinst(){
	return (time_t)time(NULL);
}

inline u32 libnds_get_dayrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (gmtime( (const time_t *) &t_inst ) )->tm_mday;
}

inline u32 libnds_get_monthrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (gmtime( (const time_t *) &t_inst ) )->tm_mon;
}

inline u32 libnds_get_yearrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (((gmtime( (const time_t *) &t_inst ) )->tm_year) + (1900) );
}

//

inline u32 libnds_get_hourrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (gmtime( (const time_t *) &t_inst ) )->tm_hour;
}

inline u32 libnds_get_minrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (gmtime( (const time_t *) &t_inst ) )->tm_min;
}

inline u32 libnds_get_secrtc(time_t t_inst){
	//time_t t = time(NULL);
	return (u32) (gmtime( (const time_t *) &t_inst )->tm_sec);
}

//clock
#ifdef ARM7
void ipc_clock(){


u8 clock_buf[0x20];
	
//rtcGetTime(clock_buf);
rtcGetTimeAndDate (clock_buf);
	
SHARED_IPC->arm7clock_inst[0] = (Tarm7clock*)(u32)clock_buf[0]; //yy
SHARED_IPC->arm7clock_inst[1] = (Tarm7clock*)(u32)clock_buf[1]; //mth
SHARED_IPC->arm7clock_inst[2] = (Tarm7clock*)(u32)clock_buf[2]; //dd
SHARED_IPC->arm7clock_inst[3] = (Tarm7clock*)(u32)clock_buf[3]; //wat - day of week?
SHARED_IPC->arm7clock_inst[4] = (Tarm7clock*)(u32)clock_buf[4]; //hh
SHARED_IPC->arm7clock_inst[5] = (Tarm7clock*)(u32)clock_buf[5]; //mm
SHARED_IPC->arm7clock_inst[6] = (Tarm7clock*)(u32)clock_buf[6]; //ss
	
}
#endif


//coto's clock
inline u8 gba_get_yearbytertc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[0];
}

inline u8 gba_get_monthrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[1];
}

inline u8 gba_get_dayrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[2];
}

inline u8 gba_get_dayofweekrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[3];
}


inline u8 gba_get_hourrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[4];
}

inline u8 gba_get_minrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[5];
}

inline u8 gba_get_secrtc(){
	return (u8)(u32)SHARED_IPC->arm7clock_inst[6];
}
