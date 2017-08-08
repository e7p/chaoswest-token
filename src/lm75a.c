#include <avr/io.h>
#include "temperature.h"

uint8_t lm75a_data_length;
uint8_t lm75a_data_buffer[2];

void lm75a_handle(uint8_t cmd) {
	uint16_t lm75a_value;
	cmd &= 0x07;

	switch (cmd) {
		case 0x00:
		// Current Temperature
		lm75a_value = celsiusToLM75A(adcToCelsius(temperature_get()));
		lm75a_data_buffer[0] = (lm75a_value >> 8) & 0xFF;
		lm75a_data_buffer[1] = lm75a_value & 0xFF;
		lm75a_data_length = 2;
		break;

		case 0x01:
		// Configuration
		lm75a_data_buffer[0] = 0x00;
		lm75a_data_length = 1;
		break;

		case 0x02:
		// Hysteresis Register
		lm75a_data_buffer[0] = 0x2A;
		lm75a_data_buffer[1] = 0x3A;
		lm75a_data_length = 2;
		break;

		case 0x03:
		// Overtemperature Threshold
		lm75a_data_buffer[0] = 0x85;
		lm75a_data_buffer[1] = 0xB3;
		lm75a_data_length = 2;
		break;

		case 0x04:
		case 0x05:
		case 0x06:
		// invalid register
		lm75a_data_buffer[0] = 0xFF;
		lm75a_data_length = 1;
		break;

		case 0x07:
		// Device ID
		lm75a_data_buffer[0] = 0xA1;
		lm75a_data_length = 1;
		break;
	}
}