#pragma once

void sensorTask(void *pvParameters);

// Sensor state machine
typedef enum {
    SENSOR_STATE_IDLE = 0,
    SENSOR_STATE_IN_BURST
} sensor_state_t;
