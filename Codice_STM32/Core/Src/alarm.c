#include "alarm.h"

static TIM_HandleTypeDef *alarm_tim = NULL;

static bool alarm_active = false;
static bool buzzer_phase_on = false;
static bool leds_blink = false;

static uint32_t alarm_start_time = 0;
static uint32_t phase_start_time = 0;
static uint32_t alarm_duration = 0;

static uint8_t active_led_mask = 0;

static void Alarm_SetSelectedLeds(uint8_t mask, GPIO_PinState state)
{
    if (mask & ALARM_LED3)  HAL_GPIO_WritePin(GPIOE, LD3_Pin, state);
    if (mask & ALARM_LED4)  HAL_GPIO_WritePin(GPIOE, LD4_Pin, state);
    if (mask & ALARM_LED5)  HAL_GPIO_WritePin(GPIOE, LD5_Pin, state);
    if (mask & ALARM_LED6)  HAL_GPIO_WritePin(GPIOE, LD6_Pin, state);
    if (mask & ALARM_LED7)  HAL_GPIO_WritePin(GPIOE, LD7_Pin, state);
    if (mask & ALARM_LED8)  HAL_GPIO_WritePin(GPIOE, LD8_Pin, state);
    if (mask & ALARM_LED9)  HAL_GPIO_WritePin(GPIOE, LD9_Pin, state);
    if (mask & ALARM_LED10) HAL_GPIO_WritePin(GPIOE, LD10_Pin, state);
}

void Alarm_Init(TIM_HandleTypeDef *htim)
{
    alarm_tim = htim;
}

void Alarm_Start(uint32_t duration_ms, uint8_t led_mask, bool blink_leds)
{
    if (alarm_tim == NULL)
    {
        return;
    }

    alarm_active = true;
    buzzer_phase_on = true;
    leds_blink = blink_leds;

    alarm_duration = duration_ms;
    active_led_mask = led_mask;

    alarm_start_time = HAL_GetTick();
    phase_start_time = HAL_GetTick();

    HAL_TIM_PWM_Start(alarm_tim, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(alarm_tim, TIM_CHANNEL_1, 250);

    if (!leds_blink)
    {
        Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_SET);
    }
}

void Alarm_Stop(void)
{
    if (alarm_tim != NULL)
    {
        HAL_TIM_PWM_Stop(alarm_tim, TIM_CHANNEL_1);
    }

    Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_RESET);

    alarm_active = false;
    buzzer_phase_on = false;
    leds_blink = false;

    alarm_duration = 0;
    active_led_mask = 0;
}

bool Alarm_IsActive(void)
{
    return alarm_active;
}

void Alarm_Update(void)
{
    uint32_t now;

    if (!alarm_active)
    {
        return;
    }

    now = HAL_GetTick();

    if ((now - alarm_start_time) >= alarm_duration)
    {
        Alarm_Stop();
        return;
    }

    /* Buzzer: 2s ON, 1s OFF */
    if (buzzer_phase_on)
    {
        if ((now - phase_start_time) >= 2000)
        {
            buzzer_phase_on = false;
            phase_start_time = now;
            HAL_TIM_PWM_Stop(alarm_tim, TIM_CHANNEL_1);
        }
    }
    else
    {
        if ((now - phase_start_time) >= 1000)
        {
            buzzer_phase_on = true;
            phase_start_time = now;
            HAL_TIM_PWM_Start(alarm_tim, TIM_CHANNEL_1);
            __HAL_TIM_SET_COMPARE(alarm_tim, TIM_CHANNEL_1, 250);
        }
    }

    /* LED management */
    if (leds_blink)
    {
        if (((now / 150) % 2) == 0)
        {
            Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_SET);
        }
        else
        {
            Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_RESET);
        }
    }
    else
    {
        Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_SET);
    }
}
