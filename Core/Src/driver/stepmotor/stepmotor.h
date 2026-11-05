/*
 * stepmotor.h
 *
 *  Created on: 2026. 10. 23.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_
#define SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_

#include "stm32f4xx_hal.h"

#define STEP_COUNT	8

typedef struct {
	GPIO_TypeDef *port;
	uint16_t pin;
} stepMotor_pin_t;

typedef struct {
	stepMotor_pin_t *pin;
	uint8_t stepcounter;
} stepMotor_t;

typedef enum {
	STEPMOTOR_DIR_CW, STEPMOTOR_DIR_CCW
} stepMotor_dir;

enum stepMotor_info {
	SEC, MIN, HOUR
};



void StepMotor_stepPhase(stepMotor_t *motor_info, stepMotor_dir dir);
void StepMotor_release(stepMotor_t *motor_info);

#endif /* SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_ */
