#ifndef ALARM_H
#define ALARM_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* Maschere LED */
#define ALARM_LED3   (1U << 0)   /* ROSSO  */
#define ALARM_LED4   (1U << 1)   /* BLUE   */
#define ALARM_LED5   (1U << 2)   /* VERDE  */

void Alarm_Init(TIM_HandleTypeDef *htim);
void Alarm_Start(uint32_t duration_ms, uint8_t led_mask, bool blink_leds);
void Alarm_Stop(void);
bool Alarm_IsActive(void);
void Alarm_Update(void);

/* Nuove funzioni dedicate ai soli LED */
void Alarm_LedStart(uint32_t duration_ms, uint8_t led_mask, bool blink_leds);
void Alarm_LedStop(void);

#endif
