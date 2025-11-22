#include "state_machine.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t stateQ;
extern QueueHandle_t busStateQ;
extern QueueHandle_t morseQ;
extern QueueHandle_t uiQ;

main_state currentState = STATE_IDLE;
bus_state currentBusState = BUS_IDLE;

void changeState(main_state newState) {
    currentState = newState;
}

void changeBusState(bus_state newState) {
    currentBusState = newState;
}

void stateMachineTask(void *pvParameters) {
    main_state nextMainState;

    for (;;) {
        if (xQueueReceive(stateQ, &nextMainState, portMAX_DELAY)) {
            switch (nextMainState) {
                case STATE_IDLE:
                    changeState(STATE_IDLE);
                    break;
                case STATE_MENU:
                    changeState(STATE_MENU);
                    break;
                case STATE_USB_CONNECTED:
                    changeState(STATE_USB_CONNECTED);
                    break;
                case STATE_WIFI_CONNECTED:
                    changeState(STATE_WIFI_CONNECTED);
                    break;
            }
        }

        bus_state nextBusState;

        if (xQueueReceive(busStateQ, &nextBusState, portMAX_DELAY)) {
            switch (nextBusState) {
                case BUS_IDLE:
                    changeBusState(BUS_IDLE);
                    break;
                case BUS_UI_UPDATE:
                    changeBusState(BUS_UI_UPDATE);
                    break;
                case BUS_READ_SENSOR:
                    changeBusState(BUS_READ_SENSOR);
                    break;
            }
        }
    }
}
