#ifndef NVDATA_H_
#define NVDATA_H_

#define SAMPLE_SECS_MIN       1
#define SAMPLE_SECS_MAX       30

#define SCALE_HI	0 // Shunt R = 0.05 ohms, Full scale = 1.64A
#define SCALE_LO	1 // Shunt R = 1.05 ohms, Full scale = 78mA

#define DEFAULT_SCALE		    SCALE_HI
#define DEFAULT_SAMPLE_SECS		SAMPLE_SECS_MIN  

#define DEFAULT_CFG_INDEX    	0

typedef struct {
	String ssid;
	String password;
	uint16_t scale;
	uint16_t sampleSecs;
	} OPTIONS_t;

typedef struct {
	uint16_t reg;
	uint32_t periodUs;
	} CONFIG_t;

#define NUM_CFG 3

typedef struct {
	CONFIG_t cfg[NUM_CFG];
	int cfgIndex;
} CONFIG_TABLE_t;

extern OPTIONS_t Options;
extern CONFIG_TABLE_t ConfigTbl;

void nv_options_store(OPTIONS_t &options);
void nv_options_load(OPTIONS_t &options);
void nv_options_reset(OPTIONS_t &options);
void nv_options_print(OPTIONS_t &options);

void nv_config_store(CONFIG_TABLE_t &configTbl);
void nv_config_load(CONFIG_TABLE_t &configTbl);
void nv_config_reset(CONFIG_TABLE_t &configTbl);
void nv_config_print(CONFIG_TABLE_t &configTbl);

#endif
