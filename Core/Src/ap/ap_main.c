/*
 * ap_main.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "ap_main.h"
#include "service/Elevator/elevator.h"


void ap_main(void) {
	elevator_init();

	while (1) {
		elevator_task();
	}
}
