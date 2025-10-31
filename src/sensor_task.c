#include "sensor_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include <stdio.h>

void vSensorTask(void *pvParameters) {
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

        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp) == 0) {
            printf("ACC[g]: x=%.2f y=%.2f z=%.2f | "
                   "GYRO[dps]: x=%.2f y=%.2f z=%.2f | "
                   "TEMP: %.2f C\r\n",
                   ax, ay, az, gx, gy, gz, temp);
        } else {
            printf("IMU read failed\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // every 500 ms
    }
}

