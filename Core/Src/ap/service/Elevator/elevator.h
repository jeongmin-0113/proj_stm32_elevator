/*
 * elevator.h
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */

#ifndef SRC_AP_ELEVATOR_ELEVATOR_H_
#define SRC_AP_ELEVATOR_ELEVATOR_H_

#include <stdint.h>

void elevator_init();
void elevator_task();
void elevator_setCurFloor(uint8_t idx, uint8_t floor);

#endif /* SRC_AP_ELEVATOR_ELEVATOR_H_ */
