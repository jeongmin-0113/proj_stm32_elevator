/*
 * button.h
 *
 *  Created on: 2026. 10. 16.
 *      Author: rhoblack
 */

#ifndef SRC_DRIVER_BUTTON_BUTTON_H_
#define SRC_DRIVER_BUTTON_BUTTON_H_

#include "stm32f4xx_hal.h"

typedef enum {
	NO_ACT,
	ACT_PUSH,
	ACT_RELEASE,
}button_state_t;

typedef struct {
	GPIO_TypeDef* GPIOx;
	uint16_t GPIO_Pin;
	int prevState;
}button_t;

enum {PUSHED=0, RELEASED};

void button_init(button_t *btn, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
button_state_t button_getState(button_t *btn);



#endif /* SRC_DRIVER_BUTTON_BUTTON_H_ */
