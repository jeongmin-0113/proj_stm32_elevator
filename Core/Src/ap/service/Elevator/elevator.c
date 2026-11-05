/*
 * elevator.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "elevator.h"
#include "../../../driver/button/button.h"
#include "../sound/sound.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>
#include <string.h>

#define BTN_1_GPIO	GPIOC
#define BTN_1_PIN	GPIO_PIN_0
#define BTN_2_GPIO	GPIOC
#define BTN_2_PIN	GPIO_PIN_1
#define BTN_3_GPIO	GPIOC
#define BTN_3_PIN	GPIO_PIN_3

extern I2C_HandleTypeDef hi2c1;

button_t btn_floor_1, btn_floor_2, btn_floor_3;

stepMotor_pin_t stepMotor1Pins[4] = { { GPIOC, GPIO_PIN_10 }, // orange
		{ GPIOC, GPIO_PIN_11 }, // yellow
		{ GPIOC, GPIO_PIN_12 }, // pink
		{ GPIOD, GPIO_PIN_2 }  // blue
};

stepMotor_pin_t stepMotor2Pins[4] = { { GPIOC, GPIO_PIN_7 }, // orange
		{ GPIOB, GPIO_PIN_6 }, // yellow
		{ GPIOA, GPIO_PIN_7 }, // pink
		{ GPIOA, GPIO_PIN_6 }  // blue
};

stepMotor_t motor1 = { stepMotor1Pins, 0 };
stepMotor_t motor2 = { stepMotor2Pins, 0 };

elevator_t elevators[2];
char prevBuf[30];

void elevator_init() {
	button_init(&btn_floor_1, BTN_1_GPIO, BTN_1_PIN);
	button_init(&btn_floor_2, BTN_2_GPIO, BTN_2_PIN);
	button_init(&btn_floor_3, BTN_3_GPIO, BTN_3_PIN);

	Lcd_init(&hi2c1);

	sound_init();

	elevators[0].motor = &motor1;
	elevators[0].curFloor = 1;
	elevators[0].reqMask = 0;
	elevators[0].dir = DIR_IDLE;
	elevators[0].lastStepTick = 0;

	elevators[1].motor = &motor2;
	elevators[1].curFloor = 1;
	elevators[1].reqMask = 0;
	elevators[1].dir = DIR_IDLE;
	elevators[1].lastStepTick = 0;

	char buf[30];
	sprintf(buf, "    elevator    ");
	Lcd_writeStringXY(0, 0, buf);

	sprintf(prevBuf, "STOP 1F  STOP 1F");
	Lcd_writeStringXY(1, 0, prevBuf);
}

void elevator_setCurFloor(uint8_t idx, uint8_t floor) {
	elevators[idx].curFloor = floor;
	if ((elevators[idx].reqMask) & (1 << floor)) {
		elevators[idx].arrivedEvt = 1;
	}
}

void elevator_up(uint8_t idx) {
	if (HAL_GetTick() - elevators[idx].lastStepTick >= 2) {
		StepMotor_stepPhase(elevators[idx].motor, STEPMOTOR_DIR_CW);
		elevators[idx].lastStepTick = HAL_GetTick();
	}
}

void elevator_down(uint8_t idx) {
	if (HAL_GetTick() - elevators[idx].lastStepTick >= 2) {
		StepMotor_stepPhase(elevators[idx].motor, STEPMOTOR_DIR_CCW);
		elevators[idx].lastStepTick = HAL_GetTick();
	}
}

uint8_t elevator_calcCost(elevator_t *e, uint8_t floor) {
	uint8_t dist =
			(e->curFloor > floor) ? e->curFloor - floor : floor - e->curFloor;

	if (e->dir == DIR_IDLE)
		return dist;
	if (e->dir == DIR_UP && floor > e->curFloor)
		return dist;
	if (e->dir == DIR_DOWN && floor < e->curFloor)
		return dist;
	return dist + 3;
}

void elevator_select(uint8_t floor) {
	uint8_t bitMask = 1 << floor;
	uint8_t cost[2] = { 0, 0 };

	for (int i = 0; i < 2; i++) {
		if (((elevators[i].reqMask) & bitMask)
				|| (elevators[i].dir == DIR_IDLE
						&& elevators[i].curFloor == floor))
			return;
		cost[i] = elevator_calcCost(&elevators[i], floor);
	}

	if (cost[0] > cost[1]) {
		elevators[1].reqMask |= bitMask;
	} else {
		elevators[0].reqMask |= bitMask;
	}
}

void elevator_buttonTask() {
	if (button_getState(&btn_floor_1) == ACT_PUSH) {
//		elevators[0].targetFloor = 1;
		elevator_select(1);
	}
	if (button_getState(&btn_floor_2) == ACT_PUSH) {
//		elevators[0].targetFloor = 2;
		elevator_select(2);
	}
	if (button_getState(&btn_floor_3) == ACT_PUSH) {
//		elevators[0].targetFloor = 3;
		elevator_select(3);
	}
}

void elevator_display() {
	char buf[30];

	char *dir_str[] = { "STOP ", "RUN ^", "RUN v" };

	sprintf(buf, "%s%1dF  %s%1dF", dir_str[elevators[0].dir],
			elevators[0].curFloor, dir_str[elevators[1].dir],
			elevators[1].curFloor);

	if (strcmp(prevBuf, buf)) {
		strcpy(prevBuf, buf);
		Lcd_writeStringXY(1, 0, prevBuf);
	}
}

void elevator_task() {
	elevator_buttonTask();

	for (int i = 0; i < 2; i++) {

		if (elevators[i].arrivedEvt) {
			elevators[i].arrivedEvt = 0;
			elevators[i].reqMask &= ~(1 << elevators[i].curFloor);
			sound_button();
		}

		// 방향 결정
		uint8_t above = elevators[i].reqMask >> (elevators[i].curFloor + 1);
		uint8_t below = elevators[i].reqMask
				& ((1 << elevators[i].curFloor) - 1);

		// up 중인데 위에 없음 -> 정지
		if (elevators[i].dir == DIR_UP && !above)
			elevators[i].dir = DIR_IDLE;
		// down 중인데 아래에 없음 -> 정지
		if (elevators[i].dir == DIR_DOWN && !below)
			elevators[i].dir = DIR_IDLE;
		// 정지 상태에서 위에 있음 -> 위로 (우선순위 존재) / 아래에 있음 -> 아래로
		if (elevators[i].dir == DIR_IDLE) {
			if (above)
				elevators[i].dir = DIR_UP;
			else if (below)
				elevators[i].dir = DIR_DOWN;
		}

		// 이동
		if (elevators[i].dir != DIR_IDLE
				&& HAL_GetTick() - elevators[i].lastStepTick >= 2) {
			StepMotor_stepPhase(elevators[i].motor,
					elevators[i].dir == DIR_UP ?
							STEPMOTOR_DIR_CW : STEPMOTOR_DIR_CCW);
			elevators[i].lastStepTick = HAL_GetTick();
		}

	}

	elevator_display();
}
