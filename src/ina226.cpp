#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"

int16_t VBusBuf[MAX_SAMPLES];
int16_t VShuntBuf[MAX_SAMPLES];

#define i32(x) ((int32_t)(x))
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

// SCALE_LO : Rshunt = 0.05ohm
// Resolution = 2.5uV/0.05ohms = 50uA = 0.05mA
// I = (ShuntADCSample * 2.5)/0.05 = (ShuntADCSample * 50) uA = (ShuntADCSample * 0.05)mA
// Full scale = (32767 * 50)uA = 1638350 uA = 1638.35mA

// SCALE_LO : Rshunt = 1.05 ohm shunt resistor
// Resolution = 2.5uV/1.05ohms = 2.38uA
// I = (ShuntADCSample * 2.5)/1.05 = (ShuntADCSample * 2.381) uA = (ShuntADCSample * 0.002381)mA
// Full scale = (32767 * 2.381) uA = 78016.67 uA = 78.017 mA

// Bus ADC resolution is 1.25mV/lsb 
// Full scale = (32767 * 1.25) mV = 40.95875V

void ina226_capture_oneshot(int cfgIndex, MEASURE_t &measure) {
	ina226_config(cfgIndex, true);
	uint32_t tstart = micros();
	while (digitalRead(pinAlert) == HIGH) {}
	uint32_t tend = micros();
	uint32_t us = tend - tstart;
	uint16_t reg_mask, reg_bus, reg_shunt;
	// read shunt and bus ADCs
	ina226_read_reg(REG_SHUNT, &reg_shunt); 
	ina226_read_reg(REG_VBUS, &reg_bus); 
	// clear alert flag
	ina226_read_reg(REG_MASK, &reg_mask); 
	int16_t sval = (int16_t)reg_shunt;
	if (Options.scale == SCALE_HI) {
		measure.iavgma = sval*0.05f;
		}
	else {
		measure.iavgma = sval*0.002381f;
		}	
	measure.vavg = reg_bus*0.00125f; 
	measure.sampleRate = 1000000.0f/(float)us;
	Serial.printf("OneShot : [%02d] %dus %dHz %.1fV %.3fmA\n", cfgIndex, us, (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}


void ina226_capture_continuous(int cfgIndex, int32_t nSamples, MEASURE_t &measure) {
	int32_t smax, smin, savg; // shunt
	int32_t bmax, bmin, bavg; // bus
	smax = bmax = -32768;
	smin = bmin = 32767;
	savg = bavg = 0;
	uint16_t reg_mask;
	uint16_t reg_bus;
	uint16_t reg_shunt;
	ina226_config(cfgIndex, false);
	uint32_t tstart = micros();
	for (int inx = 0; inx < nSamples; inx++){
		while (digitalRead(pinAlert) == HIGH) {}
		// read shunt and bus ADCs
		ina226_read_reg(REG_SHUNT, &reg_shunt); 
		ina226_read_reg(REG_VBUS, &reg_bus); 
		// clear alert flag
		ina226_read_reg(REG_MASK, &reg_mask); 

		int16_t sval = (int16_t)reg_shunt;
		VShuntBuf[inx] = sval;
		savg += i32(sval);
		if (i32(sval) > smax) smax = i32(sval);
		if (i32(sval) < smin) smin = i32(sval);

		VBusBuf[inx] = (int16_t)reg_bus;
		bavg += i32(reg_bus);
		if (i32(reg_bus) > bmax) bmax = i32(reg_bus);
		if (i32(reg_bus) < bmin) bmin = i32(reg_bus);
		}
	uint32_t us = micros() - tstart;
	measure.sampleRate = 1000000.0f*(float)nSamples/(float)us;
	savg = savg / nSamples;
	if (Options.scale == SCALE_HI) {
		measure.iavgma = savg*0.05f;
		measure.imaxma = smax*0.05f;
		measure.iminma = smin*0.05f;
		}
	else {
		measure.iavgma = savg*0.002381f;
		measure.imaxma = smax*0.002381f;
		measure.iminma = smin*0.002381f;
		}	

	bavg = bavg/nSamples;
	// convert bus adc reading to  uV
	measure.vavg = bavg*0.00125f; 
	measure.vmax = bmax*0.00125f; 
	measure.vmin = bmin*0.00125f; 
	// vload = vbus - vshunt
	Serial.printf("Continuous [%02d] %dHz %.1fV %.3fmA\n", cfgIndex, (int)(measure.sampleRate+0.5f), measure.vavg, measure.iavgma);
	}


void ina226_reset() {
	Serial.println("INA226 system reset, writing 0x8000 to CFG register");
	// system reset, bit self-clears
	ina226_write_reg(REG_CFG, 0x8000);
	delay(50);
	}


void ina226_config(int cfgIndex, bool bOneShot) {
	// conversion ready -> alert pin goes low
	ina226_write_reg(REG_MASK, 0x0400);
	// single shot bus and shunt readings, averaging and conversion times from configuration options
	uint16_t reg = ConfigTbl.cfg[cfgIndex].reg | (bOneShot ? 0x0003 : 0x0007);
	ina226_write_reg(REG_CFG, reg);
	}


void ina226_calibrate_sample_rates() {
	for (int inx = 0; inx < NUM_CFG; inx++) {
		ina226_capture_continuous(inx, 2000, Measure);
		ConfigTbl.cfg[inx].sampleRate = Measure.sampleRate;
		}
	}	