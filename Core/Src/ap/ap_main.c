/*
 * ap_main.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "ap_main.h"
#include "service/Elevator/elevator.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_9) {
		// floor 3 - elevator1
		elevator_setCurFloor(0, 3);
	} else if (GPIO_Pin == GPIO_PIN_6) {
		// floor 2 - elevator1
		elevator_setCurFloor(0, 2);
	} else if (GPIO_Pin == GPIO_PIN_5) {
		// floor 1 - elevator1
		elevator_setCurFloor(0, 1);
	} else if (GPIO_Pin == GPIO_PIN_4) {
		// floor 3 - elevator2
		elevator_setCurFloor(1, 3);
	} else if (GPIO_Pin == GPIO_PIN_10) {
		// floor 2 - elevator2
		elevator_setCurFloor(1, 2);
	} else if (GPIO_Pin == GPIO_PIN_8) {
		// floor 1 - elevator2
		elevator_setCurFloor(1, 1);
	}
}

void ap_main(void) {
	elevator_init();

	while (1) {
		elevator_task();
	}
}
