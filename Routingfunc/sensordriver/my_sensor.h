#ifndef MY_SENSOR_H
#define MY_SENSOR_H

#include "net/linkaddr.h"
#include "stdio.h"

typedef struct sensor_data
{
    uint8_t type;
    linkaddr_t  source;
    int light_lux;
    int distance;
    int battery;
    int temperature;
    /* data */
}sensor_data;


int get_light_lux(int raw_value);
int get_distance(int raw_value);
int get_millivolts(uint16_t saadc_value);

#endif
