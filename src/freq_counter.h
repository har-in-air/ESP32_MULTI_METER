#ifndef FREQ_METER_H_
#define FREQ_METER_H_

extern volatile bool FreqReadyFlag;
extern volatile bool FreqCaptureFlag;
extern volatile int FrequencyHz;

#define MSG_TX_FREQUENCY 5555

void frequency_task(void* pvParam);

#endif