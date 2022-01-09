#ifndef NVDATA_H_
#define NVDATA_H_




typedef struct {
	String ssid;
	String password;
	} OPTIONS_t;



extern OPTIONS_t Options;

void nv_options_store(OPTIONS_t &options);
void nv_options_load(OPTIONS_t &options);
void nv_options_reset(OPTIONS_t &options);
void nv_options_print(OPTIONS_t &options);

#endif
