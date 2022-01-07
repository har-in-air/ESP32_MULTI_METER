#ifndef WIFI_CFG_H_
#define WIFI_CFG_H_

#include <ESPAsyncWebServer.h>

extern AsyncWebSocket ws;

extern uint32_t clientID;
extern bool bConnected;
extern bool bCapture;

void wifi_init();
void wifi_off();

#endif
