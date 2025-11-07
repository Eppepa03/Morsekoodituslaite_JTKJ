#include "usb_task.h"
#include "pico/stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

void usbTask(void *arg) {
    (void)arg;
    while (1) {
        // Wait for usb tasks leaving the processor to the other tasks.
        tud_task();
    }
}