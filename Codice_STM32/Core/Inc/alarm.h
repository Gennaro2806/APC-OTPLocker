#ifndef ALARM_H
#define ALARM_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ALARM_LED3   (1 << 0)
#define ALARM_LED4   (1 << 1)
#define ALARM_LED5   (1 << 2)
#define ALARM_LED6   (1 << 3)
#define ALARM_LED7   (1 << 4)
#define ALARM_LED8   (1 << 5)
#define ALARM_LED9   (1 << 6)
#define ALARM_LED10  (1 << 7)

void Alarm_Init(TIM_HandleTypeDef *htim);
void Alarm_Start(uint32_t duration_ms, uint8_t led_mask, bool blink_leds);
void Alarm_Update(void);
void Alarm_Stop(void);
bool Alarm_IsActive(void);

#endif
