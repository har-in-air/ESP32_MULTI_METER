#include <Arduino.h>
#include <Wire.h>  
#include <FS.h>
#include <LittleFS.h>
#include "config.h"
#include "nv_data.h"
#include "wifi_cfg.h"
#include "tone.h"
#include "ina226.h"

const char* FwRevision = "0.90";

void switch_scale(int scale);

// switch on one of the FETs, other is turned off
// For SCALE_HI, shunt resistor is 0.05 ohms
// For SCALE_LO, shunt resistor is 1.05 ohms
void switch_scale(int scale) {
	digitalWrite(pinFET1, scale == SCALE_HI ? LOW : HIGH);  
	digitalWrite(pinFET05, scale == SCALE_HI ? HIGH : LOW);  
	Options.scale = scale;
	}


void setup() {
	pinMode(pinAlert, INPUT); // external pullup, active low
	pinMode(pinGate, INPUT); // external pullup, active low
	pinMode(pinBtn1, INPUT_PULLUP);
	pinMode(pinBtn2, INPUT_PULLUP);
	pinMode(pinFET1, OUTPUT); // external pulldown
	pinMode(pinFET05, OUTPUT); // external pulldown

	Wire.begin(pinSDA,pinSCL); 
	Wire.setClock(400000);

	Serial.begin(115200);
	Serial.println();
	Serial.printf("ESP32_INA226 v%s compiled on %s at %s\n\n", FwRevision, __DATE__, __TIME__);

	nv_options_load(Options);
	nv_config_load(ConfigTbl);

	switch_scale(Options.scale);
	
	uint16_t val;
	ina226_read_reg(REG_ID, &val);
	if (val != 0x5449) {
		Serial.printf("INA226 Manufacturer ID read = 0x%04X, expected 0x5449\n", val);
		Serial.println("Halting...");
		while (1){}
		}

	ina226_reset();

   	
	Serial.println("Measuring one-shot sample times");
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfgIndex = inx;
		Measure.scale = SCALE_HI;
		ina226_capture_oneshot(Measure);
		}

#if 0
	Serial.println("Calibrating continuous sample rates");
	ina226_calibrate_sample_rates();
	nv_config_store(ConfigTbl);
#endif

	Serial.println("Starting web server");
	if (!LittleFS.begin(true)){
		Serial.println("LittleFS mount error");
		return;
		}  
	wifi_init();
	}


void loop() {
	ws.cleanupClients();
	if (bCapture) {
		bCapture = false;
		Measure.cfgIndex = 2;
		Measure.scale = SCALE_HI;
		Measure.nSamples = NumSamples;
		Serial.printf("Capturing %d samples\n", NumSamples );
		ina226_capture_continuous(Measure, Buffer);
		if (bConnected) { 
     		Serial.println("Transmitting samples");
			ws.binary(clientID, (uint8_t*)Buffer, NumSamples*4); // needs size in bytes
			}
		}
	}
	//switch_scale(SCALE_MA);
	//ina226_capture_samples(2000, Measure);
	//Serial.printf("V = %.1fV\nIavg = %.1fmA\nSampleRate = %.1fHz\n\n", Measure.vavg, Measure.iavgma, Measure.sampleRate);
	//switch_scale(SCALE_UA);
	//ina226_capture_samples(2*SAMPLES_PER_SEC);
 //  	}


