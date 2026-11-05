/*
 * buzzer.h
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_BUZZER_BUZZER_H_
#define SRC_DRIVER_BUZZER_BUZZER_H_

#include "stm32f4xx_hal.h"

void buzzer_init(TIM_HandleTypeDef *htim, uint32_t ch);
void buzzer_off();
void buzzer_on(uint32_t freq_hz);

#endif /* SRC_DRIVER_BUZZER_BUZZER_H_ */
