/*
 * ap_main.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */
#include "ap_main.h"
#include "service/Elevator/elevator.h"
#include "../driver/uart_protocol/uart_protocol.h"

extern UART_HandleTypeDef huart2;

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
	} else if (GPIO_Pin == GPIO_PIN_8) {
		// floor 3 - elevator2
		elevator_setCurFloor(1, 3);
	} else if (GPIO_Pin == GPIO_PIN_10) {
		// floor 2 - elevator2
		elevator_setCurFloor(1, 2);
	} else if (GPIO_Pin == GPIO_PIN_4) {
		// floor 1 - elevator2
		elevator_setCurFloor(1, 1);
	}
}

void packet_doFunc(uint8_t cmd, uint8_t *payload, uint8_t payload_len) {
	switch (cmd) {
	case REQ_FLOOR_1: {
		uint8_t tx_payload = 0;

		elevator_select(1);
		tx_payload = RSP_OK;

		UART_protocol_send(cmd + CMD_RSP_BASE, &tx_payload, 1);

		break;
	}
	case REQ_FLOOR_2: {
		uint8_t tx_payload = 0;

		elevator_select(2);
		tx_payload = RSP_OK;

		UART_protocol_send(cmd + CMD_RSP_BASE, &tx_payload, 1);

		break;
	}
	case REQ_FLOOR_3: {
		uint8_t tx_payload = 0;

		elevator_select(3);
		tx_payload = RSP_OK;

		UART_protocol_send(cmd + CMD_RSP_BASE, &tx_payload, 1);

		break;
	}
	case REQ_ELEVATOR_STATE: {
		uint8_t tx_payload[6] = {0,};
		elevator_getState(tx_payload);

		UART_protocol_send(cmd + CMD_RSP_BASE, tx_payload, 6);
		break;
	}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {
		// interrupt service
		uart_protocol_it(); // 1 byte마다 인터럽트 걸리게 해서 데이터 받겠다~
	}
}

void ap_main(void) {
	elevator_init();
	uart_protocol_init(&huart2, packet_doFunc);

	while (1) {
		uart_protocol_task();
		elevator_task();
	}
}
