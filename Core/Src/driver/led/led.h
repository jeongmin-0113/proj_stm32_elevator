/*
 * led.h
 *
 *  Created on: 2026. 10. 16.
 *      Author: rhoblack
 */

#ifndef SRC_DRIVER_LED_LED_H_
#define SRC_DRIVER_LED_LED_H_

#include "stm32f4xx_hal.h"

typedef struct {
	GPIO_TypeDef* GPIOx;
	uint16_t GPIO_Pin;
} led_t;

void led_init(led_t *led, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void led_on(const led_t *led);
void led_off(const led_t *led);
void led_toggle(const led_t *led);


#endif /* SRC_DRIVER_LED_LED_H_ */
