
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

extern State_t current_state;

QueueHandle_t morseQ;
QueueHandle_t stateQ;
QueueHandle_t uiQ;

// static void testTask(void *arg) {
//     char buf[BUFFER_SIZE];

//     while (!tud_mounted() || !tud_cdc_n_connected(1)) {
//         vTaskDelay(pdMS_TO_TICKS(50));
//     }

//     while (current_state = STATE_USB_CONNECTED) {
//         char test[] = "--.";
        
//         int i;
//         char test_char;
//         for (i=0; i < strlen(test); i++) {
//             test_char = test[i];
//             if (tud_cdc_n_connected(CDC_ITF_TX)) {
//             // Sends data using tud_cdc_write
//             snprintf(buf, BUFFER_SIZE, "%c", test_char);
//             tud_cdc_n_write(CDC_ITF_TX, buf, strlen(buf));
//             tud_cdc_n_write_flush(CDC_ITF_TX);
//             }
//             vTaskDelay(pdMS_TO_TICKS(500));
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    init_hat_sdk();
    init_i2c_default();

    morseQ = xQueueCreate(64, sizeof(symbol_ev_t));
    configASSERT(morseQ != NULL);

    stateQ = xQueueCreate(64, sizeof(State_t));
    configASSERT(stateQ != NULL);

    // Jono UI-komennoille
    uiQ = xQueueCreate(10, sizeof(ui_cmd_t));
    configASSERT(uiQ != NULL);

    currentState = STATE_IDLE;

    // State machine
    xTaskCreate(stateMachineTask, "State", 1024, NULL, 2, NULL);

    // Create UI task
    // xTaskCreate(ui_task, "UI", 2048, NULL, 2, NULL);
    
    // Create Sensor Task
    xTaskCreate(sensorTask, "Sensor", 2048, NULL, 1, NULL);

    // Create button task
<<<<<<< HEAD
    xTaskCreate(buttonTask, "Buttons", 1024, NULL, 3, NULL);
=======
    xTaskCreate(buttonTask, "Buttons", 1024, NULL, 1, NULL);
>>>>>>> 6150020686519e8f5dd1b54b58933317b73c71e5

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