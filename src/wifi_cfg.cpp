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
#include "freq_counter.h"
#include "wifi_cfg.h"

static const char* TAG = "wifi_cfg";

extern const char* FwRevision;

// configure static IP address for connecting as station
static IPAddress Local_IP(192, 168, 43, 236);
static IPAddress Gateway(192, 168, 43, 1);
static IPAddress Subnet(255, 255, 255, 0);
static IPAddress PrimaryDNS(8, 8, 8, 8);
static IPAddress SecondaryDNS(8, 8, 4, 4);

// stand-alone WiFi Access Point SSID (no password)
const char* szAPSSID = "ESP32_METER";

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
static void wifi_start_as_station_static_IP();
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

static void cv_chart_handler(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/cv_chart.html", String(), false, string_processor);
    }

static void cv_meter_handler(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/cv_meter.html", String(), false, string_processor);
    }

static void freq_counter_handler(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/freq_counter.html", String(), false, string_processor);
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
	digitalWrite(pinLED, HIGH);
	}

static void wifi_start_as_station_static_IP() {
	ESP_LOGI(TAG,"Connecting as station static IP to Access Point with SSID=%s\n", Options.ssid);
	uint32_t startTick = millis();
	// Configures static IP address
  	if (!WiFi.config(Local_IP, Gateway, Subnet, PrimaryDNS, SecondaryDNS)) {
    	ESP_LOGI(TAG,"Station static IP config failure");
  		}
    WiFi.begin(Options.ssid.c_str(), Options.password.c_str());
    if (WiFi.waitForConnectResult(4000UL) != WL_CONNECTED) {
    	ESP_LOGI(TAG,"Connection failed!");
    	wifi_start_as_ap();
    	}
	else {
		IPAddress ipaddr = WiFi.localIP();
		uint32_t endTick = millis();
		ESP_LOGI(TAG, "Connected in %.2f seconds with IP addr %s", (float)(endTick - startTick)/1000.0f, ipaddr.toString().c_str());
		digitalWrite(pinLED, LOW);
		}
	}

static void wifi_start_as_station() {
	ESP_LOGI(TAG,"Connecting as station to Access Point with SSID=%s", Options.ssid);
	uint32_t startTick = millis();
    WiFi.mode(WIFI_STA);
    WiFi.begin(Options.ssid.c_str(), Options.password.c_str());
    if (WiFi.waitForConnectResult(10000UL) != WL_CONNECTED) {
    	ESP_LOGI(TAG,"Connection failed!");
    	wifi_start_as_ap();
    	}
	else {
		uint32_t endTick = millis();
		IPAddress ipaddr = WiFi.localIP();
		ESP_LOGI(TAG, "Connected in %.2f seconds with IP addr %s", (float)(endTick - startTick)/1000.0f, ipaddr.toString().c_str());
		digitalWrite(pinLED, LOW);
		}
	}


void wifi_init() {
    delay(100);
	if (Options.ssid == "") {
		wifi_start_as_ap();
		}
	else {
		wifi_start_as_station_static_IP();
		}
	
	if (!MDNS.begin("meter")) { // Use http://meter.local for web server page
		ESP_LOGI(TAG,"Error starting mDNS service");
	    }
    pServer = new AsyncWebServer(80);
    if (pServer == nullptr) {
        ESP_LOGE(TAG, "Error creating AsyncWebServer!");
        ESP.restart();
        }
	ws.onEvent(socket_event_handler);
	pServer->addHandler(&ws);
    pServer->onNotFound(not_found_handler);
    pServer->on("/", HTTP_GET, index_page_handler);
    pServer->on("/cv_chart", HTTP_GET, cv_chart_handler);
    pServer->on("/cv_meter", HTTP_GET, cv_meter_handler);
    pServer->on("/freq_counter", HTTP_GET, freq_counter_handler);
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
			//ESP_LOGI(TAG, "ack = x");
			LastPacketAckFlag = true;
			}
		else
		if (data[0] == 'm') {
			Measure.mode = MODE_CURRENT_VOLTAGE;
			Measure.m.cv_meas.nSamples = 1;
			Measure.m.cv_meas.cfg = Config[1].reg;
			Measure.m.cv_meas.periodUs = Config[1].periodUs;
			CVCaptureFlag = true;
			//ESP_LOGI(TAG,"cmd = m");
			}
		else
		if (data[0] == 'f') {
			Measure.mode = MODE_FREQUENCY;
			FreqCaptureFlag = true;
			//ESP_LOGI(TAG,"cmd = f");
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

			if (strcmp(szAction, "cv_capture") == 0) {
				const char *szCfgIndex = json["cfgIndex"];
				const char *szCaptureSeconds = json["captureSecs"];
				const char *szScale = json["scale"];

				int cfgIndex = strtol(szCfgIndex, NULL, 10);
				int captureSeconds = strtol(szCaptureSeconds, NULL, 10);
				int sampleRate = 1000000/Config[cfgIndex].periodUs;
				int numSamples = captureSeconds*sampleRate;
				int scale = strtol(szScale, NULL, 10);
				
				Measure.mode = MODE_CURRENT_VOLTAGE;
				Measure.m.cv_meas.cfg = Config[cfgIndex].reg;
				Measure.m.cv_meas.scale = scale;
				Measure.m.cv_meas.nSamples = numSamples;
				Measure.m.cv_meas.periodUs = Config[cfgIndex].periodUs;
				ESP_LOGI(TAG,"Mode = %d", Measure.mode);
				ESP_LOGI(TAG,"cfgIndex = %d", cfgIndex);
				ESP_LOGI(TAG,"scale = %d", scale);
				ESP_LOGI(TAG,"nSamples = %d", numSamples);
				ESP_LOGI(TAG,"periodUs = %d", Config[cfgIndex].periodUs);
				CVCaptureFlag = true;
				}
			else 
			if (strcmp(szAction, "oscfreq") == 0) {
				Measure.mode = MODE_FREQUENCY;
				const char *szOscFreqHz = json["freqhz"];

				ESP_LOGI(TAG,"json[\"action\"]= %s\n", szAction);
				ESP_LOGI(TAG,"json[\"freqhz\"]= %s\n", szOscFreqHz);

				OscFreqHz = (uint32_t)strtol(szOscFreqHz, NULL, 10);
				OscFreqFlag = true;
				}
			}
		}
	}
