/*
 * buzzer.c
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */
#include "buzzer.h"

static TIM_HandleTypeDef *buz_htim;
static uint32_t buz_ch;

void buzzer_init(TIM_HandleTypeDef *htim, uint32_t ch) {
	buz_htim = htim;
	buz_ch = ch;

	// ccr = 0 -> 1이 나오지 않게 한다 (전부 0) -> pwm 출력으로 계속 0이 나온다 (초기값)
	__HAL_TIM_SET_COMPARE(buz_htim, buz_ch, 0);
	HAL_TIM_PWM_Start(buz_htim, buz_ch);
}

void buzzer_off() {
	__HAL_TIM_SET_COMPARE(buz_htim, buz_ch, 0);
}

void buzzer_on(uint32_t freq_hz) {
	if (freq_hz == 0) {
		buzzer_off();
		return;
	}

	// 가청 주파수: 20hz ~ 20khz
	if (freq_hz < 20)
		freq_hz = 20;
	if (freq_hz > 20000)
		freq_hz = 20000;

	uint32_t arr = 1000000 / freq_hz - 1; // 1khz
	uint32_t ccr = (arr + 1) / 2; // 50% duty ratio
	__HAL_TIM_SET_AUTORELOAD(buz_htim, arr);
	__HAL_TIM_SET_COMPARE(buz_htim, buz_ch, ccr);

}
