#include <Arduino.h>
#include <Preferences.h>
#include "ina226.h"
#include "nv_data.h"

Preferences Prefs;
CONFIG_t Config;

#define MODE_READ_WRITE  false
#define MODE_READ_ONLY   true

#define DEFAULT_AVERAGE     	AVG_1
#define DEFAULT_BUS_CONV    	CONV_332_US
#define DEFAULT_SHUNT_CONV    	CONV_332_US  
#define DEFAULT_SCALE		    SCALE_HI
#define DEFAULT_SAMPLE_SECS		SAMPLE_SECS_MIN  


void  nv_config_load(CONFIG_t &config){
	if (Prefs.begin("config", MODE_READ_ONLY) == false) {
		Serial.println("Preferences 'config' namespace not found, creating with defaults.");
		Prefs.end();
		nv_config_reset(config);
		} 
	else {
		config.ssid = Prefs.getString("ssid", "");
		config.password = Prefs.getString("pwd", "");
		config.averaging = Prefs.getUShort("avg", DEFAULT_AVERAGE);
		config.busConv = Prefs.getUShort("bConv", DEFAULT_BUS_CONV);
		config.shuntConv = Prefs.getUShort("sConv", DEFAULT_SHUNT_CONV);
		config.sampleSecs = Prefs.getUInt("smpSecs", DEFAULT_SAMPLE_SECS);
		config.scale = Prefs.getUInt("scale", DEFAULT_SCALE);
		Prefs.end();
		nv_config_print(config);
		}
	}


void nv_config_print(CONFIG_t &config) {
	Serial.println("SSID = " + config.ssid);
	Serial.println("averaging = " + String(config.averaging));
	Serial.println("bus conversion = " + String(config.busConv));
	Serial.println("shunt conversion = " + String(config.shuntConv));
	Serial.println("scale = " + String(config.scale));
	Serial.println("sample seconds = " + String(config.sampleSecs));
	}


void nv_config_reset(CONFIG_t &config) {
	config.ssid = "";
	config.password = "";
	config.averaging = DEFAULT_AVERAGE;
	config.busConv = DEFAULT_BUS_CONV; 
	config.shuntConv = DEFAULT_SHUNT_CONV;
	config.sampleSecs = DEFAULT_SAMPLE_SECS;
	config.scale = DEFAULT_SCALE;
	nv_config_store(config);
	Serial.println("Set Defaults");
	nv_config_print(config);
	}


void nv_config_store(CONFIG_t &config){
	Prefs.begin("config", MODE_READ_WRITE);
	Prefs.clear(); 
	Prefs.putString("ssid", config.ssid); 
	Prefs.putString("pwd", config.password); 
	Prefs.putUShort("avg", config.averaging); 
	Prefs.putUShort("bConv", config.busConv); 
	Prefs.putUShort("sConv", config.shuntConv); 
	Prefs.putUInt("smpSecs", config.sampleSecs); 
	Prefs.putUInt("scale", config.scale); 
	Prefs.end();
	nv_config_print(config);
	}
