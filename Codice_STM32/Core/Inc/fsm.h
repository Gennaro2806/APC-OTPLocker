#ifndef FSM_H
#define FSM_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATE_BOOT,
    STATE_IDLE,

    STATE_PIN_ENTRY,
    STATE_PIN_VALIDATE,
    STATE_PIN_ERROR,

    STATE_LOCKDOWN,

    STATE_WAIT_ADMIN_PIN_ENTRY,
    STATE_WAIT_ADMIN_PIN_VALIDATE,
    STATE_WAIT_ADMIN_NEW_PIN_ENTRY,
    STATE_WAIT_ADMIN_NEW_PIN_CONFIRM,

    STATE_SEND_OTP,
    STATE_WAIT_OTP,
    STATE_OTP_ENTRY,
    STATE_OTP_VALIDATE,
    STATE_OTP_ERROR,

    STATE_OPENING,
    STATE_OPEN,
    STATE_CLOSING
} SystemState;

void FSM_Init(void);
void FSM_Update(char key);
SystemState FSM_GetState(void);

/* hook futuri per ESP32 / UART */
void FSM_OnEspAckOtpSent(void);
void FSM_OnEspOtpReceived(const char *otp);

#endif
