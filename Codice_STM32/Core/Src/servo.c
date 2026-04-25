#include "servo.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

#define SERVO_OPEN_PULSE    1500
#define SERVO_CLOSE_PULSE   1000

#define SERVO_STEP_DELAY    15
#define SERVO_STEP_SIZE     5

void Servo_Open(void)
{
    GPIO_PinState ledState = GPIO_PIN_RESET;

    for (uint16_t pulse = SERVO_CLOSE_PULSE;
         pulse <= SERVO_OPEN_PULSE;
         pulse += SERVO_STEP_SIZE)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);

        ledState = (ledState == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, ledState);

        HAL_Delay(SERVO_STEP_DELAY);
    }

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_OPEN_PULSE);

    /* servo aperto -> LED blue acceso fisso */
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
}

void Servo_Close(void)
{
    GPIO_PinState ledState = GPIO_PIN_RESET;

    for (int16_t pulse = SERVO_OPEN_PULSE;
         pulse >= SERVO_CLOSE_PULSE;
         pulse -= SERVO_STEP_SIZE)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);

        ledState = (ledState == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, ledState);

        HAL_Delay(SERVO_STEP_DELAY);
    }

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_CLOSE_PULSE);

    /* servo chiuso -> LED blue spento */
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
}
