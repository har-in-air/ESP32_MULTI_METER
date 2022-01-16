#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"
#include "wifi_cfg.h"

static const char* TAG = "wifi_cfg";

extern const char* FwRevision;

// stand-alone WiFi Access Point SSID (no password)
const char* szAPSSID = "ESP32_INA226";

AsyncWebServer* pServer = NULL;
AsyncWebSocket ws("/ws");


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
	nv_options_reset(Options);
    request->send(200, "text/html", "Default options set<br><a href=\"/\">Return to Home Page</a>");  
    }


static void restart_handler(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "Restarting ...");  
	ESP_LOGI(TAG,"Restarting ESP32");
	Serial.flush();
	delay(100);
    esp_restart();
    }

static void get_handler(AsyncWebServerRequest *request) {
    String inputMessage;
    bool bChange = false;
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
        ESP_LOGI(TAG,"Options changed");
		nv_options_store(Options);
        bChange = false;
        }
    request->send(200, "text/html", "Input Processed<br><a href=\"/\">Return to Home Page</a>");  
    }


static void wifi_start_as_ap() {
	ESP_LOGI(TAG,"Starting Access Point with SSID=%s, no password\n", szAPSSID);
	WiFi.softAP(szAPSSID);
	IPAddress ipaddr = WiFi.softAPIP();
	ESP_LOGI(TAG, "Web Server IP address : %s", ipaddr.toString().c_str());
	}


static void wifi_start_as_station() {
	ESP_LOGI(TAG,"Connecting as station to Access Point with SSID=%s\n", Options.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Options.ssid.c_str(), Options.password.c_str());
    if (WiFi.waitForConnectResult(10000UL) != WL_CONNECTED) {
    	ESP_LOGI(TAG,"Connection failed!\n");
    	wifi_start_as_ap();
    	}
	else {
		IPAddress ipaddr = WiFi.localIP();
    	ESP_LOGI(TAG, "Web Server IP Address: %s", ipaddr.toString().c_str());
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
	
	if (!MDNS.begin("meter")) { // Use http://meter.local for web server page
		ESP_LOGI(TAG,"Error starting mDNS service");
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
            ESP_LOGI(TAG,"WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
			ClientID = client->id();
			SocketConnectedFlag = true;
            break;
        case WS_EVT_DISCONNECT:
            ESP_LOGI(TAG,"WebSocket client #%u disconnected\n", client->id());
			SocketConnectedFlag = false;
			ClientID = 0;
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
		if (data[0] == 'x') {
			//ESP_LOGI(TAG, "Transmit OK Ack");
			TransmitOKFlag = true;
			}
		else {
			const uint8_t size = JSON_OBJECT_SIZE(4);
			StaticJsonDocument<size> json;
			DeserializationError err = deserializeJson(json, data);
			if (err) {
				ESP_LOGI(TAG, "deserializeJson() failed with code %s", err.c_str());
				return;
				}

			const char *szAction = json["action"];
			const char *szCfgIndex = json["cfgIndex"];
			const char *szCaptureSeconds = json["captureSecs"];
			const char *szScale = json["scale"];

			ESP_LOGI(TAG,"json[\"action\"]= %s\n", szAction);
			ESP_LOGI(TAG,"json[\"cfgIndex\"]= %s\n", szCfgIndex);
			ESP_LOGI(TAG,"json[\"captureSecs\"]= %s\n", szCaptureSeconds);
			ESP_LOGI(TAG,"json[\"scale\"]= %s\n", szScale);

			int cfgIndex = strtol(szCfgIndex, NULL, 10);
			int captureSeconds = strtol(szCaptureSeconds, NULL, 10);
			int sampleRate = 1000000/Config[cfgIndex].periodUs;
			int numSamples = captureSeconds*sampleRate;
			int scale = strtol(szScale, NULL, 10);
			
			Measure.cfg = Config[cfgIndex].reg | 0x0003;
			Measure.scale = scale;
			Measure.nSamples = numSamples;
			Measure.periodUs = Config[cfgIndex].periodUs;

			if (strcmp(szAction, "capture") == 0) {
				CaptureFlag = true;
				}
			}
		}
	}
