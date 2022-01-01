#ifndef NVDATA_H_
#define NVDATA_H_

#include "ina226.h"

typedef struct {
	String ssid;
	String password;
	uint16_t averaging;
	uint16_t busConv;
	uint16_t shuntConv;
	uint16_t scale;
	uint16_t sampleSecs;
	} CONFIG_t;

extern CONFIG_t Config;

void nv_config_store(CONFIG_t &config);
void nv_config_load(CONFIG_t &config);
void nv_config_reset(CONFIG_t &config);
void nv_config_print(CONFIG_t &config);

#endif
