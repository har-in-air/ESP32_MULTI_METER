#ifndef INA226_H_
#define INA226_H_

#define INA226_I2C_ADDR  0x40 // 7-bit address

#define REG_CFG		0x00
#define REG_SHUNT	0x01
#define REG_VBUS	0x02
#define REG_MASK	0x06
#define REG_ALERT	0x07
#define REG_ID		0xFE


#define SCALE_HI	0 // Shunt R = 0.05 ohms, Full scale = 1.64A
#define SCALE_LO	1 // Shunt R = 1.05 ohms, Full scale = 78mA

#define MSG_GATE_OPEN	1234
#define MSG_TX_START	1111
#define MSG_TX			2222
#define MSG_TX_COMPLETE	3333
#define MSG_TX_METER	5678

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

#define NUM_CFG		4

extern volatile int16_t* Buffer;
extern const CONFIG_t Config[];
extern volatile MEASURE_t Measure;
extern int MaxSamples;
extern volatile bool GateOpenFlag;
extern volatile bool DataReadyFlag;
extern volatile int TxSamples;
extern volatile bool EndCaptureFlag;
extern volatile bool MeterReadyFlag;

void	ina226_write_reg(uint8_t regAddr, uint16_t data);
uint16_t ina226_read_reg(uint8_t regAddr);
void	ina226_reset();
void 	ina226_capture_triggered(volatile MEASURE_t &measure, volatile int16_t* buffer);
void 	ina226_capture_gated(volatile MEASURE_t &measure, volatile int16_t* buffer);
void	ina226_capture_oneshot(volatile MEASURE_t &measure, volatile int16_t* buffer);
void	ina226_test_capture();

#endif