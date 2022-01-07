#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"

#define i32(x) ((int32_t)(x))

int NumSamples;
int16_t Buffer[MAX_SAMPLES*2];

MEASURE_t Measure;

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

void ina226_capture_oneshot(MEASURE_t &measure) {
	// configure for one-shot bus and shunt adc conversion
	ina226_config(measure.cfgIndex, true);
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
	ina226_read_reg(REG_MASK, &reg_mask); 
	int16_t sval = (int16_t)reg_shunt;
	if (measure.scale == SCALE_HI) {
		measure.iavgma = sval*0.05f;
		}
	else {
		measure.iavgma = sval*0.002381f;
		}	
	measure.iminma = measure.iavgma;
	measure.imaxma = measure.iavgma;

	measure.vavg = reg_bus*0.00125f; 
	measure.vmax = measure.vavg;
	measure.vmin = measure.vavg;
	measure.sampleRate = 1000000.0f/(float)us;
	Serial.printf("OneShot : [%02d] %s %dus %dHz %.1fV %.3fmA\n", measure.cfgIndex, measure.scale == SCALE_LO ? "LO" : "HI", us, (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}


void ina226_capture_continuous(MEASURE_t &measure, int16_t buffer[]) {
	int16_t smax, smin, bmax, bmin; // shunt and bus raw readings 
	int32_t savg, bavg; // averaging accumulators
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	uint16_t reg_mask, reg_bus, reg_shunt;
	int16_t sval;
	// configure for continuous bus and shunt adc conversions
	ina226_config(measure.cfgIndex, false);
	uint32_t tstart = micros();
	for (int inx = 0; inx < measure.nSamples; inx++){
		while (digitalRead(pinAlert) == HIGH);
		// read shunt and bus ADCs
		ina226_read_reg(REG_SHUNT, &reg_shunt); 
		ina226_read_reg(REG_VBUS, &reg_bus); 
		// clear alert flag
		ina226_read_reg(REG_MASK, &reg_mask); 

		sval = (int16_t)reg_shunt;
		buffer[2*inx] = sval;
		savg += i32(sval);
		if (sval > smax) smax = sval;
		if (sval < smin) smin = sval;

		sval = (int16_t)reg_bus;
		buffer[2*inx+1] = sval; 
		bavg += i32(sval);
		if (sval > bmax) bmax = sval;
		if (sval < bmin) bmin = sval;
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
	Serial.printf("Continuous : [%02d] %s %dHz %.1fV %.3fmA\n", measure.cfgIndex, measure.scale == SCALE_LO ? "LO" : "HI", (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}


void ina226_reset() {
	Serial.println("INA226 system reset");
	// system reset, bit self-clears
	ina226_write_reg(REG_CFG, 0x8000);
	delay(50);
	}


void ina226_config(int cfgIndex, bool bOneShot) {
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// averaging and conversion times from configuration options + one-shot / continuous bus and shunt readings
	uint16_t cfg = ConfigTbl.cfg[cfgIndex].reg | (bOneShot ? 0x0003 : 0x0007);
	ina226_write_reg(REG_CFG, cfg);
	}


void ina226_calibrate_sample_rates() {
	for (int inx = 0; inx < NUM_CFG; inx++) {
		Measure.cfgIndex = inx;
		Measure.scale = SCALE_HI;
		Measure.nSamples = 2000;
		ina226_capture_continuous(Measure, Buffer);
		ConfigTbl.cfg[inx].sampleRate = Measure.sampleRate;
		}
	}	