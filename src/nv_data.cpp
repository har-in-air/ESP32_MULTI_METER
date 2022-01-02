#include <Arduino.h>
#include <Preferences.h>
//#include "ina226.h"
#include "nv_data.h"

Preferences Prefs;

OPTIONS_t Options;
CONFIG_TABLE_t ConfigTbl;

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
		options.sampleSecs = Prefs.getUShort("smpSecs", DEFAULT_SAMPLE_SECS);
		options.scale = Prefs.getUShort("scale", DEFAULT_SCALE);
		Prefs.end();
		nv_options_print(options);
		}
	}


void nv_options_print(OPTIONS_t &options) {
	Serial.println("SSID = " + options.ssid);
	Serial.println("Scale = " + String(options.scale));
	Serial.println("Sample seconds = " + String(options.sampleSecs));
	}


void nv_options_reset(OPTIONS_t &options) {
	options.ssid = "";
	options.password = "";
	options.sampleSecs = DEFAULT_SAMPLE_SECS;
	options.scale = DEFAULT_SCALE;
	nv_options_store(options);
	Serial.println("Set Default Options");
	nv_options_print(options);
	}


void nv_options_store(OPTIONS_t &options){
	Prefs.begin("options", MODE_READ_WRITE);
	Prefs.clear(); 
	Prefs.putString("ssid", options.ssid); 
	Prefs.putString("pwd", options.password); 
	Prefs.putUShort("smpSecs", options.sampleSecs); 
	Prefs.putUShort("scale", options.scale); 
	Prefs.end();
	nv_options_print(options);
	}


void  nv_config_load(CONFIG_TABLE_t &configTbl){
	if (Prefs.begin("config", MODE_READ_ONLY) == false) {
		Serial.println("Preferences 'config' namespace not found, creating with defaults.");
		Prefs.end();
		nv_config_reset(configTbl);
		} 
	else {
		if (Prefs.getBytesLength("config") != sizeof(CONFIG_TABLE_t)) {
			Serial.println("Prefs : config <-> sizeof(CONFIG_TABLE_t) mismatch, resetting.");
			Prefs.end();
			nv_config_reset(configTbl);
			}
		else {
			Prefs.getBytes("config", (void*)&configTbl, sizeof(CONFIG_TABLE_t));
			Prefs.end();			
			nv_config_print(configTbl);
			}
		}
	}	


void nv_config_store(CONFIG_TABLE_t &configTbl) {
	Prefs.begin("config", MODE_READ_WRITE);
	Prefs.clear();
	Prefs.putBytes("config", (const void*)&configTbl, sizeof(CONFIG_TABLE_t) );
	Prefs.end();
	}	


void nv_config_reset(CONFIG_TABLE_t &configTbl){
	// ---------------------------------------------------------------- avg  bconv  sconv
	configTbl.cfg[0].reg = 0x4000 | (0 << 8) | (2 << 6) | (0 << 3); // 1, 332uS, 140uS
	configTbl.cfg[1].reg = 0x4000 | (0 << 8) | (2 << 6) | (1 << 3); // 1, 332uS, 204uS
	configTbl.cfg[2].reg = 0x4000 | (0 << 8) | (2 << 6) | (2 << 3); // 1, 332uS, 332uS
	configTbl.cfg[3].reg = 0x4000 | (0 << 8) | (2 << 6) | (3 << 3); // 1, 332uS, 588uS
	configTbl.cfg[4].reg = 0x4000 | (0 << 8) | (2 << 6) | (4 << 3); // 1, 332uS, 1100uS

	configTbl.cfg[5].reg = 0x4000 | (1 << 8) | (2 << 6) | (0 << 3); // 4, 332uS, 140uS
	configTbl.cfg[6].reg = 0x4000 | (1 << 8) | (2 << 6) | (1 << 3); // 4, 332uS, 204uS
	configTbl.cfg[7].reg = 0x4000 | (1 << 8) | (2 << 6) | (2 << 3); // 4, 332uS, 332uS
	configTbl.cfg[8].reg = 0x4000 | (1 << 8) | (2 << 6) | (3 << 3); // 4, 332uS, 588uS
	configTbl.cfg[9].reg = 0x4000 | (1 << 8) | (2 << 6) | (4 << 3); // 4, 332uS, 1100uS

	configTbl.cfgIndex = DEFAULT_CFG_INDEX;
	nv_config_store(configTbl);
	nv_config_print(configTbl);
	}

void nv_config_print(CONFIG_TABLE_t &configTbl) {
	Serial.println("Config Index = " + String(configTbl.cfgIndex));
	}
