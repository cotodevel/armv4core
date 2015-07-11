#include <nds.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//libnds clock
extern time_t libnds_get_timeinst();
extern u32 libnds_get_dayrtc(time_t t_inst);
extern u32 libnds_get_monthrtc(time_t t_inst);
extern u32 libnds_get_yearrtc(time_t t_inst);
extern u32 libnds_get_hourrtc(time_t t_inst);
extern u32 libnds_get_minrtc(time_t t_inst);
extern u32 libnds_get_secrtc(time_t t_inst);

//coto's ipc clock

//only ARM7 has the hardware RTC.
#ifdef ARM7
void ipc_clock();
#endif

extern u8 gba_get_yearbytertc();
extern u8 gba_get_monthrtc();
extern u8 gba_get_dayrtc();
extern u8 gba_get_dayofweekrtc();
extern u8 gba_get_hourrtc();
extern u8 gba_get_minrtc();
extern u8 gba_get_secrtc();

#ifdef __cplusplus
}
#endif

