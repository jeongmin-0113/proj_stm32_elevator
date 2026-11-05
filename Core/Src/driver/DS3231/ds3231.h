/*
 * ds3231.h
 *
 *  Created on: 2026. 10. 22.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_DS3231_DS3231_H_
#define SRC_DRIVER_DS3231_DS3231_H_

#include "stm32f4xx_hal.h"

typedef struct {
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
} ds3231_time_t;

typedef struct {
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
} ds3231_date_t;

typedef struct {
	ds3231_time_t time;
	ds3231_date_t date;
} ds3231_datetime_t;

void ds3231_init(I2C_HandleTypeDef* hi2c);
void ds3231_getTime(ds3231_time_t *t);
void ds3231_setTime(const ds3231_time_t *t);
void ds3231_getDate(ds3231_date_t *d);
void ds3231_setDate(const ds3231_date_t *d);
void ds3231_getDateTime(ds3231_datetime_t *dt);
void ds3231_setDateTime(const ds3231_datetime_t *dt);

#endif /* SRC_DRIVER_DS3231_DS3231_H_ */
