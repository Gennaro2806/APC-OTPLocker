#ifndef FSM_SAFE_H
#define FSM_SAFE_H

#include "main.h"

typedef enum{
	IDLE,
	INSERT_PIN,
	CHECK_PIN,
	LOCKED,
	SEND_OTP,
	WAIT_OTP,
	INSERT_OTP,
	CHECK_OTP,
	OPEN,
	RESET_LOCKER
}FSM_Locker;

void FSM_Init(void);
void FSM_Update(void);

#endif
