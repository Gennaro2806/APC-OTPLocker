#include "servo.h"

extern TIM_HandleTypeDef htim1;

#define SERVO_OPEN_PULSE   1500
#define SERVO_CLOSE_PULSE  1000

void Servo_Open(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_OPEN_PULSE);
}

void Servo_Close(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVO_CLOSE_PULSE);
}
