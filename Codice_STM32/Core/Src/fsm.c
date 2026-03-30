#include "fsm.h"

#include "alarm.h"
#include "servo.h"
#include "ssd1306.h"

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

/* Per ora usiamo i LED onboard STM32.
   In futuro ti basterà cambiare queste maschere o il modulo alarm. */
#define FSM_LED_ERROR_MASK           (ALARM_LED8 | ALARM_LED9)
#define FSM_LED_LOCKDOWN_MASK        (ALARM_LED8 | ALARM_LED9 | ALARM_LED10)
#define FSM_LED_OPEN_MASK            (ALARM_LED3 | ALARM_LED4 | ALARM_LED5)

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

/* =========================
   HELPER PRIVATI
   ========================= */

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
    Alarm_Start(PIN_ERROR_SIGNAL_MS, FSM_LED_ERROR_MASK, false);
}

static void FSM_SignalAdminError(void)
{
    Alarm_Start(ADMIN_ERROR_SIGNAL_MS, FSM_LED_ERROR_MASK, false);
}

static void FSM_SignalPinMismatch(void)
{
    Alarm_Start(NEW_PIN_MISMATCH_MS, FSM_LED_ERROR_MASK, false);
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

/* Hook futuri per ESP32 */
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

                if (pinIndex == 0)
                {
                    /* puoi scegliere se restare qui o tornare a IDLE.
                       Per ora restiamo qui, come tastiera "vuota". */
                }
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

                Alarm_Start(LOCKDOWN_DURATION_MS, FSM_LED_LOCKDOWN_MASK, true);

                FSM_DisplayMessage("LOCKDOWN", "ADMIN WAIT");
            }

            if (HAL_GetTick() - stateEntryTime >= LOCKDOWN_DURATION_MS)
            {
                FSM_Transition(STATE_WAIT_ADMIN_PIN_ENTRY);
            }

            break;
        }

        case STATE_WAIT_ADMIN_PIN_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
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
                    FSM_Transition(STATE_WAIT_ADMIN_NEW_PIN_ENTRY);
                }
                else
                {
                    FSM_DisplayMessage("ADMIN WRONG", "");
                    FSM_SignalAdminError();
                }
            }

            if (!stateEntryPending && (HAL_GetTick() - stateEntryTime >= ADMIN_ERROR_SIGNAL_MS))
            {
                if (strncmp(adminPinBuffer, adminPin, ADMIN_PIN_LENGTH) == 0)
                {
                    /* già transizionato sopra, qui non dovrebbe servire */
                }
                else
                {
                    FSM_Transition(STATE_WAIT_ADMIN_PIN_ENTRY);
                }
            }

            break;
        }

        case STATE_WAIT_ADMIN_NEW_PIN_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
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
                    FSM_Transition(STATE_WAIT_ADMIN_NEW_PIN_CONFIRM);
                }
            }

            break;
        }

        case STATE_WAIT_ADMIN_NEW_PIN_CONFIRM:
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
                        strncpy(userPin, newUserPinBuffer, USER_PIN_LENGTH);
                        userPin[USER_PIN_LENGTH] = '\0';

                        pinErrorCount = 0;
                        otpErrorCount = 0;

                        FSM_DisplayMessage("PIN UPDATED", "");
                        FSM_Transition(STATE_IDLE);
                    }
                    else
                    {
                        FSM_DisplayMessage("PIN MISMATCH", "");
                        FSM_SignalPinMismatch();
                        FSM_Transition(STATE_WAIT_ADMIN_NEW_PIN_ENTRY);
                    }
                }
            }

            break;
        }

        /* =========================
           SECONDA META' - SCAFFOLD
           ========================= */

        case STATE_SEND_OTP:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO:
                   - inviare comando UART a ESP32
                   - OLED: SENDING OTP
                   - azzerare buffer OTP
                */
                FSM_DisplayMessage("SENDING OTP", "");
            }

            /* TODO: versione robusta con ACK */
            break;
        }

        case STATE_WAIT_OTP:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO: OLED "CHECK PHONE" / "OTP SENT" */
                FSM_DisplayMessage("CHECK PHONE", "ENTER OTP");
            }

            /* TODO */
            break;
        }

        case STATE_OTP_ENTRY:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO */
                FSM_DisplayMessage("ENTER OTP", "");
            }

            /* TODO */
            break;
        }

        case STATE_OTP_VALIDATE:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO approccio A:
                   confronto otpBuffer vs storedOtp
                */
            }

            break;
        }

        case STATE_OTP_ERROR:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO */
                FSM_DisplayMessage("WRONG OTP", "");
            }

            break;
        }

        case STATE_OPENING:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO:
                   - LED apertura lampeggiante
                   - Servo_Open()
                   - OLED OPENING
                */
                Servo_Open();
                servoIsOpen = true;
                FSM_DisplayMessage("OPENING", "");
            }

            /* TODO */
            break;
        }

        case STATE_OPEN:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO */
                FSM_DisplayMessage("OPEN", "# TO CLOSE");
            }

            /* TODO */
            break;
        }

        case STATE_CLOSING:
        {
            if (FSM_StateJustEntered())
            {
                /* TODO */
                Servo_Close();
                servoIsOpen = false;
                FSM_DisplayMessage("CLOSING", "");
            }

            /* TODO */
            break;
        }

        default:
            FSM_Transition(STATE_BOOT);
            break;
    }
}
