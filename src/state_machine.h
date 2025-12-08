#pragma once

// Main state machine
typedef enum {
    STATE_IDLE,
    STATE_MENU,
    STATE_USB_CONNECTED,
    STATE_WIFI_CONNECTED,
    STATE_USB_SEND, // Uusi
    STATE_USB_RECEIVE // Uusi
} main_state;

// Bus state machine
typedef enum {
    BUS_IDLE,
    BUS_UI_UPDATE,
    BUS_READ_SENSOR
} bus_state;

extern main_state currentState;
extern bus_state currentBusState;

void stateMachineTask(void *pvParameters);
void changeState(main_state newState);
void changeBusState(bus_state newState);
