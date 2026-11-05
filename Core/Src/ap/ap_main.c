/*
 * ap_main.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "ap_main.h"
#include "service/Elevator/elevator.h"
#include "service/sound/sound.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_8) {
		// floor 3
		elevator_setCurFloor(0, 3);
		sound_button();
	} else if (GPIO_Pin == GPIO_PIN_6) {
		// floor 2
		elevator_setCurFloor(0, 2);
		sound_button();
	} else if (GPIO_Pin == GPIO_PIN_5) {
		// floor 1
		elevator_setCurFloor(0, 1);
		sound_button();
	}
}

void ap_main(void) {
	elevator_init();

	while (1) {
		elevator_task();
	}
}
