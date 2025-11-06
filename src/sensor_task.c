#include "sensor_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include <stdio.h>
#include "event.h"
#include "queue.h"


extern QueueHandle_t symbolQ;


void sensorTask(void *pvParameters) {
    // Initialize IMU
    if (init_ICM42670() != 0) {
        printf("IMU init failed\r\n");
        vTaskDelete(NULL);
    }

    ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT, ICM42670_ACCEL_FSR_DEFAULT);
    ICM42670_startGyro(ICM42670_GYRO_ODR_DEFAULT, ICM42670_GYRO_FSR_DEFAULT);
    ICM42670_enable_accel_gyro_ln_mode();

    printf("Sensor task running...\r\n");

      while (1) {
        float ax, ay, az, gx, gy, gz, temp;

        symbol_ev_t ev;

        // --- VERY SIMPLE TEST LOGIC ---
        // Assume board held flat = dot
        // Tilted forward/back = dash
        if (fabsf(az) > 0.9f && fabsf(ax) < 0.2f && fabsf(ay) < 0.2f) {
            ev = DOT;
            xQueueSend(symbolQ, &ev, 0);
            printf(". (dot)\n");
        } 
        else if (fabsf(ax) > 0.9f && fabsf(ay) < 0.2f && fabsf(az) < 0.5f) {
            ev = DASH;
            xQueueSend(symbolQ, &ev, 0);
            printf("- (dash)\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500));  // sample every 0.5s
    }
}
