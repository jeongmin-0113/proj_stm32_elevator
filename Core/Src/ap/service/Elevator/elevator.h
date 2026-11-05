/*
 * elevator.h
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */

#ifndef SRC_AP_ELEVATOR_ELEVATOR_H_
#define SRC_AP_ELEVATOR_ELEVATOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "../../../driver/stepmotor/stepmotor.h"

typedef enum { DIR_IDLE, DIR_UP, DIR_DOWN } dir_t;

typedef struct {
    stepMotor_t *motor;
    volatile uint8_t curFloor;    // 마지막으로 통과한 센서 층
    volatile uint8_t arrivedEvt;  // ISR에서 도착시 한번만 1 / 도착 x면 0
    uint8_t  reqMask;             // bit1=1F, bit2=2F, bit3=3F
    dir_t    dir;
    uint32_t lastStepTick;
} elevator_t;

void elevator_init();
void elevator_task();
void elevator_setCurFloor(uint8_t idx, uint8_t floor);

#endif /* SRC_AP_ELEVATOR_ELEVATOR_H_ */
