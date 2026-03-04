#include "FSM_safe.h"
#include <string.h>
#include <stdlib.h>

static SafeState currentState;

// Contatore per i tentativi di inserimento del PIN
static uint8_t attempts = 0;

// PIN della cassaforte, statico
static char correctPIN[]="1234";
static char insertedPIN[5];

// OTP Generato dal modulo ESP32 e ricevuto tramite telegram
static char generatedOTP[5];
static char insertedOTP[5];

// Timer per verificare il passaggio degli stati
static uint32_t stateTimer = 0;

void FSM_Init(void){
	currentState = IDLE;
}

void FSM_Update(void){
	switch(currentState){
	case IDLE:
		attempts = 0;
		currentState = INSERT_PIN;
		break;

	case INSERT_PIN:
		// Lettura keypad
		break;

	case CHECK_PIN:
		if(strcmp(insertedPIN, correctPIN) == 0)
		{
			currentState = SEND_OTP;
		}
		else
		{
			attempts++;
			if(attempts >= 3){
				stateTimer = HAL_GetTick();
				currentState = LOCKED;
			}
			else
			{
				currentState = INSERT_PIN;
			}
		}
		break;

	case LOCKED:
		if(HAL_GetTick() - stateTimer >= 100000) //sec
		{
			currentState = RESET;
		}
		break;
	case SEND_OTP:
		break;
	case WAIT_OTP:
		break;
	case INSERT_OTP:
		break;
	case CHECK_OTP:
		break;
	case OPEN:
		break;
	case RESET_LOCKER:
		break;
	}
}

