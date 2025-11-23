
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include "queue.h"
#include "sensor_task.h"
#include "usb_task.h"
#include "queue.h"
#include "event.h"
#include "button_task.h"
#include "tusb.h"
#include "ui_task.h"
#include "state_machine.h"

#define CDC_ITF_TX 1
#define BUFFER_SIZE 1024

QueueHandle_t stateQ;
QueueHandle_t busStateQ;
QueueHandle_t morseQ;
QueueHandle_t uiQ;
QueueHandle_t usbRxQ;


int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    init_hat_sdk();
    init_i2c_default();

    // Initializions for the IMU
    int rc = init_ICM42670();
    if (rc != 0) { while (1) { printf("[SENSOR] IMU init FAIL\r\n"); sleep_ms(1000); } }

    rc = ICM42670_start_with_default_values();
    if (rc != 0) { while (1) { printf("[SENSOR] IMU start FAIL\r\n"); sleep_ms(1000); } }

    // Jonot tilakomennoille
    stateQ = xQueueCreate(64, sizeof(main_state));
    configASSERT(stateQ != NULL);

    busStateQ = xQueueCreate(64, sizeof(bus_state));
    configASSERT(busStateQ != NULL);

    // Jono morsemerkeille
    morseQ = xQueueCreate(64, sizeof(symbol_ev_t));
    configASSERT(morseQ != NULL);

    // Jono UI-komennoille
    uiQ = xQueueCreate(10, sizeof(ui_cmd_t));
    configASSERT(uiQ != NULL);

    // Jono USB-vastaanotolle
    usbRxQ = xQueueCreate(10, sizeof(uint8_t) * (CFG_TUD_CDC_RX_BUFSIZE + 1));
    configASSERT(usbRxQ != NULL);

    // State machine
    xTaskCreate(stateMachineTask, "State", 1024, NULL, 2, NULL);

    // Create UI task
    xTaskCreate(ui_task, "UI", 2048, NULL, 2, NULL);
    
    // Create Sensor Task
    xTaskCreate(sensorTask, "Sensor", 2048, NULL, 1, NULL);

    // Create button task
    xTaskCreate(buttonTask, "Buttons", 1024, NULL, 1, NULL);

    // Create Usb task
    TaskHandle_t handle_usb = NULL;
    xTaskCreate(usbTask, "usb", 1024, NULL, 3, &handle_usb);

    // Pin usb_handle to core 0
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(handle_usb, 1u << 0);
    #endif

    // Initialize TinyUSB 
    tusb_init();

    // Start FreeRTOS
    vTaskStartScheduler();

    // Should never reach here
    while (1);
}