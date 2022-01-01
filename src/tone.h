#ifndef TONE_H_
#define TONE_H_


void tone_generate(int pin, int frequencyHz, int milliSeconds);
void beep(int pin, int freqHz, int onMs, int offMs, int numBeeps);

#endif
