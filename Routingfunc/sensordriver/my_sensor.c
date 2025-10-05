#include "my_sensor.h"

int get_light_lux(int raw_value){
	float lux;
	float raw_voltage;
	//float regularized_voltage;

	raw_voltage = (float)raw_value/4096*3.6;
	//regularized_voltage = raw_voltage * 5/3.6;
	lux = 2.0055 * 200 * raw_voltage + 42.879;

	return (int)lux;
}

int get_distance(int raw_value){
	float dis;
	float voltage_ratio;

	voltage_ratio = (float)raw_value/4096;
	dis = 4.8/(voltage_ratio-0.02);
	
	return (int)dis;
}

int get_millivolts(uint16_t saadc_value){
	// V_ref = 600mV
	// gain = 1/6
	// resolution = 2^12 = 4096
	// converted = raw * (V_ref / gain) / resultion
	return (int)((saadc_value * 3600UL) / 4096);
}
