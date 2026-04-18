#include "ssd1306.h"
#include "fonts.h"
#include <string.h>

static I2C_HandleTypeDef *ssd1306_i2c;
static uint8_t SSD1306_Buffer[128 * 64 / 8];
static uint8_t CurrentX = 0;
static uint8_t CurrentY = 0;

static void SSD1306_WriteCommand(uint8_t byte)
{
    uint8_t data[2];
    data[0] = 0x00;
    data[1] = byte;
    HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR, data, 2, HAL_MAX_DELAY);
}

static void SSD1306_WriteData(uint8_t *buffer, size_t size)
{
    uint8_t data[129];
    data[0] = 0x40;
    memcpy(&data[1], buffer, size);
    HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR, data, size + 1, HAL_MAX_DELAY);
}

void SSD1306_Init(I2C_HandleTypeDef *hi2c)
{
    ssd1306_i2c = hi2c;
    HAL_Delay(100);

    SSD1306_WriteCommand(0xAE);
    SSD1306_WriteCommand(0x20);
    SSD1306_WriteCommand(0x10);
    SSD1306_WriteCommand(0xB0);
    SSD1306_WriteCommand(0xC8);
    SSD1306_WriteCommand(0x00);
    SSD1306_WriteCommand(0x10);
    SSD1306_WriteCommand(0x40);
    SSD1306_WriteCommand(0x81);
    SSD1306_WriteCommand(0xFF);
    SSD1306_WriteCommand(0xA1);
    SSD1306_WriteCommand(0xA6);
    SSD1306_WriteCommand(0xA8);
    SSD1306_WriteCommand(0x3F);
    SSD1306_WriteCommand(0xA4);
    SSD1306_WriteCommand(0xD3);
    SSD1306_WriteCommand(0x00);
    SSD1306_WriteCommand(0xD5);
    SSD1306_WriteCommand(0xF0);
    SSD1306_WriteCommand(0xD9);
    SSD1306_WriteCommand(0x22);
    SSD1306_WriteCommand(0xDA);
    SSD1306_WriteCommand(0x12);
    SSD1306_WriteCommand(0xDB);
    SSD1306_WriteCommand(0x20);
    SSD1306_WriteCommand(0x8D);
    SSD1306_WriteCommand(0x14);
    SSD1306_WriteCommand(0xAF);

    SSD1306_Clear();
    SSD1306_UpdateScreen();
}

void SSD1306_Clear(void)
{
    memset(SSD1306_Buffer, 0, sizeof(SSD1306_Buffer));
    CurrentX = 0;
    CurrentY = 0;
}

void SSD1306_UpdateScreen(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        SSD1306_WriteCommand(0xB0 + i);
        SSD1306_WriteCommand(0x00);
        SSD1306_WriteCommand(0x10);
        SSD1306_WriteData(&SSD1306_Buffer[128 * i], 128);
    }
}

void SSD1306_SetCursor(uint8_t x, uint8_t y)
{
    CurrentX = x;
    CurrentY = y;
}

void SSD1306_WriteChar(char ch)
{
    if (ch < 32 || ch > 90)
        ch = 32;

    if (CurrentX > 122 || CurrentY > 7)
        return;

    for (uint8_t i = 0; i < 5; i++)
    {
        SSD1306_Buffer[CurrentX + (CurrentY * 128) + i] = Font5x7[(uint8_t)ch][i];
    }

    SSD1306_Buffer[CurrentX + (CurrentY * 128) + 5] = 0x00;
    CurrentX += 6;
}

void SSD1306_WriteString(char *str)
{
    while (*str)
    {
        SSD1306_WriteChar(*str);
        str++;
    }
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64)
        return;

    if (color)
        SSD1306_Buffer[x + (y / 8) * 128] |= (1 << (y % 8));
    else
        SSD1306_Buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
}

void SSD1306_DrawHLine(uint8_t x, uint8_t y, uint8_t w, uint8_t color)
{
    for (uint8_t i = 0; i < w; i++)
    {
        SSD1306_DrawPixel(x + i, y, color);
    }
}

void SSD1306_DrawVLine(uint8_t x, uint8_t y, uint8_t h, uint8_t color)
{
    for (uint8_t i = 0; i < h; i++)
    {
        SSD1306_DrawPixel(x, y + i, color);
    }
}

void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (w == 0 || h == 0)
        return;

    SSD1306_DrawHLine(x, y, w, color);
    SSD1306_DrawHLine(x, y + h - 1, w, color);
    SSD1306_DrawVLine(x, y, h, color);
    SSD1306_DrawVLine(x + w - 1, y, h, color);
}

void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t i = 0; i < h; i++)
    {
        SSD1306_DrawHLine(x, y + i, w, color);
    }
}
