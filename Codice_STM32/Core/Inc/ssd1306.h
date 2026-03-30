#ifndef SSD1306_H
#define SSD1306_H

#include "main.h"
#include <stdint.h>

#define SSD1306_I2C_ADDR 0x78

void SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_Clear(void);
void SSD1306_UpdateScreen(void);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_WriteChar(char ch);
void SSD1306_WriteString(char *str);

#endif
