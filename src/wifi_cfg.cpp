#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"
#include "wifi_cfg.h"

static const char* TAG = "wifi_cfg";

extern const char* FwRevision;

const char* szAPSSID = "ESP32_INA226";
const char* szAPPassword = "123456789";

AsyncWebServer* pServer = NULL;
AsyncWebSocket ws("/ws");

uint32_t clientID;
bool bConnected = false;
bool bCapture = false;

void socket_handle_message(void *arg, uint8_t *data, size_t len);
void socket_event_handler(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType          type,
             void                 *arg,
             uint8_t              *data,
             size_t                len);


static void wifi_start_as_ap();
static void wifi_start_as_station();

static String string_processor(const String& var);
static void not_found_handler(AsyncWebServerRequest *request);
static void index_page_handler(AsyncWebServerRequest *request);
static void set_defaults_handler(AsyncWebServerRequest *request);
static void get_handler(AsyncWebServerRequest *request);
static void restart_handler(AsyncWebServerRequest *request);
static void capture_handler(AsyncWebServerRequest *request);

// Replace %txt% placeholder 
static String string_processor(const String& var){
	if(var == "FW_REV"){
		return FwRevision;
		}
	else
	if(var == "C0"){
		return ConfigTbl.cfgIndex == 0 ? "selected" : "";
		}
	else
	if(var == "C1"){
		return ConfigTbl.cfgIndex == 1 ? "selected" : "";
		}
	else
	if(var == "C2"){
		return ConfigTbl.cfgIndex == 2 ? "selected" : "";
		}
	else
	if(var == "C3"){
		return ConfigTbl.cfgIndex == 3 ? "selected" : "";
		}
	else
	if(var == "C4"){
		return ConfigTbl.cfgIndex == 4 ? "selected" : "";
		}
	else
	if(var == "C5"){
		return ConfigTbl.cfgIndex == 5 ? "selected" : "";
		}
	else
	if(var == "C6"){
		return ConfigTbl.cfgIndex == 6 ? "selected" : "";
		}
	else
	if(var == "C7"){
		return ConfigTbl.cfgIndex == 7 ? "selected" : "";
		}
	else
	if(var == "C8"){
		return ConfigTbl.cfgIndex == 8 ? "selected" : "";
		}
	else
	if(var == "C9"){
		return ConfigTbl.cfgIndex == 9 ? "selected" : "";
		}
	else
	if(var == "S0"){
		return ConfigTbl.cfg[0].sampleRate ? String(ConfigTbl.cfg[0].sampleRate) : "--";
		}
	else
	if(var == "S0"){
		return ConfigTbl.cfg[0].sampleRate ? String(ConfigTbl.cfg[0].sampleRate) : "--";
		}
	else
	if(var == "S1"){
		return ConfigTbl.cfg[1].sampleRate ? String(ConfigTbl.cfg[1].sampleRate) : "--";
		}
	else
	if(var == "S2"){
		return ConfigTbl.cfg[2].sampleRate ? String(ConfigTbl.cfg[2].sampleRate) : "--";
		}
	else
	if(var == "S3"){
		return ConfigTbl.cfg[3].sampleRate ? String(ConfigTbl.cfg[3].sampleRate) : "--";
		}
	else
	if(var == "S4"){
		return ConfigTbl.cfg[4].sampleRate ? String(ConfigTbl.cfg[4].sampleRate) : "--";
		}
	else
	if(var == "S5"){
		return ConfigTbl.cfg[5].sampleRate ? String(ConfigTbl.cfg[5].sampleRate) : "--";
		}
	else
	if(var == "S6"){
		return ConfigTbl.cfg[6].sampleRate ? String(ConfigTbl.cfg[6].sampleRate) : "--";
		}
	else
	if(var == "S7"){
		return ConfigTbl.cfg[7].sampleRate ? String(ConfigTbl.cfg[7].sampleRate) : "--";
		}
	else
	if(var == "S8"){
		return ConfigTbl.cfg[8].sampleRate ? String(ConfigTbl.cfg[8].sampleRate) : "--";
		}
	else
	if(var == "S9"){
		return ConfigTbl.cfg[9].sampleRate ? String(ConfigTbl.cfg[9].sampleRate) : "--";
		}
	else
	if(var == "SAMPLE_SECS_MIN"){
		return String(SAMPLE_SECS_MIN);
		}
	else
	if(var == "SAMPLE_SECS_MAX"){
		return String(SAMPLE_SECS_MAX);
		}
	else
	if(var == "SCALE_HI"){
		return Options.scale == 0 ? "selected" : "";
		}
	else
	if(var == "SCALE_LO"){
		return Options.scale == 1 ? "selected" : "";
		}
	else
	if(var == "SAMPLE_SECS"){
		return String(Options.sampleSecs);
		}
	else
	if(var == "SSID"){
		return Options.ssid;
		}
	else
	if(var == "PASSWORD"){
		return Options.password;
		}
	else return "?";
	}


static void not_found_handler(AsyncWebServerRequest *request) {
	request->send(404, "text/plain", "Not found");
	}

static void index_page_handler(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false, string_processor);
    }

static void capture_handler(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/capture.html", String(), false, string_processor);
    }

static void set_defaults_handler(AsyncWebServerRequest *request) {
	nv_config_reset(ConfigTbl);
	nv_options_reset(Options);
    request->send(200, "text/html", "Default options set<br><a href=\"/\">Return to Home Page</a>");  
    }


static void restart_handler(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "Restarting ...");  
	Serial.println("Restarting ESP32");
	Serial.flush();
	delay(100);
    esp_restart();
    }

static void get_handler(AsyncWebServerRequest *request) {
    String inputMessage;
    bool bChange = false;
    if (request->hasParam("cfgInx")) {
        inputMessage = request->getParam("cfgInx")->value();
        bChange = true; 
    	ConfigTbl.cfgIndex = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("scale")) {
        inputMessage = request->getParam("scale")->value();
        bChange = true; 
    	Options.scale = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("sampleSecs")) {
        inputMessage = request->getParam("sampleSecs")->value();
        bChange = true; 
    	Options.sampleSecs = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("ssid")) {
        inputMessage = request->getParam("ssid")->value();
        bChange = true; 
        Options.ssid = inputMessage;
        }
    if (request->hasParam("password")) {
        inputMessage = request->getParam("password")->value();
        bChange = true; 
        Options.password = inputMessage;
        }

    if (bChange == true) {
        Serial.println("Options changed");
        nv_config_store(ConfigTbl);
		nv_options_store(Options);
        bChange = false;
        }
    request->send(200, "text/html", "Input Processed<br><a href=\"/\">Return to Home Page</a>");  
    }


static void wifi_start_as_ap() {
	Serial.printf("Starting Access Point %s with password %s\n", szAPSSID, szAPPassword);
	WiFi.softAP(szAPSSID, szAPPassword);
	IPAddress IP = WiFi.softAPIP();
	Serial.print("AP IP address : ");
	Serial.println(IP);
	}


static void wifi_start_as_station() {
	Serial.printf("Connecting as station to SSID %s\n", Options.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Options.ssid.c_str(), Options.password.c_str());
    if (WiFi.waitForConnectResult(10000UL) != WL_CONNECTED) {
    	Serial.printf("Connection failed!\n");
    	wifi_start_as_ap();
    	}
	else {
    	Serial.println();
    	Serial.print("Local IP Address: ");
    	Serial.println(WiFi.localIP());
		}
	}


void wifi_init() {
    delay(100);
	if (Options.ssid == "") {
		wifi_start_as_ap();
		}
	else {
		wifi_start_as_station();
		}
	
	if (!MDNS.begin("esp32")) { // Use http://esp32.local for web server page
		Serial.println("Error starting mDNS service");
	    }
    pServer = new AsyncWebServer(80);
    if (pServer == NULL) {
        ESP_LOGE(TAG, "Error creating AsyncWebServer!");
        while(1);
        }
	ws.onEvent(socket_event_handler);
	pServer->addHandler(&ws);
    pServer->onNotFound(not_found_handler);
    pServer->on("/", HTTP_GET, index_page_handler);
    pServer->on("/capture", HTTP_GET, capture_handler);
    pServer->on("/defaults", HTTP_GET, set_defaults_handler);
    pServer->on("/get", HTTP_GET, get_handler);
    pServer->on("/restart", HTTP_GET, restart_handler);
    pServer->serveStatic("/", LittleFS, "/");

    // support for OTA firmware update using url http://<ip address>/update
    AsyncElegantOTA.begin(pServer);  
    pServer->begin();   
	MDNS.addService("http", "tcp", 80);
    }


void socket_event_handler(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType          type,
             void                 *arg,
             uint8_t              *data,
             size_t                len) {

    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
			clientID = client->id();
			bConnected = true;
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
			bConnected = false;
			clientID = 0;
            break;
        case WS_EVT_DATA:
            socket_handle_message(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    	}
	}


void socket_handle_message(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        const uint8_t size = JSON_OBJECT_SIZE(3);
        StaticJsonDocument<size> json;
        DeserializationError err = deserializeJson(json, data);
        if (err) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
            return;
			}

        const char *szAction = json["action"];
        const char *szSamples = json["samples"];
        const char *szScale = json["scale"];

		Serial.printf("json[\"samples\"]= %s\n", szSamples);

        int nSamples = strtol(szSamples, NULL, 10);
		if (nSamples > 0  && nSamples < MAX_SAMPLES) {
			NumSamples = nSamples;
			}
        if (strcmp(szAction, "capture") == 0) {
			bCapture = true;
	        }
	Serial.printf("Received socket message : [action : %s, samples : %d, scale : %s]\n", szAction, NumSamples, szScale);
    }
}
