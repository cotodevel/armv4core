#include <nds.h>
#include "util.h"

#ifndef RTC_EMU
#define RTC_EMU

#define RTC_ADDRESS (u32) 0x080000c4

#endif

#ifdef __cplusplus
extern "C" {
#endif

u8 gpioread8(struct GBASystem * gba_inst, u8 slot);
u16 gpioread16(struct GBASystem * gba_inst, u8 slot);
u32 gpioread32(struct GBASystem * gba_inst,u8 slot);

void gpiowrite8(struct GBASystem * gba_inst,u8 slot,u8 value);
void gpiowrite16(struct GBASystem * gba_inst,u8 slot,u16 value);
void gpiowrite32(struct GBASystem * gba_inst,u8 slot,u32 value);


u32 debugrtc_gbavirt(struct GBASystem * gba_inst,u32 address, u32 cmd, u8 access, u8 width);


void rtcread(u8 * buf); // read-> buffer
void rtcwrite(u8 * buf); //write->buffer

//updates RTC IO with actual clock data depending on commands imbued on 0x080000c4
void process_iocmd(struct GBASystem * gba_inst,u8 rtc_cmd);

#ifdef __cplusplus
}
#endif
