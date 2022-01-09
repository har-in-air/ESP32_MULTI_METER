#ifndef INA226_H_
#define INA226_H_

#define INA226_I2C_ADDR  0x40 // 7-bit address

#define REG_CFG		0x00
#define REG_SHUNT	0x01
#define REG_VBUS	0x02
#define REG_MASK	0x06
#define REG_ALERT	0x07
#define REG_ID		0xFE

#define MAX_SAMPLES		10000

#define SAMPLE_SECS_MIN       1
#define SAMPLE_SECS_MAX       30

#define SCALE_HI	0 // Shunt R = 0.05 ohms, Full scale = 1.64A
#define SCALE_LO	1 // Shunt R = 1.05 ohms, Full scale = 78mA


typedef struct {
	// input
	uint16_t cfg;
	int   scale;
	int   nSamples;
	uint32_t periodUs;
	// output
	float sampleRate; // Hz
	float vavg; // volts
	float vmax;
	float vmin;
	float iavgma; // mA
	float imaxma;
	float iminma;
} MEASURE_t;

typedef struct {
	uint16_t reg;
	uint32_t periodUs;
	} CONFIG_t;

#define NUM_CFG 3

extern int16_t Buffer[];
extern CONFIG_t Config[];
extern MEASURE_t Measure;

void	ina226_write_reg(uint8_t regAddr, uint16_t data);
int 	ina226_read_reg(uint8_t regAddr, uint16_t* pdata);
void	ina226_reset();
void	ina226_config(int cfgIndex, bool bOneShot);
void	ina226_capture_continuous(MEASURE_t &measure, int16_t buffer[]);
void 	ina226_capture_triggered(MEASURE_t &measure, int16_t buffer[]);
void	ina226_capture_oneshot(MEASURE_t &measure);
void	ina226_calibrate_sample_rates();

#endif