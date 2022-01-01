#include <Arduino.h>
#include "tone.h"

void tone_generate(int pin, int freqHz, int milliseconds) {
  uint8_t channel = 0;
   ledcAttachPin(pin, channel);
   ledcWriteTone(channel, freqHz);
   delay(milliseconds);
   ledcWrite(channel, 0);
   ledcDetachPin(pin);
   pinMode(pin, OUTPUT);
   digitalWrite(pin, 0);
  }
  
void beep(int pin, int freqHz, int onMs, int offMs, int numBeeps) {
  while(numBeeps--) {
    tone_generate(pin,freqHz, onMs);
	  delay(offMs);
  	}
  }
