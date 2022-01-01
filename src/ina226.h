#ifndef INA226_H_
#define INA226_H_

#define INA226_I2C_ADDR  0x40 // 7-bit address

#define REG_CFG		0x00
#define REG_SHUNT	0x01
#define REG_VBUS	0x02
#define REG_MASK	0x06
#define REG_ID		0xFE

#define AVG_1	0
#define AVG_4	1

#define CONV_140_US	0
#define CONV_204_US	1
#define CONV_332_US	2
#define CONV_588_US	3

#define SAMPLES_PER_SEC 	1000
#define MAX_SAMPLES			10000

#define SAMPLE_SECS_MIN       1
#define SAMPLE_SECS_MAX       30

#define SCALE_HI	0 // Full scale = 1.64A
#define SCALE_LO	1 // Full scale = 78mA

extern int SampleRate;

void ina226_write_reg(uint8_t regAddr, uint16_t data);
int ina226_read_reg(uint8_t regAddr, uint16_t* pdata);
void ina226_capture_samples(int32_t nSamples);
void ina226_config();
void ina226_reset();
void ina226_get_sample_rate();

#endif