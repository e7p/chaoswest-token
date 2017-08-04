void temperature_setup(void);
void temperature_measure(void);
uint16_t temperature_get(void);
float adcToCelsius(uint16_t adc);
uint16_t celsiusToDS1721(float celsius);
