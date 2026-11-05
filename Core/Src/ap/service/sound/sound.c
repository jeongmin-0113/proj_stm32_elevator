/*
 * sound.c
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */
#include "sound.h"
#include "../../../common/delay/delay.h"

#define C4   262
#define CS4  277
#define D4   294
#define DS4  311  // Eb4 (낮은음)
#define E4   330
#define F4   349
#define FS4  370
#define G4   392
#define GS4  415  // Ab4
#define A4   440
#define AS4  466  // Bb4
#define B4   494

extern TIM_HandleTypeDef htim1;

void sound_init() {
	buzzer_init(&htim1, TIM_CHANNEL_4);
}

void sound_boot() {
	buzzer_on(G4);
	HAL_Delay(150);
	buzzer_on(D4);
	HAL_Delay(150);
	buzzer_on(G4);
	HAL_Delay(150);
	buzzer_on(C4 * 2);
	HAL_Delay(300);
	buzzer_on(B4);
	HAL_Delay(400);
	buzzer_off();
}

void sound_button() {
	buzzer_on(G4 * 2 * 2 * 2);
//	HAL_Delay(40);
	delay_ms(40);
	buzzer_on(C4 * 2 * 2);
//	HAL_Delay(60);
	delay_ms(60);
	buzzer_off();
}

void sound_error() {
	buzzer_on(C4 * 2);
	HAL_Delay(150);
	buzzer_off();
	HAL_Delay(60);
	buzzer_on(C4 * 2);
	HAL_Delay(150);
	buzzer_off();
}

void sound_alarm() {
	buzzer_on(C4 * 2 * 2 * 2);
	HAL_Delay(100);
	buzzer_off();
	HAL_Delay(100);
	buzzer_on(C4 * 2 * 2 * 2);
	HAL_Delay(100);
	buzzer_off();
	HAL_Delay(100);
	buzzer_on(C4 * 2 * 2 * 2);
	HAL_Delay(100);
	buzzer_off();
	HAL_Delay(100);
}

