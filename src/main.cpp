#include <Arduino.h>
#include <Wire.h>  
#include <FS.h>
#include <LittleFS.h>
#include "config.h"
#include "nv_data.h"
#include "wifi_cfg.h"
#include "tone.h"
#include "ina226.h"

const char* FirmwareRevision = "1.00";

void switch_scale(int scale);

// switch on one of the FETs, other is turned off
// For SCALE_HI, shunt resistor is 0.05 ohms
// For SCALE_LO, shunt resistor is 1.05 ohms
void switch_scale(int scale) {
	digitalWrite(pinFET1000, scale == SCALE_HI ? LOW : HIGH);  
	digitalWrite(pinFET50, scale == SCALE_HI ? HIGH : LOW);  
	}


void setup() {
	pinMode(pinAlert, INPUT); // external pullup, active low
	pinMode(pinGate, INPUT); // external pullup, active low
	pinMode(pinBtn1, INPUT_PULLUP);
	pinMode(pinBtn2, INPUT_PULLUP);
	pinMode(pinFET1000, OUTPUT); // external pulldown
	pinMode(pinFET50, OUTPUT); // external pulldown
	switch_scale(SCALE_HI);

	Wire.begin(pinSDA,pinSCL); 
	Wire.setClock(400000);

	Serial.begin(115200);
	Serial.println();
	Serial.printf("Esp32-INA226 v%s compiled on %s at %s\n\n", FirmwareRevision, __DATE__, __TIME__);

	uint16_t val;
	ina226_read_reg(REG_ID, &val);
	if (val != 0x5449) {
		Serial.printf("INA226 Manufacturer ID read = 0x%04X, expected 0x5449\n", val);
		Serial.println("Halting...");
		while (1){}
		}

	ina226_reset();
	//ina226_read_reg(REG_CFG, &val);
	//Serial.printf("INA226 Reset Config Reg = 0x%04X\n", val);
	//ina226_read_reg(REG_MASK, &val);
	//Serial.printf("INA226 Reset Mask Reg = 0x%04X\n", val);
	
	nv_config_load(Config); 
	ina226_config();
	ina226_read_reg(REG_CFG, &val);
	Serial.printf("INA226 Config Reg = 0x%04X\n", val);
	ina226_read_reg(REG_MASK, &val);
	Serial.printf("INA226 Mask Reg = 0x%04X\n", val);

	if (!LittleFS.begin()){
		Serial.println("LittleFS mount error");
		return;
		}  
	wifi_init();
	}


void loop() {
	//switch_scale(SCALE_MA);
	//ina226_capture_samples(2*SAMPLES_PER_SEC);
	//switch_scale(SCALE_UA);
	//ina226_capture_samples(2*SAMPLES_PER_SEC);
  	}


