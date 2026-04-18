#ifndef ESP_UART_H
#define ESP_UART_H

#include "main.h"
#include <stdbool.h>

void ESP_UART_Init(UART_HandleTypeDef *huart);
void ESP_UART_Update(void);

void ESP_UART_SendCommand(const char *cmd);
void ESP_UART_SendLockdownAlert(void);
void ESP_UART_SendOtp(const char *otp);

bool ESP_UART_IsAckOtpSent(void);
bool ESP_UART_IsAckLockdownSent(void);
bool ESP_UART_HasOtp(void);
const char* ESP_UART_GetOtp(void);

void ESP_UART_ClearAckOtpSent(void);
void ESP_UART_ClearAckLockdownSent(void);
void ESP_UART_ClearOtp(void);

#endif
