#ifndef MY_SENSOR_H
#define MY_SENSOR_H

#include "net/linkaddr.h"
#include "stdio.h"



int get_light_lux(int raw_value);
int get_distance(int raw_value);
int get_millivolts(uint16_t saadc_value);

#endif
