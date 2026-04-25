#include "fsm.h"

#include "alarm.h"
#include "servo.h"
#include "ssd1306.h"
#include "esp_uart.h"
#include "main.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* =========================
   CONFIGURAZIONE GENERALE
   ========================= */

#define USER_PIN_LENGTH              4
#define ADMIN_PIN_LENGTH             6
#define OTP_LENGTH                   6

#define MAX_PIN_ERRORS               3
#define MAX_OTP_ERRORS               2

#define PIN_ERROR_SIGNAL_MS          1200
#define ADMIN_ERROR_SIGNAL_MS        1200
#define NEW_PIN_MISMATCH_MS          1500

#define LOCKDOWN_DURATION_MS         10000
#define OPENING_SETTLE_MS            700
#define OPEN_STATE_TIMEOUT_MS        100000
#define OTP_ENTRY_TIMEOUT_MS         60000
#define NEW_PIN_NOT_VALID_TIME       2000

/* bypass temporaneo ACK ESP32 */
#define OTP_SEND_SCREEN_MS           3000

/* Maschere LED esterni:
   ALARM_LED3 -> ROSSO
   ALARM_LED4 -> BLUE
   ALARM_LED5 -> VERDE */
#define FSM_LED_RED_MASK             (ALARM_LED3)
#define FSM_LED_BLUE_MASK            (ALARM_LED4)
#define FSM_LED_GREEN_MASK           (ALARM_LED5)

/* =========================
   STATO INTERNO
   ========================= */

static SystemState currentState;
static bool stateEntryPending = true;
static uint32_t stateEntryTime = 0;

/* PIN persistenti */
static char userPin[USER_PIN_LENGTH + 1]   = "1234";
static char adminPin[ADMIN_PIN_LENGTH + 1] = "999999";

/* buffer utente */
static char pinBuffer[USER_PIN_LENGTH + 1];
static uint8_t pinIndex = 0;

/* buffer admin */
static char adminPinBuffer[ADMIN_PIN_LENGTH + 1];
static uint8_t adminPinIndex = 0;

/* nuovo PIN utente */
static char newUserPinBuffer[USER_PIN_LENGTH + 1];
static uint8_t newUserPinIndex = 0;

/* conferma nuovo PIN utente */
static char confirmPinBuffer[USER_PIN_LENGTH + 1];
static uint8_t confirmPinIndex = 0;

/* OTP */
static char storedOtp[OTP_LENGTH + 1];
static char otpBuffer[OTP_LENGTH + 1];
static uint8_t otpIndex = 0;

static bool espAckReceived = false;
static bool otpReceivedFromEsp = false;
static bool otpValid = false;

/* contatori */
static uint8_t pinErrorCount = 0;
static uint8_t otpErrorCount = 0;

/* stato porta */
static bool servoIsOpen = false;

/* Timestamp per il timeout su ACK OTP */
static uint32_t otpSendTimeStamp = 0;

/* =========================
   HELPER PRIVATI
   ========================= */

static void FSM_GenerateOtp(void)
{
    uint32_t value = HAL_GetTick() % 1000000;
    snprintf(storedOtp, sizeof(storedOtp), "%06lu", (unsigned long)value);
}

static void FSM_Transition(SystemState nextState)
{
    currentState = nextState;
    stateEntryPending = true;
    stateEntryTime = HAL_GetTick();
}

static bool FSM_StateJustEntered(void)
{
    if (stateEntryPending)
    {
        stateEntryPending = false;
        return true;
    }
    return false;
}

static void FSM_ClearBuffer(char *buffer, uint8_t len, uint8_t *index)
{
    memset(buffer, 0, len + 1);
    *index = 0;
}

static bool FSM_IsDigit(char key)
{
    return (key >= '0' && key <= '9');
}

static void FSM_DisplayMessage(const char *line1, const char *line2)
{
    SSD1306_Clear();

    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString((char *)line1);

    SSD1306_SetCursor(0, 2);
    SSD1306_WriteString((char *)line2);

    SSD1306_UpdateScreen();
}

static void FSM_DisplayMaskedInput(const char *title, uint8_t count)
{
    char stars[ADMIN_PIN_LENGTH + 1];
    uint8_t i;

    for (i = 0; i < count && i < ADMIN_PIN_LENGTH; i++)
    {
        stars[i] = '*';
    }
    stars[i] = '\0';

    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString((char *)title);
    SSD1306_SetCursor(0, 2);
    SSD1306_WriteString(stars);
    SSD1306_UpdateScreen();
}

static void FSM_AppendDigit(char *buffer, uint8_t *index, uint8_t maxLen, char key)
{
    if (*index < maxLen)
    {
        buffer[*index] = key;
        (*index)++;
        buffer[*index] = '\0';
    }
}

static void FSM_RemoveLastDigit(char *buffer, uint8_t *index)
{
    if (*index > 0)
    {
        (*index)--;
        buffer[*index] = '\0';
    }
}

static void FSM_ResetSessionData(void)
{
    FSM_ClearBuffer(pinBuffer, USER_PIN_LENGTH, &pinIndex);
    FSM_ClearBuffer(adminPinBuffer, ADMIN_PIN_LENGTH, &adminPinIndex);
    FSM_ClearBuffer(newUserPinBuffer, USER_PIN_LENGTH, &newUserPinIndex);
    FSM_ClearBuffer(confirmPinBuffer, USER_PIN_LENGTH, &confirmPinIndex);
    FSM_ClearBuffer(otpBuffer, OTP_LENGTH, &otpIndex);

    memset(storedOtp, 0, sizeof(storedOtp));

    espAckReceived = false;
    otpReceivedFromEsp = false;
    otpValid = false;
    otpErrorCount = 0;
}

static void FSM_ShowReadyScreen(void)
{
    FSM_DisplayMessage("INSERT PIN", "");
}

static void FSM_SignalPinError(void)
{
    Alarm_LedStart(PIN_ERROR_SIGNAL_MS, FSM_LED_RED_MASK, false);
}

static void FSM_SignalAdminError(void)
{
    Alarm_LedStart(ADMIN_ERROR_SIGNAL_MS, FSM_LED_RED_MASK, false);
}

static void FSM_SignalPinMismatch(void)
{
    Alarm_LedStart(NEW_PIN_MISMATCH_MS, FSM_LED_RED_MASK, false);
}

static void FSM_SignalSuccess(void)
{
    Alarm_LedStart(2000, FSM_LED_GREEN_MASK, false);
}

/* =========================
   API PUBBLICA
   ========================= */

void FSM_Init(void)
{
    currentState = STATE_BOOT;
    stateEntryPending = true;
    stateEntryTime = HAL_GetTick();

    pinErrorCount = 0;
    otpErrorCount = 0;
    servoIsOpen = false;

    FSM_ResetSessionData();
}

SystemState FSM_GetState(void)
{
    return currentState;
}

void FSM_OnEspAckOtpSent(void)
{
    espAckReceived = true;
}

void FSM_OnEspOtpReceived(const char *otp)
{
    if (otp == NULL)
        return;

    strncpy(storedOtp, otp, OTP_LENGTH);
    storedOtp[OTP_LENGTH] = '\0';
    otpReceivedFromEsp = true;
    otpValid = true;
}

/* =========================
   FSM UPDATE
   ========================= */

void FSM_Update(char key)
{
    switch (currentState)
    {
        case STATE_BOOT:
        {
            if (FSM_StateJustEntered())
            {
                FSM_ResetSessionData();
                pinErrorCount = 0;
                otpErrorCount = 0;

                Servo_Close();
                servoIsOpen = false;
                Alarm_LedStop();

                FSM_DisplayMessage("BOOTING", "SYSTEM READY");
            }

            if (HAL_GetTick() - stateEntryTime >= 3000)
            {
                FSM_Transition(STATE_IDLE);
            }
            break;
        }

        case STATE_IDLE:
        {
            if (FSM_StateJustEntered())
            {
                FSM_ResetSessionData();
                Alarm_LedStop();
                FSM_ShowReadyScreen();
            }

            if (FSM_IsDigit(key))
            {
                FSM_ClearBuffer(pinBuffer, USER_PIN_LENGTH, &pinIndex);
                FSM_AppendDigit(pinBuffer, &pinIndex, USER_PIN_LENGTH, key);
                FSM_DisplayMaskedInput("PIN:", pinIndex);
                FSM_Transition(STATE_PIN_ENTRY);
            }

            break;
        }

        case STATE_PIN_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
                Alarm_LedStop();
                FSM_DisplayMaskedInput("PIN:", pinIndex);
            }

            if (FSM_IsDigit(key))
            {
                FSM_AppendDigit(pinBuffer, &pinIndex, USER_PIN_LENGTH, key);
                FSM_DisplayMaskedInput("PIN:", pinIndex);
            }
            else if (key == '*')
            {
                FSM_RemoveLastDigit(pinBuffer, &pinIndex);
                FSM_DisplayMaskedInput("PIN:", pinIndex);
            }
            else if (key == 'A')
            {
                FSM_ClearBuffer(pinBuffer, USER_PIN_LENGTH, &pinIndex);
                FSM_Transition(STATE_IDLE);
                break;
            }
            else if (key == '#')
            {
                if (pinIndex == USER_PIN_LENGTH)
                {
                    FSM_Transition(STATE_PIN_VALIDATE);
                }
            }

            break;
        }

        case STATE_PIN_VALIDATE:
        {
            if (FSM_StateJustEntered())
            {
                if (strncmp(pinBuffer, userPin, USER_PIN_LENGTH) == 0)
                {
                    FSM_ClearBuffer(pinBuffer, USER_PIN_LENGTH, &pinIndex);
                    FSM_SignalSuccess();
                    FSM_Transition(STATE_SEND_OTP);
                }
                else
                {
                    FSM_ClearBuffer(pinBuffer, USER_PIN_LENGTH, &pinIndex);
                    FSM_Transition(STATE_PIN_ERROR);
                }
            }

            break;
        }

        case STATE_PIN_ERROR:
        {
            if (FSM_StateJustEntered())
            {
                pinErrorCount++;
                FSM_DisplayMessage("WRONG PIN", "");
                FSM_SignalPinError();
            }

            if (HAL_GetTick() - stateEntryTime >= PIN_ERROR_SIGNAL_MS)
            {
                if (pinErrorCount >= MAX_PIN_ERRORS)
                {
                    FSM_Transition(STATE_LOCKDOWN);
                }
                else
                {
                    FSM_Transition(STATE_IDLE);
                }
            }

            break;
        }

        case STATE_LOCKDOWN:
        {
            if (FSM_StateJustEntered())
            {
                Servo_Close();
                servoIsOpen = false;

                Alarm_Start(LOCKDOWN_DURATION_MS, FSM_LED_RED_MASK, true);
                FSM_DisplayMessage("LOCKDOWN", "ADMIN WAIT");

                ESP_UART_SendLockdownAlert();
            }

            if (HAL_GetTick() - stateEntryTime >= LOCKDOWN_DURATION_MS)
            {
                Alarm_Stop();
                FSM_Transition(STATE_WAIT_ADMIN_PIN_ENTRY);
            }

            break;
        }

        case STATE_WAIT_ADMIN_PIN_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
                Alarm_LedStop();
                FSM_ClearBuffer(adminPinBuffer, ADMIN_PIN_LENGTH, &adminPinIndex);
                FSM_DisplayMaskedInput("ADMIN PIN:", adminPinIndex);
            }

            if (FSM_IsDigit(key))
            {
                FSM_AppendDigit(adminPinBuffer, &adminPinIndex, ADMIN_PIN_LENGTH, key);
                FSM_DisplayMaskedInput("ADMIN PIN:", adminPinIndex);
            }
            else if (key == '*')
            {
                FSM_RemoveLastDigit(adminPinBuffer, &adminPinIndex);
                FSM_DisplayMaskedInput("ADMIN PIN:", adminPinIndex);
            }
            else if (key == '#')
            {
                if (adminPinIndex == ADMIN_PIN_LENGTH)
                {
                    FSM_Transition(STATE_WAIT_ADMIN_PIN_VALIDATE);
                }
            }

            break;
        }

        case STATE_WAIT_ADMIN_PIN_VALIDATE:
        {
            if (FSM_StateJustEntered())
            {
                if (strncmp(adminPinBuffer, adminPin, ADMIN_PIN_LENGTH) == 0)
                {
                    FSM_SignalSuccess();
                    FSM_Transition(STATE_WAIT_ADMIN_NEW_USER_PIN_ENTRY);
                }
                else
                {
                    FSM_DisplayMessage("ADMIN WRONG", "");
                    FSM_SignalAdminError();
                }
            }

            if (!stateEntryPending && (HAL_GetTick() - stateEntryTime >= ADMIN_ERROR_SIGNAL_MS))
            {
                if (strncmp(adminPinBuffer, adminPin, ADMIN_PIN_LENGTH) != 0)
                {
                    FSM_Transition(STATE_WAIT_ADMIN_PIN_ENTRY);
                }
            }

            break;
        }

        case STATE_WAIT_ADMIN_NEW_USER_PIN_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
                Alarm_LedStop();
                FSM_ClearBuffer(newUserPinBuffer, USER_PIN_LENGTH, &newUserPinIndex);
                FSM_DisplayMaskedInput("NEW USER PIN", newUserPinIndex);
            }

            if (FSM_IsDigit(key))
            {
                FSM_AppendDigit(newUserPinBuffer, &newUserPinIndex, USER_PIN_LENGTH, key);
                FSM_DisplayMaskedInput("NEW USER PIN", newUserPinIndex);
            }
            else if (key == '*')
            {
                FSM_RemoveLastDigit(newUserPinBuffer, &newUserPinIndex);
                FSM_DisplayMaskedInput("NEW USER PIN", newUserPinIndex);
            }
            else if (key == '#')
            {
                if (newUserPinIndex == USER_PIN_LENGTH)
                {
                    FSM_Transition(STATE_WAIT_ADMIN_NEW_USER_PIN_CONFIRM);
                }
            }

            break;
        }

        case STATE_WAIT_ADMIN_NEW_USER_PIN_CONFIRM:
        {
            if (FSM_StateJustEntered())
            {
                FSM_ClearBuffer(confirmPinBuffer, USER_PIN_LENGTH, &confirmPinIndex);
                FSM_DisplayMaskedInput("CONFIRM PIN", confirmPinIndex);
            }

            if (FSM_IsDigit(key))
            {
                FSM_AppendDigit(confirmPinBuffer, &confirmPinIndex, USER_PIN_LENGTH, key);
                FSM_DisplayMaskedInput("CONFIRM PIN", confirmPinIndex);
            }
            else if (key == '*')
            {
                FSM_RemoveLastDigit(confirmPinBuffer, &confirmPinIndex);
                FSM_DisplayMaskedInput("CONFIRM PIN", confirmPinIndex);
            }
            else if (key == '#')
            {
                if (confirmPinIndex == USER_PIN_LENGTH)
                {
                    if (strncmp(newUserPinBuffer, confirmPinBuffer, USER_PIN_LENGTH) == 0)
                    {
                        if (strncmp(newUserPinBuffer, userPin, USER_PIN_LENGTH) == 0)
                        {
                            FSM_DisplayMessage("PIN UNCHANGED", "TRY ANOTHER");
                            FSM_SignalPinMismatch();
                            HAL_Delay(NEW_PIN_NOT_VALID_TIME);
                            FSM_Transition(STATE_WAIT_ADMIN_NEW_USER_PIN_ENTRY);
                        }
                        else
                        {
                            strncpy(userPin, newUserPinBuffer, USER_PIN_LENGTH);
                            userPin[USER_PIN_LENGTH] = '\0';

                            pinErrorCount = 0;
                            otpErrorCount = 0;

                            FSM_DisplayMessage("PIN UPDATED", "");
                            FSM_SignalSuccess();
                            FSM_Transition(STATE_IDLE);
                        }
                    }
                    else
                    {
                        FSM_DisplayMessage("PIN MISMATCH", "");
                        FSM_SignalPinMismatch();
                        FSM_Transition(STATE_WAIT_ADMIN_NEW_USER_PIN_ENTRY);
                    }
                }
            }

            break;
        }

        case STATE_SEND_OTP:
        {
            if (FSM_StateJustEntered())
            {
                FSM_ClearBuffer(otpBuffer, OTP_LENGTH, &otpIndex);
                espAckReceived = false;
                otpReceivedFromEsp = false;

                FSM_GenerateOtp();
                ESP_UART_SendOtp(storedOtp);

                FSM_DisplayMessage("SENDING OTP", "");

                otpSendTimeStamp = HAL_GetTick();
            } else {
            	if (espAckReceived)
            	{
            		otpValid = true;
            		FSM_Transition(STATE_WAIT_OTP);
            	}
            	else if ((HAL_GetTick() - otpSendTimeStamp) >= 5000)
				{
            		otpValid = true;
            		FSM_DisplayMessage("OTP TIMEOUT", "CHECK PHONE");
            		HAL_Delay(3000);
            		FSM_Transition(STATE_OTP_ENTRY);
				}
            }



            break;
        }

        case STATE_WAIT_OTP:
        {
            if (FSM_StateJustEntered())
            {
                FSM_DisplayMessage("OTP SENT", "ENTER OTP");
            }

            if (otpValid)
            {
                FSM_Transition(STATE_OTP_ENTRY);
            }

            break;
        }

        case STATE_OTP_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
                Alarm_LedStop();
                FSM_ClearBuffer(otpBuffer, OTP_LENGTH, &otpIndex);
                FSM_DisplayMaskedInput("ENTER OTP", otpIndex);
            }

            if (HAL_GetTick() - stateEntryTime >= OTP_ENTRY_TIMEOUT_MS)
            {
                otpValid = false;
                FSM_ClearBuffer(otpBuffer, OTP_LENGTH, &otpIndex);
                FSM_Transition(STATE_IDLE);
                break;
            }

            if (FSM_IsDigit(key))
            {
                FSM_AppendDigit(otpBuffer, &otpIndex, OTP_LENGTH, key);
                FSM_DisplayMaskedInput("ENTER OTP", otpIndex);
            }
            else if (key == '*')
            {
                FSM_RemoveLastDigit(otpBuffer, &otpIndex);
                FSM_DisplayMaskedInput("ENTER OTP", otpIndex);
            }
            else if (key == '#')
            {
                if (otpIndex == OTP_LENGTH)
                {
                    FSM_Transition(STATE_OTP_VALIDATE);
                }
            }

            break;
        }

        case STATE_OTP_VALIDATE:
        {
            if (FSM_StateJustEntered())
            {
                if (otpValid && strncmp(otpBuffer, storedOtp, OTP_LENGTH) == 0)
                {
                    otpErrorCount = 0;
                    FSM_SignalSuccess();
                    FSM_Transition(STATE_OPENING);
                }
                else
                {
                    otpErrorCount++;

                    if (otpErrorCount >= MAX_OTP_ERRORS)
                    {
                        FSM_Transition(STATE_OTP_ERROR);
                    }
                    else
                    {
                        FSM_Transition(STATE_OTP_ENTRY);
                    }
                }
            }

            break;
        }

        case STATE_OTP_ERROR:
        {
            if (FSM_StateJustEntered())
            {
                Alarm_Start(2000, FSM_LED_RED_MASK, true);
                FSM_DisplayMessage("WRONG OTP", "ADMIN REQUIRED");
                otpValid = false;
            }

            if (HAL_GetTick() - stateEntryTime >= 2000)
            {
                Alarm_Stop();
                FSM_Transition(STATE_WAIT_ADMIN_PIN_ENTRY);
            }

            break;
        }

        case STATE_OPENING:
        {
            if (FSM_StateJustEntered())
            {
                FSM_DisplayMessage("OPENING", "");
                Servo_Open();
                servoIsOpen = true;
            }

            if (HAL_GetTick() - stateEntryTime >= OPENING_SETTLE_MS)
            {
                FSM_Transition(STATE_OPEN);
            }

            break;
        }

        case STATE_OPEN:
        {
            if (FSM_StateJustEntered())
            {
                HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
                FSM_DisplayMessage("OPEN", "# TO CLOSE");
            }

            if (key == '#')
            {
                FSM_Transition(STATE_CLOSING);
            }
            else if (HAL_GetTick() - stateEntryTime >= OPEN_STATE_TIMEOUT_MS)
            {
                FSM_Transition(STATE_CLOSING);
            }

            break;
        }

        case STATE_CLOSING:
        {
            if (FSM_StateJustEntered())
            {
                FSM_DisplayMessage("CLOSING", "");
                Servo_Close();
                servoIsOpen = false;

                FSM_ResetSessionData();
                pinErrorCount = 0;
                otpErrorCount = 0;
            }

            if (HAL_GetTick() - stateEntryTime >= 700)
            {
                FSM_Transition(STATE_IDLE);
            }

            break;
        }

        default:
            FSM_Transition(STATE_BOOT);
            break;
    }
}
