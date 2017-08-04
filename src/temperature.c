#include <avr/io.h>
#include <avr/interrupt.h>

#define TEMP_ZERO_ADC_OFFSET 200
#define TEMP_ADC_PER_CELSIUS 6.0

void temperature_setup(void) {
    // Setup ADC (2.56 V reference, Interrupt)
	ADMUX = (1 << REFS2) | (1 << REFS1) | (1 << MUX0);
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2);
	sei();
}

void temperature_measure(void) {
	ADCSRA |= (1 << ADSC);
}

uint16_t temp = 0;

ISR(ADC_vect)
{
	temp = ADCL;
	temp |= (ADCH << 8);
}

uint16_t temperature_get(void) {
	return temp;
}

float adcToCelsius(uint16_t adc) {
	return ((float)(adc - TEMP_ZERO_ADC_OFFSET)) / TEMP_ADC_PER_CELSIUS;
}

uint16_t celsiusToDS1721(float celsius) {
	uint8_t tempFull = (uint8_t)celsius;
	celsius -= tempFull;
	uint8_t tempRemain = 0;
	for (uint8_t i = 0; i < 4; i++) {
		tempRemain <<= 1;
		celsius = celsius * 2.0;
		if (celsius >= 1.0) {
			tempRemain |= 0x10;
		}
	}
	return (tempFull << 8) | tempRemain;
}