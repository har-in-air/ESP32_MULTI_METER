#include <Arduino.h>
#include <Preferences.h>
#include "nv_data.h"

Preferences Prefs;

OPTIONS_t Options;

#define MODE_READ_WRITE  false
#define MODE_READ_ONLY   true


void  nv_options_load(OPTIONS_t &options){
	if (Prefs.begin("options", MODE_READ_ONLY) == false) {
		Serial.println("Preferences 'options' namespace not found, creating with defaults.");
		Prefs.end();
		nv_options_reset(options);
		} 
	else {
		options.ssid = Prefs.getString("ssid", "");
		options.password = Prefs.getString("pwd", "");
		Prefs.end();
		nv_options_print(options);
		}
	}


void nv_options_print(OPTIONS_t &options) {
	Serial.println("SSID = " + options.ssid);
	}


void nv_options_reset(OPTIONS_t &options) {
	options.ssid = "";
	options.password = "";
	nv_options_store(options);
	Serial.println("Set Default Options");
	nv_options_print(options);
	}


void nv_options_store(OPTIONS_t &options){
	Prefs.begin("options", MODE_READ_WRITE);
	Prefs.clear(); 
	Prefs.putString("ssid", options.ssid); 
	Prefs.putString("pwd", options.password); 
	Prefs.end();
	nv_options_print(options);
	}

