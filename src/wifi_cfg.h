#ifndef WIFI_CFG_H_
#define WIFI_CFG_H_

#include <ESPAsyncWebServer.h>

extern AsyncWebSocket ws;

extern uint32_t clientID;
extern bool SocketConnectedFlag;
extern bool CaptureFlag;

void wifi_init();

#endif
