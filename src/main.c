
#include "pico/stdlib.h"
#include "tkjhat/sdk.h"
#include <stdio.h>

int main(void) {
    
    stdio_init_all();

    // 4) Anna USB:lle hetki aikaa enumerointiin ennen tulostusta
    sleep_ms(1500);

    printf("ICM-42670-P IMU demo starting...\n");

    // 5) Alustaa IMU:n (I2C-yhteys + perusrekisterit)
    if (init_ICM42670() != 0) {
        printf("ERROR: IMU init failed\n");
        while (1) { sleep_ms(1000); }
    }

    // 6) Käynnistää mittaukset SDK:n oletusasetuksilla (accel + gyro)
    if (ICM42670_start_with_default_values() != 0) {
        printf("ERROR: IMU start failed\n");
        while (1) { sleep_ms(1000); }
    }

    printf("IMU ready. Move the board to see values change.\n");

    // 7) Lukee dataa puolen sekunnin välein
    while (true) {
        float ax, ay, az;   // kiihtyvyys (g)
        float gx, gy, gz;   // gyro (deg/s)
        float temp_c;       // lämpötila (°C)

        // 8) SDK täyttää edellä annetut muuttujat
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp_c);

        // 9) Tulostaa arvot (odotus: ~az ≈ ±1.0 g laitteen ollessa paikallaan)
        printf("ACC[g]  x=% .3f  y=% .3f  z=% .3f | "
               "GYRO[dps] x=% .2f  y=% .2f  z=% .2f | "
               "T=%.2f C\n",
               ax, ay, az, gx, gy, gz, temp_c);

        // 10) Lukutiheys 
        sleep_ms(500);
    }
}