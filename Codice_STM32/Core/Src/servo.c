#include "servo.h"

extern TIM_HandleTypeDef htim1;

#define SERVO_OPEN_PULSE    1500
#define SERVO_CLOSE_PULSE   1000

#define SERVO_STEP_DELAY    15     // ms tra uno step e l'altro
#define SERVO_STEP_SIZE     5      // grandezza dello step

void Servo_Open(void)
{
    for (uint16_t pulse = SERVO_CLOSE_PULSE;
         pulse <= SERVO_OPEN_PULSE;
         pulse += SERVO_STEP_SIZE)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
        HAL_Delay(SERVO_STEP_DELAY);
    }

    /* sicurezza finale */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_OPEN_PULSE);
}

void Servo_Close(void)
{
    for (int16_t pulse = SERVO_OPEN_PULSE;
         pulse >= SERVO_CLOSE_PULSE;
         pulse -= SERVO_STEP_SIZE)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
        HAL_Delay(SERVO_STEP_DELAY);
    }

    /* sicurezza finale */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_CLOSE_PULSE);
}
