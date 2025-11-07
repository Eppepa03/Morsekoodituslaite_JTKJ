#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/sdk.h"
#include "sensor_task.h"

void sensorTask(void *pvParameters)
{
    printf("[TASK] start\r\n");

    // IMU init
    int rc = init_ICM42670();
    printf("[TASK] init_ICM42670 rc=%d\r\n", rc);
    if (rc != 0) {
        while (1) { printf("[TASK] IMU init FAIL\r\n"); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    rc = ICM42670_start_with_default_values();
    printf("[TASK] start_with_default_values rc=%d\r\n", rc);
    if (rc != 0) {
        while (1) { printf("[TASK] IMU start FAIL\r\n"); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // Pose state
    enum { POSE_NONE = 0, POSE_DOT, POSE_DASH };

    static int last_pose   = POSE_NONE;  // last emitted pose
    static int stable_pose = POSE_NONE;  // pose we currently believe we're in
    static int stable_count = 0;         // how long we’ve been IN the pose
    static int exit_count   = 0;         // how long we’ve been OUT of the pose

    // Tunables 
    const int   STABLE_N = 2;      // ~300 ms (3×100 ms)
    const int   EXIT_N   = 3;      // need ~400 ms out-of-pose to reset
    const float NEAR0    = 0.25f;  // |axis| ~ 0 g
    const float NEAR1    = 0.25f;  // |axis - 1| ~ 0

    while (1)
    {
        float ax, ay, az, gx, gy, gz, t;
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t);

        // Pose classification
        bool near0_x     = (ax > -NEAR0 && ax < NEAR0);
        bool near0_y     = (ay > -NEAR0 && ay < NEAR0);
        bool near0_z     = (az > -NEAR0 && az < NEAR0);
        bool near1_y     = (ay > (1.0f - NEAR1) && ay < (1.0f + NEAR1));
        bool near_neg1_z = (az > (-1.0f - NEAR1) && az < (-1.0f + NEAR1));

        int current = POSE_NONE;
        // DOT = Y ≈ +1g, X ≈ 0, Z ≈ 0
        if (near1_y && near0_x && near0_z) {
            current = POSE_DOT;
        }
        // DASH = Z ≈ -1g, X ≈ 0, Y ≈ 0  (rotate 90° away from you)
        else if (near_neg1_z && near0_x && near0_y) {
            current = POSE_DASH;
        }

        // Stability filtering: stickiness in-pose; require EXIT_N misses to reset
        if (current == stable_pose) {
            if (stable_count < 1000) stable_count++;
            exit_count = 0;
        } else {
            exit_count++;
            if (exit_count >= EXIT_N) {
                stable_pose  = current;
                stable_count = (current != POSE_NONE) ? 1 : 0;
                exit_count   = 0;
            }
        }

        if (stable_pose == POSE_NONE) {
        last_pose = POSE_NONE;
        }

        // Emit exactly once on pose entry
        if (stable_pose != POSE_NONE && stable_count == STABLE_N && stable_pose != last_pose) {
            if (stable_pose == POSE_DOT)  printf(". (dot)\r\n");
            if (stable_pose == POSE_DASH) printf("- (dash)\r\n");
            last_pose = stable_pose;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz
    }
}