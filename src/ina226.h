#ifndef INA226_H_
#define INA226_H_

#define INA226_I2C_ADDR  0x40 // 7-bit address

#define REG_CFG		0x00
#define REG_SHUNT	0x01
#define REG_VBUS	0x02
#define REG_MASK	0x06
#define REG_ALERT	0x07
#define REG_ID		0xFE

#define MAX_SAMPLES			10000

typedef struct {
	float vavg; // volts
	float vmax;
	float vmin;
	float iavgma; // mA
	float imaxma;
	float iminma;
	float sampleRate; // Hz
} MEASURE_t;

extern MEASURE_t Measure;

void	ina226_write_reg(uint8_t regAddr, uint16_t data);
int 	ina226_read_reg(uint8_t regAddr, uint16_t* pdata);
void	ina226_reset();
void	ina226_config(int cfgIndex, bool bOneShot);
void	ina226_capture_continuous(int cfgIndex, int32_t nSamples, MEASURE_t &measure);
void	ina226_capture_oneshot(int cfgIndex, MEASURE_t &measure);
void	ina226_calibrate_sample_rates();

#endif