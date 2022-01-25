#ifndef WIFI_CFG_H_
#define WIFI_CFG_H_

#include <ESPAsyncWebServer.h>

extern AsyncWebSocket ws;

extern uint32_t ClientID;
extern volatile bool SocketConnectedFlag;
extern volatile bool StartCaptureFlag;
extern volatile bool LastPacketAckFlag;

void wifi_init();

#endif
