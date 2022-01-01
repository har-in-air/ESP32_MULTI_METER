#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <esp_wifi.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"
#include "wifi_cfg.h"

static const char* TAG = "wifi_cfg";

extern const char* FwRevision;

const char* szAPSSID = "ESP32_INA226";
const char* szAPPassword = "123456789";

AsyncWebServer* pServer = NULL;

static void wifi_start_as_AP();
static void wifi_start_as_station();

static String string_processor(const String& var);
static void not_found_handler(AsyncWebServerRequest *request);
static void index_page_handler(AsyncWebServerRequest *request);
static void set_defaults_handler(AsyncWebServerRequest *request);
static void css_handler(AsyncWebServerRequest *request);
static void get_handler(AsyncWebServerRequest *request);
static void restart_handler(AsyncWebServerRequest *request);


// Replace %txt% placeholder 
static String string_processor(const String& var){
	if(var == "FW_REV"){
		return FwRevision;
		}
	else
	if(var == "AVG_0"){
		return Config.averaging == 0 ? "selected" : "";
		}
	else
	if(var == "AVG_1"){
		return Config.averaging == 1 ? "selected" : "";
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
	if(var == "BUS_CONV_0"){
		return Config.busConv == 0 ? "selected" : "";
		}
	else
	if(var == "BUS_CONV_1"){
		return Config.busConv == 1 ? "selected" : "";
		}
	else
	if(var == "BUS_CONV_2"){
		return Config.busConv == 2 ? "selected" : "";
		}
	else
	if(var == "BUS_CONV_3"){
		return Config.busConv == 3 ? "selected" : "";
		}
	else
	if(var == "SHUNT_CONV_0"){
		return Config.shuntConv == 0 ? "selected" : "";
		}
	else
	if(var == "SHUNT_CONV_1"){
		return Config.shuntConv == 1 ? "selected" : "";
		}
	else
	if(var == "SHUNT_CONV_2"){
		return Config.shuntConv == 2 ? "selected" : "";
		}
	else
	if(var == "SHUNT_CONV_3"){
		return Config.shuntConv == 3 ? "selected" : "";
		}
	else
	if(var == "SCALE_0"){
		return Config.scale == 0 ? "selected" : "";
		}
	else
	if(var == "SCALE_1"){
		return Config.scale == 1 ? "selected" : "";
		}
	else
	if(var == "SAMPLE_SECS"){
		return String(Config.sampleSecs);
		}
	else
	if(var == "SAMPLE_RATE"){
		return String(SampleRate);
		}
	else
	if(var == "SSID"){
		return Config.ssid;
		}
	else
	if(var == "PASSWORD"){
		return Config.password;
		}
	else return "?";
	}


void wifi_off() {
    WiFi.mode(WIFI_MODE_NULL);
    esp_wifi_stop();
    delay(100);
    }

static void not_found_handler(AsyncWebServerRequest *request) {
	request->send(404, "text/plain", "Not found");
	}

static void index_page_handler(AsyncWebServerRequest *request) {
	ina226_get_sample_rate();
    request->send(LittleFS, "/index.html", String(), false, string_processor);
    }

static void css_handler(AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
    }

static void set_defaults_handler(AsyncWebServerRequest *request) {
    nv_config_reset(Config);
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
    if (request->hasParam("avg")) {
        inputMessage = request->getParam("avg")->value();
        bChange = true; 
    	Config.averaging = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("shuntConv")) {
        inputMessage = request->getParam("shuntConv")->value();
        bChange = true; 
    	Config.shuntConv = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("busConv")) {
        inputMessage = request->getParam("busConv")->value();
        bChange = true; 
    	Config.busConv = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("scale")) {
        inputMessage = request->getParam("scale")->value();
        bChange = true; 
    	Config.scale = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("sampleSecs")) {
        inputMessage = request->getParam("sampleSecs")->value();
        bChange = true; 
    	Config.sampleSecs = (uint16_t)inputMessage.toInt();
        }
    if (request->hasParam("ssid")) {
        inputMessage = request->getParam("ssid")->value();
        bChange = true; 
        Config.ssid = inputMessage;
        }
    if (request->hasParam("password")) {
        inputMessage = request->getParam("password")->value();
        bChange = true; 
        Config.password = inputMessage;
        }

    if (bChange == true) {
        Serial.println("Options changed");
        nv_config_store(Config);
        bChange = false;
        }
    request->send(200, "text/html", "Input Processed<br><a href=\"/\">Return to Home Page</a>");  
    }


static void wifi_start_as_AP() {
	Serial.printf("Starting Access Point %s with password %s\n", szAPSSID, szAPPassword);
	WiFi.softAP(szAPSSID, szAPPassword);
	IPAddress IP = WiFi.softAPIP();
	Serial.print("AP IP address : ");
	Serial.println(IP);
	}


static void wifi_start_as_station() {
	Serial.printf("Connecting as station to SSID %s\n", Config.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config.ssid.c_str(), Config.password.c_str());
    if (WiFi.waitForConnectResult(10000UL) != WL_CONNECTED) {
    	Serial.printf("Connection failed!\n");
    	wifi_start_as_AP();
    	}
	else {
    	Serial.println();
    	Serial.print("Local IP Address: ");
    	Serial.println(WiFi.localIP());
		}
	}


void wifi_init() {
    esp_wifi_start(); // necessary if esp_wifi_stop() was called before deep_sleep
    delay(100);
	if (Config.ssid == "") {
		wifi_start_as_AP();
		}
	else {
		wifi_start_as_station();
		}
	
    pServer = new AsyncWebServer(80);
    if (pServer == NULL) {
        ESP_LOGE(TAG, "Error creating AsyncWebServer!");
        while(1);
        }

    pServer->onNotFound(not_found_handler);
    pServer->on("/", HTTP_GET, index_page_handler);
    pServer->on("/style.css", HTTP_GET, css_handler);
    pServer->on("/defaults", HTTP_GET, set_defaults_handler);
    pServer->on("/get", HTTP_GET, get_handler);
    pServer->on("/restart", HTTP_GET, restart_handler);

    // support for OTA firmware update using url http://<ip address>/update
    AsyncElegantOTA.begin(pServer);  
    pServer->begin();   
    }


