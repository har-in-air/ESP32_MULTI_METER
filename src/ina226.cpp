#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "nv_data.h"
#include "ina226.h"

int Scale = SCALE_HI;
int SampleRate;

int16_t VBusBuf[MAX_SAMPLES];
int16_t VShuntBuf[MAX_SAMPLES];


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

#define i32(x) ((int32_t)(x))

void ina226_capture_samples(int32_t nSamples) {
	int32_t vsmax, vsmin, vsavg; // vshunt adc
	int32_t vbmax, vbmin, vbavg; // vbus adc
	vsmax = vbmax = -32768;
	vsmin = vbmin = 32767;
	vsavg = vbavg = 0;
	uint32_t tstart = micros();
	for (int inx = 0; inx < nSamples; inx++){
		if (digitalRead(pinAlert) == 0) {
			uint16_t val;
			// clear alert flag
			ina226_read_reg(REG_MASK, &val); 

			// read shunt voltage adc
			ina226_read_reg(REG_SHUNT, &val); 
			int16_t sval = (int16_t)val;
			VShuntBuf[inx] = sval;
			vsavg += i32(sval);
			if (i32(sval) > vsmax) vsmax = i32(sval);
			if (i32(sval) < vsmin) vsmin = i32(sval);

			// read bus voltage adc
			ina226_read_reg(REG_VBUS, &val); 
			VBusBuf[inx] = (int16_t)val;
			vbavg += i32(val);
			if (i32(val) > vbmax) vbmax = i32(val);
			if (i32(val) < vbmin) vbmin = i32(val);
			}
		}
	uint32_t us = micros() - tstart;
	Serial.printf("\n%d samples captured in %dus, sample rate = %.1f\n", nSamples, us, 1000000.0f*(float)nSamples/(float)us);
	vsavg = vsavg / nSamples;
	// convert shunt adc readings to uV
	// shunt adc resolution is 2.5uV/lsb
	vsavg = (vsavg*25)/10;  // uV
	vsmax = (vsmax*25)/10;  // uV
	vsmin = (vsmin*25)/10;  // uV
	int32_t imax, imin, iavg;
	if (Scale == SCALE_HI) {
		Serial.println("Full scale = 1.64A");
		// 0.05ohm shunt resistor, I = Vshunt/0.05 = (Vshunt * 20) uA
		// Resolution = 2.5uV/0.05ohms = 50uA
		// Full scale = ((32767 * 2.5uV) / 0.05ohms) = 1638350 uA = 1.63835A
		iavg = vsavg*20; // uA
		imax = vsmax*20; // uA
		imin = vsmin*20; // uA
		}
	else {
		Serial.println("Full scale = 78mA");
		// 1.05 ohm shunt resistor, I = Vshunt/1.05 = Vshunt * 0.95238 uA
		// Resolution = 2.5uV/1.05ohms = 2.38uA
		// Full scale = ((32767 * 2.5uV) / 1.05ohms) = 78016.6 uA = 78.016 mA
		iavg = (vsavg*9524)/10000;
		imax = (vsmax*9524)/10000;
		imin = (vsmin*9524)/10000;
		}	
	Serial.printf("Iavg %.3fmA\n", (float)iavg/1000.0f);
	Serial.printf("Imax %.3fmA\n", (float)imax/1000.0f);
	Serial.printf("Imin %.3fmA\n", (float)imin/1000.0f);

	vbavg = vbavg/nSamples;
	// convert bus adc reading to  uV
	// bus adc resolution is 1.25mV/lsb = 1250 uV/lsb
	// full scale = 32767 * 1.25mV = 40.95875V
	vbavg = vbavg*1250; // uV
	// vload = vbus - vshunt
	Serial.printf("Vavg %.3fV\n", (float)(vbavg - vsavg)/1000000.0f);
	}


void ina226_reset() {
	Serial.println("INA226 system reset, writing 0x8000 to CFG register");
	// system reset, bit self-clears
	ina226_write_reg(REG_CFG, 0x8000);
	delay(50);
	}


void ina226_config() {
	// continuous bus and shunt readings, averaging and conversion times from configuration options
	uint16_t config = 0x0400 | (Config.averaging << 8) | (Config.busConv << 6) | (Config.shuntConv << 3) | 0x0007;
	Serial.printf("Writing 0x%04X to CFG register\n", config);
	ina226_write_reg(REG_CFG, config);

	Serial.println("Writing 0x0400 to MSK register");
	// conversion ready -> alert pin goes low, clear by reading REG_MASK
	ina226_write_reg(REG_MASK, 0x0400);
	}


void ina226_get_sample_rate() {
	SampleRate = 1000;
	}