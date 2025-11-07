
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

extern void ui_task(void *params);

QueueHandle_t symbolQ;

int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    init_hat_sdk();
    init_i2c_default();

    symbolQ = xQueueCreate(64, sizeof(symbol_ev_t));
    configASSERT(symbolQ != NULL);


    // Create Sensor Task
    xTaskCreate(sensorTask, "Sensor", 2048, NULL, 1, NULL);


    // Create button task
    xTaskCreate(buttonTask, "Buttons", 1024, NULL, 1, NULL);

    // Create Usb task
    TaskHandle_t handle_usb;
    xTaskCreate(usbTask, "usb", 1024, NULL, 3, &handle_usb);

    // Create UI task
    xTaskCreate(uiTask, "UI", 2048, NULL, 2, NULL);
    

    // Start FreeRTOS
    vTaskStartScheduler();

    // Should never reach here
    while (1);
}