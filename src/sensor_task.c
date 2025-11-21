/**
 * This task:
 *  - Continuously samples the gyro.
 *  - Detects "flick bursts" of motion.
 *  - Uses the *average Z-axis* angular velocity over a burst to decide
 *    whether the flick was a DOT or a DASH.
 *  - Sends DOT/DASH events into the morseQ FreeRTOS queue for the
 *    Morse decoder / state machine to process.
 */

#include <stdio.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include "sensor_task.h"
#include "queue.h"
#include "event.h"
#include "state_machine.h"

// External queues
extern QueueHandle_t morseQ;
extern volatile State_t currentState;

// Tunable thresholds
#define SAMPLE_PERIOD_MS     10      // Sampling rate 100 Hz
#define GYRO_START_THRESH    110.0f  // Required gyro magnitude to enter burst state
#define GYRO_END_THRESH      40.0f   // Gyro magnitude that ends a flick
#define MIN_BURST_SAMPLES    5       // Minimun number of samples for a valid flick
#define MAX_BURST_SAMPLES    60      // Maximun number of samples allowed in a flick
#define AXIS_MAG_THRESH      30.0f   // The average Z angular velocity over the burst must exceed this threshold to be counted as a flick

// Starts in idle state (waiting for a big enough motion to start a flick)
sensor_state_t sensorState = SENSOR_STATE_IDLE;


// Main task
void sensorTask(void *pvParameters)
{
    printf("[SENSOR] start\r\n");

    // Initializions for the IMU
    int rc = init_ICM42670();
    if (rc != 0) { while (1) { printf("[SENSOR] IMU init FAIL\r\n"); vTaskDelay(1000); } }

    rc = ICM42670_start_with_default_values();
    if (rc != 0) { while (1) { printf("[SENSOR] IMU start FAIL\r\n"); vTaskDelay(1000); } }

    // sum of gz values, samples and samples below the end threshold
    float sum_gz = 0.0f;
    int   burst_samples = 0;
    int   below_end_count = 0;

    while (1)
    {
        // Reads rensor values 
        float ax, ay, az, gx, gy, gz, t;
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);

        // Total gyro magnitude in degrees/s
        float gmag = sqrtf(gx*gx + gy*gy + gz*gz);

        switch (sensorState)
        {
        case SENSOR_STATE_IDLE:

            // Wait for a big enough motion to start a flick and enter burst state
            if (gmag > GYRO_START_THRESH) {
                sensorState = SENSOR_STATE_IN_BURST;
                sum_gz = gz;
                burst_samples = 1;
                below_end_count = 0;
            }
            break;


        case SENSOR_STATE_IN_BURST:

            // Collects z-axis movement while we are in a burst
            sum_gz += gz;
            burst_samples++;

             // Check if motion has dropped below end-threshold
            if (gmag < GYRO_END_THRESH)
                below_end_count++;
            else
                below_end_count = 0;

            // Conditions to end the burst:
            //  - motion has been low for a few samples, OR
            //  - the burst has grown too long. 
            if (below_end_count >= 3 || burst_samples >= MAX_BURST_SAMPLES)
            {
                // Computes average gz over the burst.
                float avg_gz = sum_gz / burst_samples;

                // Only treat as a valid flick if:
                //  - it lasted long enough (burst_samples)
                //  - the average Z motion was big enough
                if (burst_samples >= MIN_BURST_SAMPLES &&
                    fabsf(avg_gz) > AXIS_MAG_THRESH)
                {
                    // Decide if this was a dot or dash
                    // Sign convention:
                    //  - avg_gz > 0  → DASH
                    //  - avg_gz < 0  → DOT
                    symbol_ev_t ev = (avg_gz > 0.0f) ? DASH : DOT;
                    xQueueSend(morseQ, &ev, 0);
                }

                // Resets for the next flick and returns to idle state
                sensorState = SENSOR_STATE_IDLE;
                sum_gz = 0.0f;
                burst_samples = 0;
                below_end_count = 0;
            }

            break;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}