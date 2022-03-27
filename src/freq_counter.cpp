
// Modified from : 
// BLOG Eletrogate
// ESP32 Frequency Meter
// https://blog.eletrogate.com/esp32-frequencimetro-de-precisao
// Rui Viana and Gustavo Murta august/2020
#include <Arduino.h>
#include <stdio.h>                            // Library STDIO
#include <driver/ledc.h>                      // Library ESP32 LEDC
#include <soc/pcnt_struct.h>
#include <driver/pcnt.h>                      // Library ESP32 PCNT
#include "config.h"
#include "freq_counter.h"

static const char* TAG = "freq_meter";

#define PCNT_COUNT_UNIT       PCNT_UNIT_0     // Set Pulse Counter Unit - 0 
#define PCNT_COUNT_CHANNEL    PCNT_CHANNEL_0  // Set Pulse Counter channel - 0 

#define PCNT_INPUT_SIG_IO     GPIO_NUM_34     // Set Pulse Counter input - Freq Meter Input GPIO 34
#define LEDC_HS_CH0_GPIO      GPIO_NUM_33     // Saida do LEDC - gerador de pulsos - GPIO_33
#define PCNT_INPUT_CTRL_IO    GPIO_NUM_35     // Set Pulse Counter Control GPIO pin - HIGH = count up, LOW = count down  
#define OUTPUT_CONTROL_GPIO   GPIO_NUM_32     // Timer output control port - GPIO_32
#define PCNT_H_LIM_VAL        overflow        // Overflow of Pulse Counter 

volatile bool FreqReadyFlag = false;
volatile bool FreqCaptureFlag = false;
volatile int  FrequencyHz   = 0;           // frequency value
volatile SemaphoreHandle_t FreqSemaphore;

uint32_t OscFreqHz = 23456; // 1Hz to 40MHz
bool OscFreqFlag = false;


uint32_t        overflow      = 20000;       // Max Pulse Counter value
int16_t         pulses        = 0;           // Pulse Counter value
uint32_t        multPulses    = 0;           // Quantidade de overflows do contador PCNT
uint32_t        sample_time   = 999955;     // sample time of 1 second to count pulses
uint32_t        mDuty         = 0;           // Duty value
uint32_t        resolution    = 0;           // Resolution value
char            buf[32];                     // Buffer

esp_timer_create_args_t create_args;         // Create an esp_timer instance
esp_timer_handle_t timer_handle;             // Create an single timer

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; // portMUX_TYPE to do synchronism


static void read_PCNT(void *p);                                 // Read Pulse Counter
static char *ultos_recursive(unsigned long val, char *s, unsigned radix, int pos); // Format an unsigned long (32 bits) into a string
static char *ltos(long val, char *s, int radix);                // Format an long (32 bits) into a string
static void init_PCNT(void);                                    // Initialize and run PCNT unit
static void IRAM_ATTR pcnt_intr_handler(void *arg);             // Counting overflow pulses
static void init_osc_freq ();                                   // Initialize Oscillator to test Freq Meter
static void init_frequency_meter ();


// Initialize Oscillator to test Freq Meter
static void init_osc_freq() {
	resolution = (log (80000000 / OscFreqHz)  / log(2)) / 2 ;     // Calc of resolution of Oscillator
	if (resolution < 1) resolution = 1;                          // set min resolution 
	// Serial.println(resolution);                               // Print
	mDuty = (pow(2, resolution)) / 2;                            // Calc of Duty Cycle 50% of the pulse
	// Serial.println(mDuty);                                    // Print

	ledc_timer_config_t ledc_timer = {};                         // LEDC timer config instance

	ledc_timer.duty_resolution =  ledc_timer_bit_t(resolution);  // Set resolution
	ledc_timer.freq_hz    = OscFreqHz;                            // Set Oscillator frequency
	ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;                // Set high speed mode
	ledc_timer.timer_num = LEDC_TIMER_0;                         // Set LEDC timer index - 0
	ledc_timer_config(&ledc_timer);                              // Set LEDC Timer config
	ledc_channel_config_t ledc_channel = {};                     // LEDC Channel config instance

	ledc_channel.channel    = LEDC_CHANNEL_0;              // Set HS Channel - 0
	ledc_channel.duty       = mDuty;                       // Set Duty Cycle 50%
	ledc_channel.gpio_num   = LEDC_HS_CH0_GPIO;            // LEDC Oscillator output GPIO 33
	ledc_channel.intr_type  = LEDC_INTR_DISABLE;           // LEDC Fade interrupt disable
	ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;        // Set LEDC high speed mode
	ledc_channel.timer_sel  = LEDC_TIMER_0;                // Set timer source of channel - 0
	ledc_channel_config(&ledc_channel);                    // Config LEDC channel
	}


// Counting overflow pulses
static void IRAM_ATTR pcnt_intr_handler(void *arg) {
	portENTER_CRITICAL_ISR(&timerMux);                     // disabling the interrupts
	multPulses++;                                          // increment Overflow counter
	PCNT.int_clr.val = BIT(PCNT_COUNT_UNIT);               // Clear Pulse Counter interrupt bit
	portEXIT_CRITICAL_ISR(&timerMux);                      // enabling the interrupts
	}


// Initialize and run PCNT unit
static void init_PCNT(void) {
	pcnt_config_t pcnt_config = { };                       // PCNT unit instance

	pcnt_config.pulse_gpio_num = PCNT_INPUT_SIG_IO;        // Pulse input GPIO 34 - Freq Meter Input
	pcnt_config.ctrl_gpio_num = PCNT_INPUT_CTRL_IO;        // Control signal input GPIO 35
	pcnt_config.unit = PCNT_COUNT_UNIT;                    // Unidade de contagem PCNT - 0
	pcnt_config.channel = PCNT_COUNT_CHANNEL;              // PCNT unit number - 0
	pcnt_config.counter_h_lim = PCNT_H_LIM_VAL;            // Maximum counter value - 20000
	pcnt_config.pos_mode = PCNT_COUNT_INC;                 // PCNT positive edge count mode - inc
	pcnt_config.neg_mode = PCNT_COUNT_INC;                 // PCNT negative edge count mode - inc
	pcnt_config.lctrl_mode = PCNT_MODE_DISABLE;            // PCNT low control mode - disable
	pcnt_config.hctrl_mode = PCNT_MODE_KEEP;               // PCNT high control mode - won't change counter mode
	pcnt_unit_config(&pcnt_config);                        // Initialize PCNT unit

	pcnt_counter_pause(PCNT_COUNT_UNIT);                   // Pause PCNT unit
	pcnt_counter_clear(PCNT_COUNT_UNIT);                   // Clear PCNT unit

	pcnt_event_enable(PCNT_COUNT_UNIT, PCNT_EVT_H_LIM);    // Enable event to watch - max count
	pcnt_isr_register(pcnt_intr_handler, NULL, 0, NULL);   // Setup Register ISR handler
	pcnt_intr_enable(PCNT_COUNT_UNIT);                     // Enable interrupts for PCNT unit

	pcnt_counter_resume(PCNT_COUNT_UNIT);                  // Resume PCNT unit - starts count
	}


// Read Pulse Counter
static void read_PCNT(void *p){
	gpio_set_level(OUTPUT_CONTROL_GPIO, 0);             // Stop counter - output control LOW
	pcnt_get_counter_value(PCNT_COUNT_UNIT, &pulses);   // Read Pulse Counter value
	xSemaphoreGive(FreqSemaphore);
	}


static void init_frequency_meter (){
	init_osc_freq();                                                        // Initialize Oscillator
	init_PCNT();                                                            // Initialize and run PCNT unit

	gpio_pad_select_gpio(OUTPUT_CONTROL_GPIO);                              // Set GPIO pad
	gpio_set_direction(OUTPUT_CONTROL_GPIO, GPIO_MODE_OUTPUT);              // Set GPIO 32 as output

	create_args.callback = read_PCNT;                                       // Set esp-timer argument
	esp_timer_create(&create_args, &timer_handle);                          // Create esp-timer instance

	gpio_matrix_in(PCNT_INPUT_SIG_IO, SIG_IN_FUNC226_IDX, false);           // Set GPIO matrin IN - Freq Meter input
	}

#if 0
// Format an unsigned long (32 bits) into a string
static char *ultos_recursive(unsigned long val, char *s, unsigned radix, int pos) {
	int c;
	if (val >= radix) {
		s = ultos_recursive(val / radix, s, radix, pos + 1);
		}
	c = val % radix;
	c += (c < 10 ? '0' : 'a' - 10);
	*s++ = c;
	if (pos % 3 == 0) {
		*s++ = ',';
		}
	return s;
	}


// Format an long (32 bits) into a string
static char *ltos(long val, char *s, int radix) {              
	if (radix < 2 || radix > 36) {
		s[0] = 0;
		} 
	else {
		char *p = s;
		if (radix == 10 && val < 0) {
			val = -val;
			*p++ = '-';
			}
		p = ultos_recursive(val, p, radix, 0) - 1;
		*p = 0;
		}
	return s;
	}
#endif

void frequency_task(void* pvParam){
    ESP_LOGI(TAG, "frequency_task running on core %d with priority %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

	FreqSemaphore = xSemaphoreCreateBinary();
	init_frequency_meter();
	pcnt_counter_clear(PCNT_COUNT_UNIT);               // Clear Pulse Counter
	esp_timer_start_once(timer_handle, sample_time);   // Initialize High resolution timer (1 sec)
	gpio_set_level(OUTPUT_CONTROL_GPIO, 1);            // Set enable PCNT count
	while (1) {
        xSemaphoreTake(FreqSemaphore, portMAX_DELAY); 
		FrequencyHz = (pulses + (multPulses * overflow)) / 2  ;  // Calculation of FrequencyHz
		FreqReadyFlag = true;
		//Serial.printf("Frequency : %d Hz\n", FrequencyHz);  // Print FrequencyHz with commas

		multPulses = 0;                                    // Clear overflow counter
		// Put your function here, if you want
		//vTaskDelay(10);                                      
		// Put your function here, if you want

		pcnt_counter_clear(PCNT_COUNT_UNIT);               // Clear Pulse Counter
		esp_timer_start_once(timer_handle, sample_time);   // Initialize High resolution timer (1 sec)
		gpio_set_level(OUTPUT_CONTROL_GPIO, 1);            // Set enable PCNT count

		if (OscFreqFlag == true) {
			OscFreqFlag = false;
			init_osc_freq();
			}		
		}
	vTaskDelete(NULL);
}