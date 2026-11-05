/*
 * ds3231.c
 *
 *  Created on: 2026. 10. 22.
 *      Author: kccistc
 */
#include "ds3231.h"

#define DS3231_I2C_ADR		(0x68<<1)
#define DS3231_TIMEOUT_MS	10

#define DS3231_SEC			0x00
#define DS3231_MIN			0x01
#define DS3231_HOUR			0x02
#define DS3231_DAY			0x03
#define DS3231_DATE			0x04
#define DS3231_MONTH		0x05
#define DS3231_YEAR			0x06

I2C_HandleTypeDef* ds3231_hi2c;

uint8_t bcd2bin(uint8_t v) {
	return ((v >> 4) * 10) + (v & 0x0f);
}

uint8_t bin2bcd(uint8_t v) {
	return ((v / 10) << 4) | (v % 10);
}

void ds3231_init(I2C_HandleTypeDef* hi2c) {
	ds3231_hi2c = hi2c;
}

void ds3231_getTime(ds3231_time_t *t) {
	uint8_t buf[3];
//	uint8_t ds3231_regAddr = DS3231_SEC;

	// i2c 통신으로 ds3231 register 값을 read
//	HAL_I2C_Master_Transmit(hi2c1, DS3231_I2C_ADR, &ds3231_regAddr, 1,
//	DS3231_TIMEOUT_MS);
//	HAL_I2C_Master_Receive(hi2c1, DS3231_I2C_ADR, buf, 3,
//	DS3231_TIMEOUT_MS);

// repeated start 지원하는 hal 메서드 (중간에 stop sign 없음)
	HAL_I2C_Mem_Read(ds3231_hi2c, DS3231_I2C_ADR, DS3231_SEC, 1, buf, 3,
	DS3231_TIMEOUT_MS);

	t->sec = bcd2bin(buf[0] & 0x7f);
	t->min = bcd2bin(buf[1] & 0x7f);
	t->hour = bcd2bin(buf[2] & 0x7f);
}

// const: 매개변수 포인터 접근만 하고 변경 금지
void ds3231_setTime(const ds3231_time_t *t) {
	uint8_t buf[4];
	buf[0] = DS3231_SEC;
	buf[1] = bin2bcd(t->sec);
	buf[2] = bin2bcd(t->min);
	buf[3] = bin2bcd(t->hour);

	HAL_I2C_Master_Transmit(ds3231_hi2c, DS3231_I2C_ADR, buf, 4, DS3231_TIMEOUT_MS);
	// 이렇게도 된다~
//	HAL_I2C_Mem_Write(hi2c1, DS3231_I2C_ADR, DS3231_SEC, 1, buf, 3, DS3231_TIMEOUT_MS);
}

void ds3231_getDate(ds3231_date_t *d) {
	uint8_t buf[4];
//	uint8_t ds3231_regAddr = DS3231_DAY;

	// i2c 통신으로 ds3231 register 값을 read
//	HAL_I2C_Master_Transmit(hi2c1, DS3231_I2C_ADR, &ds3231_regAddr, 1,
//	DS3231_TIMEOUT_MS);
//	HAL_I2C_Master_Receive(hi2c1, DS3231_I2C_ADR, buf, 4,
//	DS3231_TIMEOUT_MS);

// repeated start 지원하는 hal 메서드 (중간에 stop sign 없음)
	HAL_I2C_Mem_Read(ds3231_hi2c, DS3231_I2C_ADR, DS3231_DAY, 1, buf, 4,
	DS3231_TIMEOUT_MS);

	d->day = bcd2bin(buf[0] & 0x0f);
	d->date = bcd2bin(buf[1] & 0x3f);
	d->month = bcd2bin(buf[2] & 0x1f);
	d->year = bcd2bin(buf[3]);
}

void ds3231_setDate(const ds3231_date_t *d) {
	uint8_t buf[5];
	buf[0] = DS3231_DAY;
	buf[1] = bin2bcd(d->day);
	buf[2] = bin2bcd(d->date);
	buf[3] = bin2bcd(d->month);
	buf[4] = bin2bcd(d->year);

	HAL_I2C_Master_Transmit(ds3231_hi2c, DS3231_I2C_ADR, buf, 5, DS3231_TIMEOUT_MS);
}

void ds3231_getDateTime(ds3231_datetime_t *dt) {
	uint8_t buf[7];
//	uint8_t ds3231_regAddr = DS3231_SEC;

	// i2c 통신으로 ds3231 register 값을 read
//	HAL_I2C_Master_Transmit(hi2c1, DS3231_I2C_ADR, &ds3231_regAddr, 1,
//	DS3231_TIMEOUT_MS);
//	HAL_I2C_Master_Receive(hi2c1, DS3231_I2C_ADR, buf, 7,
//	DS3231_TIMEOUT_MS);

// repeated start 지원하는 hal 메서드 (중간에 stop sign 없음)
	HAL_I2C_Mem_Read(ds3231_hi2c, DS3231_I2C_ADR, DS3231_SEC, 1, buf, 7,
	DS3231_TIMEOUT_MS);

	dt->time.sec = bcd2bin(buf[0] & 0x7f);
	dt->time.min = bcd2bin(buf[1] & 0x7f);
	dt->time.hour = bcd2bin(buf[2] & 0x7f);
	dt->date.day = bcd2bin(buf[3] & 0x0f);
	dt->date.date = bcd2bin(buf[4] & 0x3f);
	dt->date.month = bcd2bin(buf[5] & 0x1f);
	dt->date.year = bcd2bin(buf[6]);
}

void ds3231_setDateTime(const ds3231_datetime_t *dt) {
	uint8_t buf[8];
	buf[0] = DS3231_SEC;
	buf[1] = bin2bcd(dt->time.sec);
	buf[2] = bin2bcd(dt->time.min);
	buf[3] = bin2bcd(dt->time.hour);
	buf[4] = bin2bcd(dt->date.day);
	buf[5] = bin2bcd(dt->date.date);
	buf[6] = bin2bcd(dt->date.month);
	buf[7] = bin2bcd(dt->date.year);

	HAL_I2C_Master_Transmit(ds3231_hi2c, DS3231_I2C_ADR, buf, 8, DS3231_TIMEOUT_MS);
}

