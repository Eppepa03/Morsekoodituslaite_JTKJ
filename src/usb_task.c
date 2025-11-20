#include "usb_task.h"
#include "pico/stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include "queue.h"

extern QueueHandle_t morseQ;

void usbTask(void *args) {
    char morseChar;
    for (;;) {
        tud_task(); // Always service USB first
        if (xQueueReceive(morseQ, &morseChar, 0)) {
            if (tud_cdc_connected()) {
                tud_cdc_write(&morseChar, 1);
                tud_cdc_write_flush();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
