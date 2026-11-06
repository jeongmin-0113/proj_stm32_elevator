/*
 * uart_protocol.c
 *
 *  Created on: 2026. 11. 4.
 *      Author: kccistc
 */
#include "uart_protocol.h"
#include "../led/led.h"

typedef enum {
	RX_WAIT_START, RX_WAIT_LEN, RX_WAIT_DATA, RX_WAIT_CHK
} uart_protocol_state_t;

UART_HandleTypeDef *protocol_huart;

static uart_protocol_state_t rx_state = RX_WAIT_START;
static volatile uint8_t rx_byte;
static uint8_t rx_len = 0;
static uint8_t rx_idx = 0;
uint8_t rx_buf[PKT_MAX_LEN] = { 0, };

static volatile uint8_t packet_ready = 0;

static uart_packet_callback_t packet_callback = 0;

void UART_protocol_send(uint8_t cmd, uint8_t *payload, uint8_t payload_len) {
	uint8_t cksum = payload_len + 1;

	uint8_t tx_buf[4 + PKT_MAX_LEN];
	tx_buf[0] = PKT_START;
	tx_buf[1] = payload_len + 1;
	tx_buf[2] = cmd;
	cksum += cmd;

	for (int i = 0; i < payload_len; i++) {
		cksum += payload[i];
		tx_buf[3 + i] = payload[i];
	}

	tx_buf[3 + payload_len] = cksum;

	HAL_UART_Transmit(protocol_huart, tx_buf, 4 + payload_len, 1000);
}

// uart protocol format parser
void rx_parseByte(uint8_t rx_byte) {
	switch (rx_state) {
	case RX_WAIT_START:
		if (rx_byte == PKT_START && !packet_ready) {
			rx_len = 0;
			rx_idx = 0;
			rx_state = RX_WAIT_LEN;
		}
		break;
	case RX_WAIT_LEN:
		if (rx_byte == 0 || rx_byte >= PKT_MAX_LEN) {
			rx_state = RX_WAIT_START;
			return;
		}
		rx_len = rx_byte;
		rx_state = RX_WAIT_DATA;
		break;
	case RX_WAIT_DATA:
		rx_buf[rx_idx++] = rx_byte;
		if (rx_idx >= rx_len) {
			rx_state = RX_WAIT_CHK;
		}
		break;
	case RX_WAIT_CHK: {
		uint8_t data_check = rx_len;
		for (int i = 0; i < rx_len; i++)
			data_check += rx_buf[i];
		if (data_check == rx_byte) {
			// checksum ok - 명령 실행 + ok 전송
			packet_ready = 1;

		} else {
			// checksum error - error 전송
			uint8_t tx_payload = RSP_ERROR;
			UART_protocol_send(rx_buf[0] + CMD_RSP_BASE, &tx_payload, 1);
		}
		rx_state = RX_WAIT_START;
		break;
	}
	default:
		rx_state = RX_WAIT_START;
		break;
	}
}

void uart_protocol_task() {
	// checksum ok - 명령 실행 + ok 전송
	if (packet_ready) {
		packet_callback(rx_buf[0], &rx_buf[1], rx_len - 1);
		packet_ready = 0;
	}
}

void uart_protocol_it() {
	rx_parseByte(rx_byte);
	HAL_UART_Receive_IT(protocol_huart, (uint8_t*) &rx_byte, 1);
}

void uart_protocol_init(UART_HandleTypeDef *huart,
		uart_packet_callback_t packet_cb) {

	protocol_huart = huart;
	packet_callback = packet_cb;
	rx_state = RX_WAIT_START;
	packet_ready = 0;

	HAL_UART_Receive_IT(protocol_huart, (uint8_t*) &rx_byte, 1);
}
