/*
 * led.c
 *
 *  Created on: 2026. 10. 16.
 *      Author: rhoblack
 */

#include "led.h"

void led_init(led_t *led, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	led->GPIOx = GPIOx;
	led->GPIO_Pin = GPIO_Pin;
}

void led_on(const led_t *led)
{
	HAL_GPIO_WritePin(led->GPIOx, led->GPIO_Pin, GPIO_PIN_SET);
}

void led_off(const led_t *led)
{
	HAL_GPIO_WritePin(led->GPIOx, led->GPIO_Pin, GPIO_PIN_RESET);
}

void led_toggle(const led_t *led)
{
	HAL_GPIO_TogglePin(led->GPIOx, led->GPIO_Pin);
}
