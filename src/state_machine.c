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
bus_state currentBusState = BUS_UI_UPDATE; // Aloitetaan tervetulotekstillä

void changeState(main_state newState) {
    currentState = newState;
}

void changeBusState(bus_state newState) {
    currentBusState = newState;
}

void stateMachineTask(void *pvParameters) {
    main_state nextMainState;

    for (;;) {
        if (xQueueReceive(stateQ, &nextMainState, 50)) {
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
                case STATE_USB_RECEIVE:
                    changeState(STATE_USB_RECEIVE);
                    break;
                case STATE_USB_SEND:
                    changeState(STATE_USB_SEND);
                    break;
            }
        }

        bus_state nextBusState;

        if (xQueueReceive(busStateQ, &nextBusState, 50)) {
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
