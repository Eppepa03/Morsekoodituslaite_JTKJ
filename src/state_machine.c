#include "state_machine.h"
#include "FreeRTOS.h"
#include "task.h"

volatile State_t currentState = STATE_IDLE;

void changeState(State_t newState) {
    currentState = newState;
}

void stateMachineTask(void *pvParameters) {
    for (;;) {
        switch (currentState) {
            case STATE_IDLE:
                // Idle logic
                break;
            case STATE_SELECT_MODE:
                // Mode selection logic
                break;
            case STATE_USB_CONNECTED:
                // USB logic
                break;
            case STATE_WIFI_CONNECTED:
                // Wi-Fi logic
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
