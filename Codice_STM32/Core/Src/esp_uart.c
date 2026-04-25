#include "esp_uart.h"
#include "fsm.h"
#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *esp_huart = NULL;

static uint8_t rxByte;
static char rxLine[64];
static uint8_t rxIndex = 0;

static bool ackOtpSent = false;
static bool ackLockdownSent = false;
static bool otpAvailable = false;
static char receivedOtp[7] = {0};

static void ESP_UART_ProcessLine(const char *line)
{
	char clean[64];
	strncpy(clean, line, sizeof(clean) - 1);
	clean[sizeof(clean) - 1] = '\0';

	//Remove \r if present
	int len = strlen(clean);
	if (len > 0 && clean[len - 1] == '\r') {
		clean[len - 1] = '\0';
	}
    if (strcmp(clean, "ACK_OTP_SENT") == 0)
    {
        ackOtpSent = true;
    }
    else if (strcmp(clean, "ACK_LOCKDOWN_SENT") == 0)
    {
        ackLockdownSent = true;
    }
    else if (strncmp(clean, "OTP:", 4) == 0)
    {
        strncpy(receivedOtp, clean + 4, 6);
        receivedOtp[6] = '\0';
        otpAvailable = true;
    }
}
void ESP_UART_Init(UART_HandleTypeDef *huart)
{
    esp_huart = huart;
    rxIndex = 0;
    memset(rxLine, 0, sizeof(rxLine));
    HAL_UART_Receive_IT(esp_huart, &rxByte, 1);
}

void ESP_UART_Update(void)
{
}

void ESP_UART_SendCommand(const char *cmd)
{
    if (esp_huart == NULL || cmd == NULL) return;

    HAL_UART_Transmit(esp_huart, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    HAL_UART_Transmit(esp_huart, (uint8_t *)"\n", 1, HAL_MAX_DELAY);
}

void ESP_UART_SendLockdownAlert(void)
{
    ESP_UART_SendCommand("LOCKDOWN_ALERT");
}

void ESP_UART_SendOtp(const char *otp)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "OTP_SEND:%s", otp);
    ESP_UART_SendCommand(cmd);
}

bool ESP_UART_IsAckOtpSent(void)
{
    return ackOtpSent;
}

bool ESP_UART_IsAckLockdownSent(void)
{
    return ackLockdownSent;
}

bool ESP_UART_HasOtp(void)
{
    return otpAvailable;
}

const char* ESP_UART_GetOtp(void)
{
    return receivedOtp;
}

void ESP_UART_ClearAckOtpSent(void)
{
    ackOtpSent = false;
}

void ESP_UART_ClearAckLockdownSent(void)
{
    ackLockdownSent = false;
}

void ESP_UART_ClearOtp(void)
{
    otpAvailable = false;
    memset(receivedOtp, 0, sizeof(receivedOtp));
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (esp_huart == NULL) return;

    if (huart == esp_huart)
    {
        if (rxByte == '\n' || rxByte == '\r')
        {
            if (rxIndex > 0)
            {
                rxLine[rxIndex] = '\0';
                ESP_UART_ProcessLine(rxLine);
                rxIndex = 0;
                memset(rxLine, 0, sizeof(rxLine));
            }
        }
        else
        {
            if (rxIndex < sizeof(rxLine) - 1)
            {
                rxLine[rxIndex++] = (char)rxByte;
            }
        }

        HAL_UART_Receive_IT(esp_huart, &rxByte, 1);
    }
}
