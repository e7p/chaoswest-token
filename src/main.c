#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#include "temperature.h"
#include "i2cusb.h"
#include "light_ws2812.h"

#define LED_R (1 << PB5)
#define LED_G (1 << PB0)
#define LED_B (1 << PB1)

#define NUM_LEDS 6
struct cRGB leds[NUM_LEDS];
struct cRGB fade[NUM_LEDS];

struct cRGB frame[NUM_LEDS];

uint8_t config_brightness EEMEM = 0x80;
uint8_t config_osccal EEMEM;

uint8_t brightness_cache;
uint8_t pwmClock = 0;
uint8_t animation = 1;

const uint8_t pwmTable[] PROGMEM = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x10, 0x10,
	0x10, 0x11, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x16, 0x17,
	0x17, 0x18, 0x18, 0x19, 0x19, 0x1a, 0x1a, 0x1b, 0x1b, 0x1c, 0x1d, 0x1d, 0x1e, 0x1f, 0x1f, 0x20,
	0x21, 0x21, 0x22, 0x23, 0x24, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x29, 0x2a, 0x2b, 0x2c, 0x2d,
	0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3b, 0x3c, 0x3d, 0x3e, 0x40,
	0x41, 0x43, 0x44, 0x46, 0x47, 0x49, 0x4a, 0x4c, 0x4e, 0x4f, 0x51, 0x53, 0x55, 0x56, 0x58, 0x5a,
	0x5c, 0x5e, 0x60, 0x62, 0x65, 0x67, 0x69, 0x6b, 0x6e, 0x70, 0x72, 0x75, 0x78, 0x7a, 0x7d, 0x80,
	0x82, 0x85, 0x88, 0x8b, 0x8e, 0x91, 0x94, 0x98, 0x9b, 0x9e, 0xa2, 0xa5, 0xa9, 0xad, 0xb0, 0xb4,
	0xb8, 0xbc, 0xc0, 0xc5, 0xc9, 0xcd, 0xd2, 0xd6, 0xdb, 0xe0, 0xe5, 0xea, 0xef, 0xf4, 0xfa, 0xff
};

uint8_t invalidate_ws2813 = 0;

void updateFrame(uint8_t index, uint8_t channel) {
	uint8_t temp_color = ((uint16_t)((uint8_t*)(leds + index))[channel]) * brightness_cache / 255;
	if (index == 0) {
		temp_color = pgm_read_byte(&(pwmTable[temp_color]));
	}
	((uint8_t*)(frame + index))[channel] = temp_color;
	if (index > 0) {
		invalidate_ws2813 = 1;
	}
}

void setFadeColorValue(uint8_t index, uint8_t channel, uint8_t value) {
	((uint8_t*)(fade + index))[channel] = value;
}

void setColorValue(uint8_t index, uint8_t channel, uint8_t value) {
	((uint8_t*)(leds + index))[channel] = value;
	((uint8_t*)(fade + index))[channel] = value;
	updateFrame(index, channel);
}

uint8_t getColorValue(uint8_t index, uint8_t channel) {
	return ((uint8_t*)(leds + index))[channel];
}

int main(void) {
	// Initialize Temperature Sensor and USB
	usb_setup();
	temperature_setup();

	// Initialize GPIOs
	PORTB |= LED_R | LED_G | LED_B;
	DDRB |= LED_R | LED_G | LED_B;

	// Load the stored LED Brightness
	brightness_cache = eeprom_read_byte(&config_brightness);

	// Set the back WS2813 LEDs
	leds[1].r=255;leds[1].g=0;leds[1].b=0;
	leds[2].r=255;leds[2].g=255;leds[2].b=0;	
	leds[3].r=0;leds[3].g=255;leds[3].b=0;
	leds[4].r=0;leds[4].g=0;leds[4].b=255;
	leds[5].r=255;leds[5].g=0;leds[5].b=255;
	for(uint8_t i = 0; i < NUM_LEDS; i++) {
		for(uint8_t j = 0; j < 3; j++) {
			updateFrame(i, j);
		}
	}

    uint16_t virtual_timer = 0;
    uint8_t step = 0;
    for(;;)
	{
		// Update WS2813
		/*if (invalidate_ws2813 == 1) {
			invalidate_ws2813 = 0;
			PORTB &= ~LED_B;
			_delay_ms(100);
			ws2812_setleds(frame+1,5);
			_delay_ms(100);
			PORTB |= LED_B;
		}*/

		virtual_timer++;
		if (virtual_timer == 0) {
			temperature_measure(); // Start Temperature Measurement

			// Front LED Animation
			if (animation) {
				switch (step) {
					case 0:
					setFadeColorValue(0, 0, 255);
					setFadeColorValue(0, 1, 0);
					setFadeColorValue(0, 2, 0);
					break;

					case 1:
					setFadeColorValue(0, 0, 255);
					setFadeColorValue(0, 1, 255);
					setFadeColorValue(0, 2, 0);
					break;

					case 2:
					setFadeColorValue(0, 0, 0);
					setFadeColorValue(0, 1, 255);
					setFadeColorValue(0, 2, 0);
					break;

					case 3:
					setFadeColorValue(0, 0, 0);
					setFadeColorValue(0, 1, 255);
					setFadeColorValue(0, 2, 255);
					break;

					case 4:
					setFadeColorValue(0, 0, 0);
					setFadeColorValue(0, 1, 0);
					setFadeColorValue(0, 2, 255);
					break;

					case 5:
					setFadeColorValue(0, 0, 255);
					setFadeColorValue(0, 1, 0);
					setFadeColorValue(0, 2, 255);
					step = -1;
					break;

					default:
					break;
				}
				step++;
			}
		}
		
		// Software PWM of Front LED
		if (pwmClock < frame[0].r) {
			PORTB &= ~LED_R;
		} else {
			PORTB |= LED_R;
		}
		if (pwmClock < frame[0].g) {
			PORTB &= ~LED_G;
		} else {
			PORTB |= LED_G;
		}
		if (pwmClock < frame[0].b) {
			PORTB &= ~LED_B;
		} else {
			PORTB |= LED_B;
		}

		pwmClock++;

		// Calculate next Color
		if (pwmClock == 0) {
			for (uint8_t j = 0; j < NUM_LEDS; j++) {
				for (uint8_t i = 0; i < 3; i++) {
					if (((uint8_t*)(leds + j))[i] > ((uint8_t*)(fade + j))[i]) {
						((uint8_t*)(leds + j))[i]--;
						updateFrame(j, i);
					} else if (((uint8_t*)leds + j)[i] < ((uint8_t*)(fade + j))[i]) {
						((uint8_t*)(leds + j))[i]++;
						updateFrame(j, i);
					}
				}
			}
		}

		// USB Loop
		usb_loop();
	}

    return 0; 
}