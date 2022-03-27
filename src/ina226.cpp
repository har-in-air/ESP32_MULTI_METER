#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"

#define i32(x) ((int32_t)(x))

static const char* TAG = "ina226";

volatile bool DataReadyFlag = false;
volatile bool MeterReadyFlag = false;
volatile bool GateOpenFlag = false;
volatile bool CVCaptureFlag = false;
volatile bool EndCaptureFlag = false;
volatile bool LastPacketAckFlag = false;


const CONFIG_t Config[NUM_CFG] = {
	// chart capture configurations
	{ 0x4000 | (0 << 9) | (1 << 6) | (1 << 3), 500}, // avg 1, v 204uS, s 204uS, period 500uS
	{ 0x4000 | (0 << 9) | (2 << 6) | (2 << 3), 1000}, // avg 1, v 332uS, s 332uS, period 1000uS
	{ 0x4000 | (1 << 9) | (1 << 6) | (1 << 3), 2500},  // avg 4, v 204uS, s 204uS, period 2500uS
	// meter capture configuration
	{ 0x4000 | (4 << 9) | (4 << 6) | (4 << 3), 1064000}  // avg 128, v 1.1mS, s 1.1mS, period 557.57mS
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


uint16_t ina226_read_reg(uint8_t regAddr) {
	Wire.beginTransmission(INA226_I2C_ADDR);
	Wire.write(regAddr);
	Wire.endTransmission(false); // restart
	Wire.requestFrom(INA226_I2C_ADDR, 2);
	uint8_t buf[2];
	buf[0] = Wire.read();
	buf[1] = Wire.read();
	// msbyte, then lsbyte
	uint16_t res = ((uint16_t)buf[1]) |  (((uint16_t)buf[0])<<8);
	return res;
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

void ina226_capture_oneshot(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	switch_scale(SCALE_LO);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// configure for one-shot bus and shunt adc conversion
	buffer[0] = MSG_TX_CV_METER;
	buffer[1] = SCALE_LO;
	// configure for one-shot bus and shunt adc conversion
	ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0003);
	uint32_t tstart = micros();
	// pinAlert pulled up to 3v3, active low on conversion complete
	while (digitalRead(pinAlert) == HIGH);
	ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0003);
	while (digitalRead(pinAlert) == HIGH);
	uint32_t tend = micros();
	// measure the INA226 total conversion time for shunt and bus adcs
	uint32_t us = tend - tstart;

	uint16_t reg_bus, reg_shunt;
	// read shunt and bus ADCs
	reg_shunt = ina226_read_reg(REG_SHUNT);
	reg_bus = ina226_read_reg(REG_VBUS); 
	int16_t shunt_i16 = (int16_t)reg_shunt;
	measure.m.cv_meas.iavgma = shunt_i16*0.002381f;
	measure.m.cv_meas.scale = SCALE_LO;
	if (measure.m.cv_meas.iavgma > 78.0f) {
		switch_scale(SCALE_HI);
		buffer[1] = SCALE_HI;
		ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0003);
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		reg_shunt = ina226_read_reg(REG_SHUNT);
		shunt_i16 = (int16_t)reg_shunt;
		reg_bus = ina226_read_reg(REG_VBUS); 
		measure.m.cv_meas.iavgma = shunt_i16*0.05f;
		measure.m.cv_meas.scale = SCALE_HI;
		}

	buffer[2] = shunt_i16;
	buffer[3] = (int16_t)reg_bus;
	measure.m.cv_meas.iminma = measure.m.cv_meas.iavgma;
	measure.m.cv_meas.imaxma = measure.m.cv_meas.iavgma;
	measure.m.cv_meas.vavg = reg_bus*0.00125f; 
	measure.m.cv_meas.vmax = measure.m.cv_meas.vavg;
	measure.m.cv_meas.vmin = measure.m.cv_meas.vavg;
	measure.m.cv_meas.sampleRate = 1000000.0f/(float)us;
	MeterReadyFlag = true;
	ESP_LOGI(TAG,"OneShot : 0x%04X %s %dus %dHz %.1fV %.3fmA\n", measure.m.cv_meas.cfg, measure.m.cv_meas.scale == SCALE_LO ? "LO" : "HI", us, (int)(measure.m.cv_meas.sampleRate+0.5f), measure.m.cv_meas.vavg, measure.m.cv_meas.iavgma);
	}


bool ina226_capture_averaged_sample(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_bus, reg_shunt;
	buffer[0] = MSG_TX_CV_METER;
	buffer[1] = measure.m.cv_meas.scale;
	switch_scale(measure.m.cv_meas.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// continuous bus and shunt conversion
	ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0007);

	// throw away first sample
	// pinAlert pulled up to 3v3, active low on conversion complete
	while (digitalRead(pinAlert) == HIGH);
	// read shunt and bus ADCs
	reg_shunt = ina226_read_reg(REG_SHUNT); 
	reg_bus = ina226_read_reg(REG_VBUS); 

	uint32_t tstart = micros();
	int inx = 0;
	savg = bavg = 0;
	int numSamples = 250000/(int)measure.m.cv_meas.periodUs; 
	while (inx < numSamples){
		uint32_t t1 = micros();
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		reg_shunt = ina226_read_reg(REG_SHUNT); 
		reg_bus = ina226_read_reg(REG_VBUS); 
		
		data_i16 = (int16_t)reg_shunt;
		if ((data_i16 == 32767) || (data_i16 == -32768)) {
			return false; // off-scale reading
			}
		savg += i32(data_i16);

		data_i16 = (int16_t)reg_bus;
		bavg += i32(data_i16);
		while ((micros() - t1) < measure.m.cv_meas.periodUs);
		inx++;
		}
	uint32_t us = micros() - tstart;
	measure.m.cv_meas.sampleRate = (1000000.0f*(float)numSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / numSamples;
	measure.m.cv_meas.iavgma = (measure.m.cv_meas.scale == SCALE_HI)? savg*0.05f : savg*0.002381f;
	// convert bus adc reading to V
	bavg = bavg / numSamples;
	measure.m.cv_meas.vavg = bavg*0.00125f; 
	buffer[2] = (int16_t)savg;
	buffer[3] = (int16_t)bavg;
	// vload = vbus
	ESP_LOGI(TAG,"CV Meter sample : %s %.1fV %.3fmA\n", measure.m.cv_meas.scale == SCALE_LO ? "LO" : "HI", measure.m.cv_meas.vavg, measure.m.cv_meas.iavgma);
	MeterReadyFlag = true;
	return true;
	}


void ina226_capture_buffer_triggered(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t smax, smin, bmax, bmin, data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_bus, reg_shunt;
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	int samplesPerSecond = 1000000/(int)measure.m.cv_meas.periodUs;
	switch_scale(measure.m.cv_meas.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// continuous bus and shunt conversion
	ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0007);

	// throw away first sample
	// pinAlert pulled up to 3v3, active low on conversion complete
	while (digitalRead(pinAlert) == HIGH);
	// read shunt and bus ADCs
	reg_shunt = ina226_read_reg(REG_SHUNT); 
	reg_bus = ina226_read_reg(REG_VBUS); 

	uint32_t tstart = micros();
	// packet header with start packet ID,
	// sample period in mS, and current scale 
	buffer[0] = MSG_TX_START;
	buffer[1] = (int16_t)measure.m.cv_meas.periodUs;
	buffer[2] = measure.m.cv_meas.scale;
	int offset = 3;
	EndCaptureFlag = false;
	int inx = 0;
	while (inx < measure.m.cv_meas.nSamples){
		uint32_t t1 = micros();
		int bufIndex = offset + 2*inx;
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		reg_shunt = ina226_read_reg(REG_SHUNT); 
		reg_bus = ina226_read_reg(REG_VBUS); 
		
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
		// break data buffer into packets with samplesPerSecond samples
		if (((inx+1) % samplesPerSecond) == 0) {
			// continued packet header ID for next socket transmission
			buffer[bufIndex+2] = MSG_TX;
			offset++;
			TxSamples = samplesPerSecond;
			EndCaptureFlag = (inx == (measure.m.cv_meas.nSamples-1)) ? true : false;
			DataReadyFlag = true; // ready to transmit the last `samplesPerSecond` samples 
			}
		while ((micros() - t1) < measure.m.cv_meas.periodUs);
		inx++;
		}
	uint32_t us = micros() - tstart;
	measure.m.cv_meas.sampleRate = (1000000.0f*(float)measure.m.cv_meas.nSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / measure.m.cv_meas.nSamples;
	if (measure.m.cv_meas.scale == SCALE_HI) {
		measure.m.cv_meas.iavgma = savg*0.05f;
		measure.m.cv_meas.imaxma = smax*0.05f;
		measure.m.cv_meas.iminma = smin*0.05f;
		}
	else {
		measure.m.cv_meas.iavgma = savg*0.002381f;
		measure.m.cv_meas.imaxma = smax*0.002381f;
		measure.m.cv_meas.iminma = smin*0.002381f;
		}	
	// convert bus adc reading to V
	bavg = bavg / measure.m.cv_meas.nSamples;
	measure.m.cv_meas.vavg = bavg*0.00125f; 
	measure.m.cv_meas.vmax = bmax*0.00125f; 
	measure.m.cv_meas.vmin = bmin*0.00125f; 
	// vload = vbus 
	ESP_LOGI(TAG,"CV Buffer Triggered : 0x%04X %s %.1fHz %.1fV %.3fmA\n", measure.m.cv_meas.cfg, measure.m.cv_meas.scale == SCALE_LO ? "LO" : "HI", measure.m.cv_meas.sampleRate, measure.m.cv_meas.vavg, measure.m.cv_meas.iavgma);
	}



void ina226_capture_buffer_gated(volatile MEASURE_t &measure, volatile int16_t* buffer) {
	int16_t smax, smin, bmax, bmin, data_i16; // shunt and bus readings 
	int32_t savg, bavg; // averaging accumulators
	uint16_t reg_bus, reg_shunt;
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	int samplesPerSecond = 1000000/(int)measure.m.cv_meas.periodUs;
	switch_scale(measure.m.cv_meas.scale);
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// continuous bus and shunt conversion
	ina226_write_reg(REG_CFG, measure.m.cv_meas.cfg | 0x0007);
	// throw away first sample
	// pinAlert pulled up to 3v3, active low on conversion complete
	while (digitalRead(pinAlert) == HIGH);
	// read shunt and bus ADCs
	reg_shunt = ina226_read_reg(REG_SHUNT); 
	reg_bus = ina226_read_reg(REG_VBUS); 

	// packet header with start packet ID,
	// sample period in mS, and current scale 
	buffer[0] = MSG_TX_START;
	buffer[1] = (int16_t)measure.m.cv_meas.periodUs;
	buffer[2] = measure.m.cv_meas.scale;
	int offset = 3;
	int numSamples = 0;
	EndCaptureFlag = false;
	// gate signal is active low
	while (digitalRead(pinGate) == HIGH); 
	GateOpenFlag = true;
	uint32_t tstart = micros();
	while((digitalRead(pinGate) == LOW) && (numSamples < MaxSamples)) {
		uint32_t t1 = micros();
		int bufIndex = offset + 2*numSamples;
		// pinAlert pulled up to 3v3, active low on conversion complete
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		reg_shunt = ina226_read_reg(REG_SHUNT); 
		reg_bus = ina226_read_reg(REG_VBUS); 
		
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
		// break data buffer into packets with samplesPerSecond samples
		if (((numSamples+1) % samplesPerSecond) == 0) {
			// continued packet header ID for next socket transmission
			buffer[bufIndex+2] = MSG_TX;
			offset++;
			TxSamples = samplesPerSecond;
			DataReadyFlag = true;
			}		
		while ((micros() - t1) < measure.m.cv_meas.periodUs);
		numSamples++;
		}
	uint32_t us = micros() - tstart;
	TxSamples = numSamples % samplesPerSecond;
	EndCaptureFlag = true;
	DataReadyFlag = true;
	measure.m.cv_meas.nSamples = numSamples;
	measure.m.cv_meas.sampleRate = (1000000.0f*(float)numSamples)/(float)us;
	// convert shunt adc reading to mA
	savg = savg / numSamples;
	if (measure.m.cv_meas.scale == SCALE_HI) {
		measure.m.cv_meas.iavgma = savg*0.05f;
		measure.m.cv_meas.imaxma = smax*0.05f;
		measure.m.cv_meas.iminma = smin*0.05f;
		}
	else {
		measure.m.cv_meas.iavgma = savg*0.002381f;
		measure.m.cv_meas.imaxma = smax*0.002381f;
		measure.m.cv_meas.iminma = smin*0.002381f;
		}	
	// convert bus adc reading to V
	bavg = bavg / numSamples;
	measure.m.cv_meas.vavg = bavg*0.00125f; 
	measure.m.cv_meas.vmax = bmax*0.00125f; 
	measure.m.cv_meas.vmin = bmin*0.00125f; 
	// vload = vbus
	ESP_LOGI(TAG,"CV Buffer Gated : %.3fsecs 0x%04X %s %.1fHz %.1fV %.3fmA\n", (float)us/1000000.0f, measure.m.cv_meas.cfg, measure.m.cv_meas.scale == SCALE_LO ? "LO" : "HI", measure.m.cv_meas.sampleRate, measure.m.cv_meas.vavg, measure.m.cv_meas.iavgma);
	}


void ina226_reset() {
	ESP_LOGI(TAG,"INA226 system reset");
	// system reset, bit self-clears
	ina226_write_reg(REG_CFG, 0x8000);
	delay(50);
	}


void ina226_test_capture() {
	ESP_LOGI(TAG,"Measuring one-shot sample times");
	Measure.m.cv_meas.scale = SCALE_HI;
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.m.cv_meas.cfg = Config[inx].reg;
		ina226_capture_oneshot(Measure, Buffer);
		}
	}