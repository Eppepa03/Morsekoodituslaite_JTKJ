
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include "sensor_task.h"


int main(void) {
    stdio_init_all();
    sleep_ms(1500);

    init_hat_sdk();
    init_i2c_default();

    // Create Sensor Task
    xTaskCreate(vSensorTask, "Sensor", 1024, NULL, 1, NULL);

    // Start FreeRTOS
    vTaskStartScheduler();

    // Should never reach here
    while (1);
}