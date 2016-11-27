#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>  
#include <ctype.h>


#ifdef __cplusplus
extern "C" {
#endif

extern void getHttp(char* url);

extern char* server_ip;

extern char ip_decimal[0x10];

extern char * print_ip(u32 ip);


extern void Wifi_CopyMacAddr(volatile void * dest, volatile void * src);
extern int Wifi_CmpMacAddr(volatile void * mac1, volatile void * mac2);

extern unsigned long Wifi_Init(int initflags);
bool Wifi_InitDefault(bool useFirmwareSettings);
extern int Wifi_CheckInit();

extern int Wifi_RawTxFrame(u16 datalen, u16 rate, u16 * data);
extern void Wifi_SetSyncHandler(WifiSyncHandler wshfunc);
extern void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc);
extern int Wifi_RxRawReadPacket(s32 packetID, s32 readlength, u16 * data);

extern void Wifi_DisableWifi();
extern void Wifi_EnableWifi();
extern void Wifi_SetPromiscuousMode(int enable);
extern void Wifi_ScanMode();
extern void Wifi_SetChannel(int channel);

extern int Wifi_GetNumAP();
extern int Wifi_GetAPData(int apnum, Wifi_AccessPoint * apdata);
extern int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint * apdata, Wifi_AccessPoint * match_dest);
extern int Wifi_ConnectAP(Wifi_AccessPoint * apdata, int wepmode, int wepkeyid, u8 * wepkey);
extern void Wifi_AutoConnect();

extern int Wifi_AssocStatus();
extern int Wifi_DisconnectAP();
extern int Wifi_GetData(int datatype, int bufferlen, unsigned char * buffer);


extern void Wifi_Update();
extern void Wifi_Sync();


#ifdef WIFI_USE_TCP_SGIP
extern void Wifi_Timer(int num_ms);
extern void Wifi_SetIP(u32 IPaddr, u32 gateway, u32 subnetmask, u32 dns1, u32 dns2);
extern u32 Wifi_GetIP();

#endif

#ifdef __cplusplus
};
#endif
