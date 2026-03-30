#include "keypad.h"

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static void Keypad_SetAllColumnsLow(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
}

void Keypad_Init(void)
{
    Keypad_SetAllColumnsLow();
}

char Keypad_GetKey(void)
{
	const uint16_t colPins[4] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_10};
	const uint16_t rowPins[4] = {GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14};

    for (int col = 0; col < 4; col++)
    {
        Keypad_SetAllColumnsLow();
        HAL_GPIO_WritePin(GPIOB, colPins[col], GPIO_PIN_SET);

        for (volatile int d = 0; d < 3000; d++) { }

        for (int row = 0; row < 4; row++)
        {
        	if (HAL_GPIO_ReadPin(GPIOB, rowPins[row]) == GPIO_PIN_SET)
            {
                HAL_Delay(20);

                if (HAL_GPIO_ReadPin(GPIOB, rowPins[row]) == GPIO_PIN_SET)
                {
                	while (HAL_GPIO_ReadPin(GPIOB, rowPins[row]) == GPIO_PIN_SET)
                	{
                	}

                    Keypad_SetAllColumnsLow();
                    return keymap[row][col];
                }
            }
        }
    }

    Keypad_SetAllColumnsLow();
    return '\0';
}
