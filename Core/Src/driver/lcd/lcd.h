/*
 * lcd.h
 *
 *  Created on: 2026. 10. 21.
 *      Author: rhoblack
 */

#ifndef SRC_DRIVER_LCD_LCD_H_
#define SRC_DRIVER_LCD_LCD_H_

#include "stm32f4xx_hal.h"

void Lcd_backlightOn(void);
void Lcd_backlightOff(void);
void Lcd_writeCmdData(uint8_t data);
void Lcd_writeCharData(uint8_t data);
void Lcd_init(I2C_HandleTypeDef* hi2c);
void Lcd_writeString(char *str);
void Lcd_gotoXY(uint8_t row, uint8_t col);
void Lcd_writeStringXY(uint8_t row, uint8_t col, char *str);


#endif /* SRC_DRIVER_LCD_LCD_H_ */
