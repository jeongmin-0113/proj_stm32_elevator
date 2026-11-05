/*
 * elevator.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "elevator.h"
#include "../../../driver/stepmotor/stepmotor.h"
#include "../../../driver/button/button.h"

#define BTN_1_GPIO	GPIOC
#define BTN_1_PIN	GPIO_PIN_0
#define BTN_2_GPIO	GPIOC
#define BTN_2_PIN	GPIO_PIN_1
#define BTN_3_GPIO	GPIOC
#define BTN_3_PIN	GPIO_PIN_3

button_t btn_floor_1, btn_floor_2, btn_floor_3;

stepMotor_pin_t stepMotor1Pins[4] = { { GPIOC, GPIO_PIN_10 }, // orange
		{ GPIOC, GPIO_PIN_11 }, // yellow
		{ GPIOC, GPIO_PIN_12 }, // pink
		{ GPIOD, GPIO_PIN_2 }  // blue
};

stepMotor_pin_t stepMotor2Pins[4] = { { GPIOC, GPIO_PIN_10 }, // orange
		{ GPIOC, GPIO_PIN_11 }, // yellow
		{ GPIOC, GPIO_PIN_12 }, // pink
		{ GPIOD, GPIO_PIN_2 }  // blue
};

stepMotor_t motor1 = { stepMotor1Pins, 0 };
stepMotor_t motor2 = { stepMotor2Pins, 0 };

typedef struct {
	stepMotor_t *motor;
	uint8_t curFloor;
	uint8_t targetFloor;
	uint8_t stopFlag;
} elevator_t;

elevator_t elevators[2];

void elevator_init() {
	button_init(&btn_floor_1, BTN_1_GPIO, BTN_1_PIN);
	button_init(&btn_floor_2, BTN_2_GPIO, BTN_2_PIN);
	button_init(&btn_floor_3, BTN_3_GPIO, BTN_3_PIN);

	elevators[0].motor = &motor1;
	elevators[0].curFloor = 2;
	elevators[0].targetFloor = 1;
	elevators[0].stopFlag = 1;

	elevators[1].motor = &motor2;
	elevators[1].curFloor = 1;
	elevators[1].targetFloor = 1;
	elevators[1].stopFlag = 1;
}

void elevator_setCurFloor(uint8_t idx, uint8_t floor) {
	elevators[idx].curFloor = floor;
}

void elevator_up(uint8_t idx) {
	static uint32_t prevTime = 0;
	if (HAL_GetTick() - prevTime >= 2) {
		StepMotor_stepPhase(elevators[idx].motor, STEPMOTOR_DIR_CW);
		prevTime = HAL_GetTick();
	}
}

void elevator_down(uint8_t idx) {
	static uint32_t prevTime = 0;
	if (HAL_GetTick() - prevTime >= 2) {
		StepMotor_stepPhase(elevators[idx].motor, STEPMOTOR_DIR_CCW);
		prevTime = HAL_GetTick();
	}
}

void elevator_select() {

}

void elevator_buttonQueue() {
	if (button_getState(&btn_floor_1) == ACT_PUSH) {
		elevators[0].targetFloor = 1;
	}
	if (button_getState(&btn_floor_2) == ACT_PUSH) {
		elevators[0].targetFloor = 2;
	}
	if (button_getState(&btn_floor_3) == ACT_PUSH) {
		elevators[0].targetFloor = 3;
	}
}

void elevator_task() {
	elevator_buttonQueue();

	for (int i = 0; i < 2; i++) {

		if (elevators[i].curFloor < elevators[i].targetFloor) {
			elevator_up(i);
		} else if (elevators[i].curFloor > elevators[i].targetFloor) {
			elevator_down(i);
		} else {

		}
	}
}
