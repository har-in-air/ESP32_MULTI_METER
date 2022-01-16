#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"

#define i32(x) ((int32_t)(x))

static const char* TAG = "ina226";

const CONFIG_t Config[NUM_CFG] = {
	{ 0x4000 | (0 << 8) | (1 << 6) | (2 << 3), // 1, 140uS, 332uS
	  1000UL },
	{ 0x4000 | (0 << 8) | (1 << 6) | (4 << 3), // 1, 140uS, 1100uS
	  2000UL },
	{ 0x4000 | (1 << 8) | (2 << 6) | (2 << 3), // 4, 332uS, 332uS
	  5000UL }
	};


static void switch_scale(int scale);

// switch on one of the FETs, other is turned off
// For SCALE_HI, shunt resistor is 0.05 ohms
// For SCALE_LO, shunt resistor is 1.05 ohms
static void switch_scale(int scale) {
	digitalWrite(pinFET1, scale == SCALE_HI ? LOW : HIGH);  
	digitalWrite(pinFET05, scale == SCALE_HI ? HIGH : LOW);  
	}

void ina226_write_reg(uint8_t regAddr, uint16_t data) {
	Wire.beginTransmission(INA226_I2C_ADDR);  
	Wire.write(regAddr);
	Wire.write((uint8_t)((data>>8) & 0x00FF));           
	Wire.write((uint8_t)(data & 0x00FF));           
	Wire.endTransmission();     
	}


int ina226_read_reg(uint8_t regAddr, uint16_t* pdata) {
	Wire.beginTransmission(INA226_I2C_ADDR);
	Wire.write(regAddr);
	Wire.endTransmission(false); // restart
	Wire.requestFrom(INA226_I2C_ADDR, 2);
	uint8_t buf[2];
	int cnt = 0;
	while (Wire.available())  {
		buf[cnt++] = Wire.read();
		}
	// msbyte, then lsbyte
	*pdata = ((uint16_t)buf[1]) |  (((uint16_t)buf[0])<<8);
	return cnt;
	}

// Shunt ADC resolution is 2.5uV/lsb
// Vshunt = (ShuntADCSample * 2.5) uV
// I = Vshunt/Rshunt

// SCALE_HI : Rshunt = 0.05ohm
// Resolution = 2.5uV/0.05ohms = 50uA = 0.05mA
// I = (ShuntADCSample * 2.5)/0.05 = (ShuntADCSample * 50) uA = (ShuntADCSample * 0.05)mA
// Full scale = (32767 * 50)uA = 1638350 uA = 1638.35mA

// SCALE_LO : Rshunt = 1.05 ohm shunt resistor
// Resolution = 2.5uV/1.05ohms = 2.38uA
// I = (ShuntADCSample * 2.5)/1.05 = (ShuntADCSample * 2.381) uA = (ShuntADCSample * 0.002381)mA
// Full scale = (32767 * 2.381) uA = 78016.67 uA = 78.017 mA

// Bus ADC resolution is 1.25mV/lsb 
// Full scale = (32767 * 1.25) mV = 40.95875V

void ina226_capture_oneshot(volatile MEASURE_t &measure) {
	switch_scale(measure.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// configure for one-shot bus and shunt adc conversion
	ina226_write_reg(REG_CFG, measure.cfg);
	uint32_t tstart = micros();
	// pinAlert pulled up to 3v3, active low on conversion complete
	while (digitalRead(pinAlert) == HIGH);
	uint32_t tend = micros();
	// measure the INA226 total conversion time for shunt and bus adcs
	uint32_t us = tend - tstart;

	uint16_t reg_mask, reg_bus, reg_shunt;
	// read shunt and bus ADCs
	ina226_read_reg(REG_SHUNT, &reg_shunt); 
	ina226_read_reg(REG_VBUS, &reg_bus); 
	// clear alert flag
	//ina226_read_reg(REG_MASK, &reg_mask); 
	int16_t data_i16 = (int16_t)reg_shunt;
	if (measure.scale == SCALE_HI) {
		measure.iavgma = data_i16*0.05f;
		}
	else {
		measure.iavgma = data_i16*0.002381f;
		}	
	measure.iminma = measure.iavgma;
	measure.imaxma = measure.iavgma;

	measure.vavg = reg_bus*0.00125f; 
	measure.vmax = measure.vavg;
	measure.vmin = measure.vavg;
	measure.sampleRate = 1000000.0f/(float)us;
	ESP_LOGI(TAG,"OneShot : 0x%04X %s %dus %dHz %.1fV %.3fmA\n", measure.cfg, measure.scale == SCALE_LO ? "LO" : "HI", us, (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}



void ina226_capture_triggered(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t smax, smin, bmax, bmin, data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_mask, reg_bus, reg_shunt;
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	switch_scale(measure.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	uint32_t tstart = micros();
	// packet header with start packet ID,
	// sample period in mS, and current scale 
	buffer[0] = MSG_TX_START;
	buffer[1] = ((uint16_t)measure.periodUs)/1000;
	buffer[2] = measure.scale;
	int offset = 3;
	for (int inx = 0; inx < measure.nSamples; inx++){
		uint32_t t1 = micros();
		int bufIndex = offset + 2*inx;
		ina226_write_reg(REG_CFG, measure.cfg);
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		ina226_read_reg(REG_SHUNT, &reg_shunt); 
		ina226_read_reg(REG_VBUS, &reg_bus); 
		
		data_i16 = (int16_t)reg_shunt;
		buffer[bufIndex] = data_i16;
		savg += i32(data_i16);
		if (data_i16 > smax) smax = data_i16;
		if (data_i16 < smin) smin = data_i16;

		data_i16 = (int16_t)reg_bus;
		buffer[bufIndex+1] = data_i16; 
		bavg += i32(data_i16);
		if (data_i16 > bmax) bmax = data_i16;
		if (data_i16 < bmin) bmin = data_i16;
		// break data buffer into packets with MAX_TRANSMIT_SAMPLES samples
		if (((inx+1) % MAX_TRANSMIT_SAMPLES) == 0) {
			// continued packet header ID for next socket transmission
			buffer[bufIndex+2] = MSG_TX;
			offset++;
			}
		while ((micros() - t1) < measure.periodUs);
		}
	uint32_t us = micros() - tstart;
	measure.sampleRate = (1000000.0f*(float)measure.nSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / measure.nSamples;
	if (measure.scale == SCALE_HI) {
		measure.iavgma = savg*0.05f;
		measure.imaxma = smax*0.05f;
		measure.iminma = smin*0.05f;
		}
	else {
		measure.iavgma = savg*0.002381f;
		measure.imaxma = smax*0.002381f;
		measure.iminma = smin*0.002381f;
		}	
	// convert bus adc reading to V
	bavg = bavg / measure.nSamples;
	measure.vavg = bavg*0.00125f; 
	measure.vmax = bmax*0.00125f; 
	measure.vmin = bmin*0.00125f; 
	// vload = vbus - vshunt
	ESP_LOGI(TAG,"Triggered : 0x%04X %s %.1fHz %.1fV %.3fmA\n", measure.cfg, measure.scale == SCALE_LO ? "LO" : "HI", measure.sampleRate, measure.vavg, measure.iavgma);
	}


void ina226_capture_gated(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t smax, smin, bmax, bmin, data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_mask, reg_bus, reg_shunt;
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	switch_scale(measure.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// packet header with start packet ID,
	// sample period in mS, and current scale 
	buffer[0] = MSG_TX_START;
	buffer[1] = ((uint16_t)measure.periodUs)/1000;
	buffer[2] = measure.scale;
	int offset = 3;
	int numSamples = 0;
	// wait for gate signal to go low
	while (digitalRead(pinGate) == HIGH); 
	GateOpenFlag = true;
	uint32_t tstart = micros();
	while((digitalRead(pinGate) == LOW) && (numSamples < MaxSamples)) {
		uint32_t t1 = micros();
		int bufIndex = offset + 2*numSamples;
		ina226_write_reg(REG_CFG, measure.cfg);
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		ina226_read_reg(REG_SHUNT, &reg_shunt); 
		ina226_read_reg(REG_VBUS, &reg_bus); 
		
		data_i16 = (int16_t)reg_shunt;
		buffer[bufIndex] = data_i16;
		savg += i32(data_i16);
		if (data_i16 > smax) smax = data_i16;
		if (data_i16 < smin) smin = data_i16;

		data_i16 = (int16_t)reg_bus;
		buffer[bufIndex+1] = data_i16; 
		bavg += i32(data_i16);
		if (data_i16 > bmax) bmax = data_i16;
		if (data_i16 < bmin) bmin = data_i16;
		// break data buffer into packets with MAX_TRANSMIT_SAMPLES samples
		if (((numSamples+1) % MAX_TRANSMIT_SAMPLES) == 0) {
			// continued packet header ID for next socket transmission
			buffer[bufIndex+2] = MSG_TX;
			offset++;
			}		
		while ((micros() - t1) < measure.periodUs);
		numSamples++;
		}
	uint32_t us = micros() - tstart;
	measure.nSamples = numSamples;
	measure.sampleRate = (1000000.0f*(float)numSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / numSamples;
	if (measure.scale == SCALE_HI) {
		measure.iavgma = savg*0.05f;
		measure.imaxma = smax*0.05f;
		measure.iminma = smin*0.05f;
		}
	else {
		measure.iavgma = savg*0.002381f;
		measure.imaxma = smax*0.002381f;
		measure.iminma = smin*0.002381f;
		}	
	// convert bus adc reading to V
	bavg = bavg / numSamples;
	measure.vavg = bavg*0.00125f; 
	measure.vmax = bmax*0.00125f; 
	measure.vmin = bmin*0.00125f; 
	// vload = vbus - vshunt
	ESP_LOGI(TAG,"Gated : %.3fsecs 0x%04X %s %.1fHz %.1fV %.3fmA\n", (float)us/1000000.0f, measure.cfg, measure.scale == SCALE_LO ? "LO" : "HI", measure.sampleRate, measure.vavg, measure.iavgma);
	}


void ina226_capture_continuous(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t smax, smin, bmax, bmin, data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_mask, reg_bus, reg_shunt;
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	switch_scale(measure.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// configure for continuous bus and shunt adc conversions
	ina226_write_reg(REG_CFG, measure.cfg);
	uint32_t tstart = micros();
	for (int inx = 0; inx < measure.nSamples; inx++){
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		ina226_read_reg(REG_SHUNT, &reg_shunt); 
		ina226_read_reg(REG_VBUS, &reg_bus); 
		data_i16 = (int16_t)reg_shunt;
		buffer[2*inx] = data_i16;
		savg += i32(data_i16);
		if (data_i16 > smax) smax = data_i16;
		if (data_i16 < smin) smin = data_i16;

		data_i16 = (int16_t)reg_bus;
		buffer[2*inx+1] = data_i16; 
		bavg += i32(data_i16);
		if (data_i16 > bmax) bmax = data_i16;
		if (data_i16 < bmin) bmin = data_i16;
		}
	uint32_t us = micros() - tstart;
	measure.sampleRate = (1000000.0f*(float)measure.nSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / measure.nSamples;
	if (measure.scale == SCALE_HI) {
		measure.iavgma = savg*0.05f;
		measure.imaxma = smax*0.05f;
		measure.iminma = smin*0.05f;
		}
	else {
		measure.iavgma = savg*0.002381f;
		measure.imaxma = smax*0.002381f;
		measure.iminma = smin*0.002381f;
		}	
	// convert bus adc reading to V
	bavg = bavg / measure.nSamples;
	measure.vavg = bavg*0.00125f; 
	measure.vmax = bmax*0.00125f; 
	measure.vmin = bmin*0.00125f; 
	// vload = vbus - vshunt
	ESP_LOGI(TAG,"Continuous : 0x%04X %s %dHz %.1fV %.3fmA\n", measure.cfg, measure.scale == SCALE_LO ? "LO" : "HI", (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}


void ina226_reset() {
	ESP_LOGI(TAG,"INA226 system reset");
	// system reset, bit self-clears
	ina226_write_reg(REG_CFG, 0x8000);
	delay(50);
	}


void ina226_test_capture() {
	ESP_LOGI(TAG,"Measuring one-shot sample times");
	Measure.scale = SCALE_LO;
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfg = Config[inx].reg | 0x0003;
		ina226_capture_oneshot(Measure);
		}

#if 0
	ESP_LOGI(TAG,"Measuring triggered sample-rates");
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfg = Config[inx].reg | 0x0003;
		Measure.periodUs = Config[inx].periodUs;
		Measure.nSamples = 2000;
		ina226_capture_triggered(Measure, Buffer);
		}
#endif
	}