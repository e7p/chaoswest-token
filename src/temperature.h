void temperature_setup(void);
void temperature_measure(void);
uint8_t temperature_done(void);
uint16_t temperature_get(void);
float adcToCelsius(uint16_t adc);
uint16_t celsiusToLM75A(float celsius);
