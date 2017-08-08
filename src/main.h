#include "light_ws2812.h"

#define NUM_LEDS 6
void setColorValue(uint8_t index, uint8_t channel, uint8_t value);
uint8_t getColorValue(uint8_t index, uint8_t channel);

extern uint8_t config_brightness EEMEM;
extern uint8_t brightness_cache;
extern uint8_t animation;

void updateFrame(uint8_t index, uint8_t channel);