#include "state_machine.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

State_t currentState = STATE_IDLE;

void changeState(State_t newState) {
    currentState = newState;
}

void stateMachineTask(void *pvParameters) {
    State_t nextState;

    for (;;) {
        if (xQueueReceive(stateQ, &nextState, portMAX_DELAY)) {
            switch (nextState) {
                case STATE_IDLE:
                    // Idle logic
                    changeState(STATE_IDLE);
                    break;
                case STATE_MENU:
                    // Mode selection logic
                    changeState(STATE_MENU);
                    break;
                case STATE_USB_CONNECTED:
                    // USB logic
                    changeState(STATE_USB_CONNECTED);
                    break;
                case STATE_WIFI_CONNECTED:
                    // Wi-Fi logic
                    changeState(STATE_WIFI_CONNECTED);
                    break;
            }
        }
    }
}
