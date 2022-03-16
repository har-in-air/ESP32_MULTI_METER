#include <Arduino.h>
#include <Wire.h>  
#include <FS.h>
#include <LittleFS.h>
#include "config.h"
#include "nv_data.h"
#include "wifi_cfg.h"
#include "ina226.h"
#include "freq_counter.h"

const char* FwRevision = "0.96";
static const char* TAG = "main";

volatile int TxSamples;
volatile bool SocketConnectedFlag = false;
uint32_t ClientID;

#define WIFI_TASK_PRIORITY 				1
#define CURRENT_VOLTAGE_TASK_PRIORITY 	(configMAX_PRIORITIES-1)
#define FREQUENCY_TASK_PRIORITY 		(configMAX_PRIORITIES-2)

volatile MEASURE_t Measure;
volatile int16_t* Buffer = NULL; 
int MaxSamples;

#define ST_IDLE 			1
#define ST_TX				2
#define ST_TX_COMPLETE		3
#define ST_METER_COMPLETE	4
#define ST_FREQ_COMPLETE	5

static void wifi_task(void* pvParameter);
static void current_voltage_task(void* pvParameter);
static void reset_flags();

// create the desired tasks, and then delete arduino created loopTask that calls setup() and loop(). 
// Core 0 : wifi task with web server and websocket communication, and low level esp-idf wifi code
// Core 1 : capture task

void setup() {
	pinMode(pinAlert, INPUT); // external pullup, active low
	pinMode(pinGate, INPUT); // external pullup, active low
	pinMode(pinFET1, OUTPUT); // external pulldown
	pinMode(pinFET05, OUTPUT); // external pulldown
	pinMode(pinLED, OUTPUT); 
	digitalWrite(pinLED, LOW);

	Serial.begin(115200);
	ESP_LOGI(TAG,"ESP32_INA226 v%s compiled on %s at %s\n\n", FwRevision, __DATE__, __TIME__);
    ESP_LOGI(TAG, "Max task priority = %d", configMAX_PRIORITIES-1);
    ESP_LOGI(TAG, "arduino loopTask : setup() running on core %d with priority %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));    

	nv_options_load(Options);

	Measure.mode = MODE_CURRENT_VOLTAGE;

	// web server and web socket connection handler on core 0 along with low level wifi actions (ESP-IDF code)
    xTaskCreatePinnedToCore(&wifi_task, "wifi_task", 4096, NULL, WIFI_TASK_PRIORITY, NULL, CORE_0);
    // frequency_task on core 1, lower priority than cv capture task 
	xTaskCreatePinnedToCore(&frequency_task, "freq_task", 4096, NULL, FREQUENCY_TASK_PRIORITY, NULL, CORE_1);
    // current_voltage_task on core 1, don't want i2c capture to be pre-empted as far as possible to maintain sampling rate. 
	xTaskCreatePinnedToCore(&current_voltage_task, "cv_task", 4096, NULL, CURRENT_VOLTAGE_TASK_PRIORITY, NULL, CORE_1);

	// destroy loopTask which called setup() from arduino:app_main()
    vTaskDelete(NULL);
    }


// never called as loopTask is deleted, but needs to be defined
void loop(){
	}

void reset_flags() {			
	DataReadyFlag = false;
	GateOpenFlag = false;
	EndCaptureFlag = false;
	MeterReadyFlag = false;
	FreqReadyFlag = false;
	LastPacketAckFlag = false;
	}

static void wifi_task(void* pVParameter) {
    ESP_LOGD(TAG, "wifi_task running on core %d with priority %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));
	ESP_LOGI(TAG,"Starting web server");
    // do NOT format, partition is built and flashed using 
	// 1. PlatformIO Build FileSystem Image
	// 2. Upload FileSystem Image    
    if (!LittleFS.begin(false)) { 
		ESP_LOGE(TAG, "Cannot mount LittleFS, Rebooting");
		delay(1000);
		ESP.restart();
		}    
	// initialize web server and web socket interface	
	wifi_init();
	int state = ST_IDLE;
	int bufferOffset = 0;
	int numBytes;
	volatile int16_t* pb;
	int16_t msg;
	uint32_t t1, t2;
	reset_flags();

	while (1) {
		vTaskDelay(1);
		ws.cleanupClients();
		if (SocketConnectedFlag == true) { 
			switch (Measure.mode) {
				default :
				break;

				case MODE_CURRENT_VOLTAGE :
				switch (state) {
					default :
					break;

					case ST_IDLE :
					if (MeterReadyFlag == true) {
						MeterReadyFlag = false;
						LastPacketAckFlag = false;
						numBytes = 4 * sizeof(int16_t);
						ws.binary(ClientID, (uint8_t*)Buffer, numBytes);
						state = ST_METER_COMPLETE; 
						}
					else					
					if (GateOpenFlag){
						GateOpenFlag = false;
						ESP_LOGD(TAG,"Socket msg : Capture Gate Open");
						msg = MSG_GATE_OPEN;
						ws.binary(ClientID, (uint8_t*)&msg, 2); 
						}	
					else 
					if (DataReadyFlag == true) {
						DataReadyFlag = false;
						ESP_LOGD(TAG,"Socket msg : Tx Start");		
						numBytes = (3 + TxSamples*2)*sizeof(int16_t);
						t1 = micros();
						ws.binary(ClientID, (uint8_t*)Buffer, numBytes); 
						bufferOffset += numBytes/2; // in uint16_ts
						if (EndCaptureFlag == true) {
							EndCaptureFlag = false;
							state = ST_TX_COMPLETE;
							}
						else {
							state = ST_TX;
							}
						}
					break;

					case ST_TX :
					// wait for next packet ready and last packet receive acknowledgement 
					// before transmitting next packet
					if ((DataReadyFlag == true) && (LastPacketAckFlag == true)) {
						LastPacketAckFlag = false;
						DataReadyFlag = false;
						t2 = micros();
						ESP_LOGD(TAG,"Socket msg : %dus, Tx ...", t2-t1);
						t1 = t2;
						pb = Buffer + bufferOffset; 
						numBytes = (1 + TxSamples*2)*sizeof(int16_t);
						ws.binary(ClientID, (uint8_t*)pb, numBytes); 
						bufferOffset += numBytes/2; // in uint16_ts
						if (EndCaptureFlag == true) {
							EndCaptureFlag = false;
							state = ST_TX_COMPLETE;
							}
						}						
					break;

					case ST_TX_COMPLETE :
					// wait for last packet receive acknowledgement before transmitting
					// capture end message
					if (LastPacketAckFlag == true) {
						t2 = micros();
						ESP_LOGD(TAG,"Socket msg : %dus, Tx ...", t2-t1);
						ESP_LOGD(TAG,"Socket msg : Tx Complete");
						msg = MSG_TX_COMPLETE;
						ws.binary(ClientID, (uint8_t*)&msg, 2); 
						reset_flags();
						state = ST_IDLE;
						TxSamples = 0;
						bufferOffset = 0;
						}
					break;

					case ST_METER_COMPLETE :
					if (LastPacketAckFlag == true) {
						reset_flags();
						state = ST_IDLE;
						}
					break;					
					}
				break;
			
				case MODE_FREQUENCY :
				switch (state) {
					default :
					break;

					case ST_IDLE:
					if (FreqReadyFlag == true) {
						FreqReadyFlag = false;
						LastPacketAckFlag = false;
						int32_t buffer[2];
						buffer[0] = MSG_TX_FREQUENCY;
						buffer[1] = FrequencyHz;
						numBytes = 2*sizeof(int32_t);
						ws.binary(ClientID, (uint8_t*)buffer, numBytes);
						state = ST_FREQ_COMPLETE; 
						}
					break;

					case ST_FREQ_COMPLETE :
					if (LastPacketAckFlag == true) {
						reset_flags();
						state = ST_IDLE;
						}
					break;					
					}
				break;
				}
			}
		else {
			// socket disconnection, reset state and flags
			reset_flags();
			state = ST_IDLE;
			}
		}
	vTaskDelete(NULL);		
	}


static void current_voltage_task(void* pvParameter)  {	
    ESP_LOGI(TAG, "current_voltage_task running on core %d with priority %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));
	CVCaptureFlag = false;
	Wire.begin(pinSDA,pinSCL); 
	Wire.setClock(400000);
	uint16_t id = ina226_read_reg(REG_ID);
	if (id != 0x5449) {
		ESP_LOGE(TAG,"INA226 Manufacturer ID read = 0x%04X, expected 0x5449\n", id);
		ESP_LOGE(TAG,"Halting...");
		while (1){
			vTaskDelay(1);
			}
		}

	ina226_reset();

	// get largest malloc-able block of byte-addressable free memory
	int32_t maxBufferBytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
	ESP_LOGI(TAG, "Free memory malloc-able for sample Buffer = %d bytes", maxBufferBytes);

	Buffer = (int16_t*)malloc(maxBufferBytes);
	if (Buffer == nullptr) {
		ESP_LOGE(TAG, "Could not allocate sample Buffer with %d bytes", maxBufferBytes);
		ESP_LOGE(TAG,"Halting...");
		while (1){
			vTaskDelay(1);
			}
		}

	MaxSamples = (maxBufferBytes - 8)/4;
	ESP_LOGI(TAG, "Max Samples = %d", MaxSamples);
#if 0
	ina226_test_capture();	
#endif

	while (1){
			if (CVCaptureFlag == true) {
				CVCaptureFlag = false;
				if (Measure.m.cv_meas.nSamples == 0) {
					ESP_LOGD(TAG,"Capturing gated samples using cfg = 0x%04X, scale %d\n", Measure.m.cv_meas.cfg, Measure.m.cv_meas.scale );
					ina226_capture_gated(Measure, Buffer);
					}
				else 
				if (Measure.m.cv_meas.nSamples == 1) {
					ina226_capture_oneshot(Measure, Buffer);
					}
				else {
					ESP_LOGD(TAG,"Capturing %d samples using cfg = 0x%04X, scale %d\n", Measure.m.cv_meas.nSamples, Measure.m.cv_meas.cfg, Measure.m.cv_meas.scale );
					ina226_capture_triggered(Measure, Buffer);
					}
				}
			vTaskDelay(1);
			}
	vTaskDelete(NULL);		
	}


