#include "state_machine.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

volatile State_t currentState = STATE_IDLE;

void changeState(State_t newState) {
    currentState = newState;
}

void stateMachineTask(void *pvParameters) {
    
    // for (;;) {
    //     switch (currentState) {
    //         case STATE_IDLE:
    //             // Idle logic
    //             break;
    //         case STATE_MENU:
    //             // Mode selection logic
    //             break;
    //         case STATE_USB_CONNECTED:
    //             // USB logic
    //             break;
    //         case STATE_WIFI_CONNECTED:
    //             // Wi-Fi logic
    //             break;
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }
}
