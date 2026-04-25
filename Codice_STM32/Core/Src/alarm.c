#include "alarm.h"

static TIM_HandleTypeDef *alarm_tim = NULL;

/* Allarme con buzzer */
static bool alarm_active = false;
static bool buzzer_phase_on = false;
static bool buzzer_leds_blink = false;

static uint32_t alarm_start_time = 0;
static uint32_t phase_start_time = 0;
static uint32_t alarm_duration = 0;

/* Gestione LED indipendente */
static bool led_active = false;
static bool led_blink = false;
static uint32_t led_start_time = 0;
static uint32_t led_duration = 0;

/* maschera LED attiva globale */
static uint8_t active_led_mask = 0;

/*
 * Mappatura LED esterni:
 * ALARM_LED3 -> LED_RED
 * ALARM_LED4 -> LED_BLUE
 * ALARM_LED5 -> LED_GREEN
 */
static void Alarm_SetSelectedLeds(uint8_t mask, GPIO_PinState state)
{
    if (mask & ALARM_LED3)
    {
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, state);
    }

    if (mask & ALARM_LED4)
    {
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, state);
    }

    if (mask & ALARM_LED5)
    {
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, state);
    }
}

void Alarm_Init(TIM_HandleTypeDef *htim)
{
    alarm_tim = htim;
    Alarm_SetSelectedLeds(ALARM_LED3 | ALARM_LED4 | ALARM_LED5, GPIO_PIN_RESET);
}

void Alarm_Start(uint32_t duration_ms, uint8_t led_mask, bool blink_leds)
{
    if (alarm_tim == NULL)
    {
        return;
    }

    alarm_active = true;
    buzzer_phase_on = true;
    buzzer_leds_blink = blink_leds;

    alarm_duration = duration_ms;
    active_led_mask = led_mask;

    alarm_start_time = HAL_GetTick();
    phase_start_time = HAL_GetTick();

    HAL_TIM_PWM_Start(alarm_tim, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(alarm_tim, TIM_CHANNEL_1, 250);

    if (!buzzer_leds_blink)
    {
        Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_SET);
    }
}

void Alarm_LedStart(uint32_t duration_ms, uint8_t led_mask, bool blink_leds)
{
    led_active = true;
    led_blink = blink_leds;
    led_duration = duration_ms;
    led_start_time = HAL_GetTick();
    active_led_mask = led_mask;

    if (!led_blink)
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

    alarm_active = false;
    buzzer_phase_on = false;
    buzzer_leds_blink = false;

    alarm_duration = 0;
    alarm_start_time = 0;
    phase_start_time = 0;

    Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_RESET);
}

void Alarm_LedStop(void)
{
    led_active = false;
    led_blink = false;
    led_duration = 0;
    led_start_time = 0;

    Alarm_SetSelectedLeds(active_led_mask, GPIO_PIN_RESET);
}

bool Alarm_IsActive(void)
{
    return alarm_active || led_active;
}

void Alarm_Update(void)
{
    uint32_t now = HAL_GetTick();

    /* =========================
       Allarme buzzer + LED
       ========================= */
    if (alarm_active)
    {
        if ((now - alarm_start_time) >= alarm_duration)
        {
            Alarm_Stop();
        }
        else
        {
            /* Buzzer: 2s ON, 1s OFF */
            if (buzzer_phase_on)
            {
                if ((now - phase_start_time) >= 2000U)
                {
                    buzzer_phase_on = false;
                    phase_start_time = now;
                    HAL_TIM_PWM_Stop(alarm_tim, TIM_CHANNEL_1);
                }
            }
            else
            {
                if ((now - phase_start_time) >= 1000U)
                {
                    buzzer_phase_on = true;
                    phase_start_time = now;
                    HAL_TIM_PWM_Start(alarm_tim, TIM_CHANNEL_1);
                    __HAL_TIM_SET_COMPARE(alarm_tim, TIM_CHANNEL_1, 250);
                }
            }

            if (buzzer_leds_blink)
            {
                if (((now / 150U) % 2U) == 0U)
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
    }

    /* =========================
       Solo LED
       ========================= */
    if (led_active)
    {
        if ((now - led_start_time) >= led_duration)
        {
            Alarm_LedStop();
        }
        else
        {
            if (led_blink)
            {
                if (((now / 150U) % 2U) == 0U)
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
    }
}
