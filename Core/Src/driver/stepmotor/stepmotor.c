/*
 * stepmotor.c
 *
 *  Created on: 2026. 10. 23.
 *      Author: kccistc
 */
#include "stepmotor.h"

uint8_t motorSteps[STEP_COUNT] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0c, 0x08,
		0x09 };

void StepMotor_writePattern(stepMotor_pin_t *pin_info, uint8_t step) {
	for (int i = 0; i < 4; i++) {
		HAL_GPIO_WritePin(pin_info[i].port, pin_info[i].pin,
				((step & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET));
	}
}

void StepMotor_stepPhase(stepMotor_t *motor_info, stepMotor_dir dir) {
	StepMotor_writePattern(motor_info->pin,
			motorSteps[motor_info->stepcounter]);

	if (dir == STEPMOTOR_DIR_CCW) {
		if (motor_info->stepcounter == 7) {
			motor_info->stepcounter = 0;
		} else {
			motor_info->stepcounter++;
		}
	} else {
		if (motor_info->stepcounter == 0) {
			motor_info->stepcounter = 7;
		} else {
			motor_info->stepcounter--;
		}
	}
}

void StepMotor_release(stepMotor_t *motor_info) {
	StepMotor_writePattern(motor_info->pin, 0x00);
}
