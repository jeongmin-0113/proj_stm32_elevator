/*
 * delay.h
 *
 *  Created on: 2026. 10. 27.
 *      Author: rhoblack
 */

#ifndef SRC_COMMON_DELAY_H_
#define SRC_COMMON_DELAY_H_

#include "stm32f4xx_hal.h"


void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);
uint32_t delay_elapsed_us(uint32_t start_cycle);
uint32_t delay_get_cycle(void);


#endif /* SRC_COMMON_DELAY_H_ */
