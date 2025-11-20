#include <stdio.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include "sensor_task.h"
#include "queue.h"
#include "event.h"

extern QueueHandle_t symbolQ;

// Tune these after testing
#define SAMPLE_PERIOD_MS     10      // 100 Hz
#define GYRO_START_THRESH    110.0f  // deg/s, start of flick
#define GYRO_END_THRESH      40.0f   // deg/s, end of flick
#define MIN_BURST_SAMPLES    5       // minimum ~50 ms
#define MAX_BURST_SAMPLES    60      // maximum ~600 ms
#define AXIS_MAG_THRESH      30.0f   // minimum avg axis magnitude to count as real flick

// Choose which axis decides left/right
#define USE_GX 0
#define USE_GY 0
#define USE_GZ 1   // using gz as you had before

void sensorTask(void *pvParameters)
{
    printf("[SENSOR] start\r\n");

    int rc = init_ICM42670();
    printf("[SENSOR] init_ICM42670 rc=%d\r\n", rc);
    if (rc != 0) {
        while (1) {
            printf("[SENSOR] IMU init FAIL\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    rc = ICM42670_start_with_default_values();
    printf("[SENSOR] start_with_default_values rc=%d\r\n", rc);
    if (rc != 0) {
        while (1) {
            printf("[SENSOR] IMU start FAIL\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    enum {
        STATE_IDLE = 0,
        STATE_IN_BURST
    };

    int state = STATE_IDLE;

    // Accumulators for one flick burst
    float sum_gx = 0.0f;
    float sum_gy = 0.0f;
    float sum_gz = 0.0f;
    int   burst_samples = 0;
    int   below_end_count = 0;

    while (1)
    {
        float ax, ay, az, gx, gy, gz, t;
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);

        // Total gyro magnitude
        float gmag = sqrtf(gx*gx + gy*gy + gz*gz);

        switch (state)
        {
        case STATE_IDLE:
            // Wait for big enough motion to start a flick
            if (gmag > GYRO_START_THRESH) {
                state = STATE_IN_BURST;
                sum_gx = gx;
                sum_gy = gy;
                sum_gz = gz;
                burst_samples = 1;
                below_end_count = 0;

                // Debug prints removed to avoid spam
                // printf("[SENSOR] burst start gmag=%.1f gx=%.1f gy=%.1f gz=%.1f\r\n",
                //        gmag, gx, gy, gz);
            }
            break;

        case STATE_IN_BURST:
            // Accumulate while we're in a burst
            sum_gx += gx;
            sum_gy += gy;
            sum_gz += gz;
            burst_samples++;

            if (gmag < GYRO_END_THRESH) {
                // Count how many consecutive "low motion" samples we see
                below_end_count++;
            } else {
                below_end_count = 0;
            }

            // Conditions to end the burst:
            // - motion has dropped below threshold for a few samples
            // - OR burst is too long
            if (below_end_count >= 3 || burst_samples >= MAX_BURST_SAMPLES) {

                // Compute average axis values
                float avg_gx = sum_gx / burst_samples;
                float avg_gy = sum_gy / burst_samples;
                float avg_gz = sum_gz / burst_samples;

                // Debug prints removed to avoid spam
                // printf("[SENSOR] burst end samples=%d avg_gx=%.1f avg_gy=%.1f avg_gz=%.1f\r\n",
                //        burst_samples, avg_gx, avg_gy, avg_gz);

                // Pick which axis we use for left/right decision
                float axis_avg = 0.0f;
#if USE_GX
                axis_avg = avg_gx;
#elif USE_GY
                axis_avg = avg_gy;
#else // USE_GZ by default
                axis_avg = avg_gz;
#endif

                // Only treat as valid flick if axis magnitude is big enough
                if (burst_samples >= MIN_BURST_SAMPLES &&
                    fabsf(axis_avg) > AXIS_MAG_THRESH)
                {
                    symbol_ev_t ev;

                    if (axis_avg > 0.0f) {
                        // DASH
                        ev = DASH;
                        printf("-");    // print actual dash
                    } else {
                        // DOT
                        ev = DOT;
                        printf(".");    // print actual dot
                    }

                    // Send symbol to queue (non-blocking)
                    xQueueSend(symbolQ, &ev, 0);
                }
                
                // else: silently ignore tiny or weird bursts

                // Go back to idle and reset accumulators
                state = STATE_IDLE;
                sum_gx = sum_gy = sum_gz = 0.0f;
                burst_samples = 0;
                below_end_count = 0;
            }

            break;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}