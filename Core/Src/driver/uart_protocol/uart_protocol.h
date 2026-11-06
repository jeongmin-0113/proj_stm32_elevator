/*
 * uart_protocol.h
 *
 *  Created on: 2026. 11. 4.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_UART_PROTOCOL_UART_PROTOCOL_H_
#define SRC_DRIVER_UART_PROTOCOL_UART_PROTOCOL_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define PKT_START	0xAA
#define PKT_MAX_LEN 16

// request command (pc -> mcu)
#define REQ_FLOOR_1			0x10
#define REQ_FLOOR_2			0x11
#define REQ_FLOOR_3			0x12
#define REQ_ELEVATOR_STATE	0x20

// response command (mcu -> pc)
#define CMD_RSP_BASE		0x80

#define RSP_OK			0x00
#define RSP_ERROR		0x01

// 함수 포인터 자료형 - 함수의 주소를 넣을 수 있음
typedef void (*uart_packet_callback_t)(uint8_t cmd, uint8_t *payload, uint8_t payload_len);

void uart_protocol_init(UART_HandleTypeDef* huart, uart_packet_callback_t packet_cb);
void uart_protocol_it();
void UART_protocol_send(uint8_t cmd, uint8_t *payload, uint8_t payload_len);
void uart_protocol_task();

#endif /* SRC_DRIVER_UART_PROTOCOL_UART_PROTOCOL_H_ */
