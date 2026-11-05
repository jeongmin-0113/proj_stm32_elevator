/*
 * sound.h
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */

#ifndef SRC_AP_SERVICE_SOUND_SOUND_H_
#define SRC_AP_SERVICE_SOUND_SOUND_H_

#include "../../../driver/buzzer/buzzer.h"

void sound_init();
void sound_boot();
void sound_button();
void sound_error();
void sound_alarm();

#endif /* SRC_AP_SERVICE_SOUND_SOUND_H_ */
