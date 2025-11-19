#pragma once
typedef enum {
    STATE_IDLE,
    STATE_SELECT_MODE,
    STATE_USB_CONNECTED,
    STATE_WIFI_CONNECTED
} State_t;

extern volatile State_t currentState;

void stateMachineTask(void *pvParameters);
void changeState(State_t newState);
