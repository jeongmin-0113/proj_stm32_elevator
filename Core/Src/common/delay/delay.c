/*
 * delay.c
 *
 *  Created on: 2026. 10. 27.
 *      Author: rhoblack
 */

#include "delay.h"
#include <stdbool.h>

static uint32_t s_cycles_per_us = 1;
static bool s_ready = false;

//void dwt_init(void)
//{
//	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
//	DWT->CYCCNT = 0;
//	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
//}
//
//uint32_t micros(void)
//{
//	return DWT->CYCCNT / (SystemCoreClock / 1000000);
//}

void delay_init(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	s_cycles_per_us = SystemCoreClock / 1000000; // 100MHz / 1MHz => 100
	if (s_cycles_per_us == 0) s_cycles_per_us = 1;
	s_ready = true;
}

void delay_us(uint32_t us)
{
	if (!s_ready) delay_init();

	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * s_cycles_per_us;

	while((DWT->CYCCNT - start) < ticks) ;
}

void delay_ms(uint32_t ms)
{
	while (ms-- > 0) {
		delay_us(1000);
	}
}

uint32_t delay_elapsed_us(uint32_t start_cycle)
{
	return (DWT->CYCCNT - start_cycle) / s_cycles_per_us;
}

uint32_t delay_get_cycle(void)
{
	if (!s_ready) delay_init();
	return DWT->CYCCNT;
}
