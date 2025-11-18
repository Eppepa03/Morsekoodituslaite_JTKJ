#include "FreeRTOS.h"
#include "task.h"
#include "tkjhat/ssd1306.h"
#include "tkjhat/ssd1306.h"
#include "ui_menu.h"
#include "tkjhat/sdk.h"
#include <stdio.h>

static ssd1306_t disp;

static void on_usb(void)      { printf("USB valittu\n"); }
static void on_wireless(void) { printf("Wireless valittu\n"); }
static void on_shutdown(void) { printf("Shutdown valittu\n"); }

void ui_task(void *params) {
    (void)params;

    bool ok = false;
    for (int i = 0; i < 3 && !ok; ++i) {
        ok = ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
        if (!ok) vTaskDelay(pdMS_TO_TICKS(50));
    }
    printf("SSD1306 init(retry): %d\n", ok);
    if (!ok) { printf("OLED init fail @0x3C I2C0\n"); vTaskDelete(NULL); }

    ssd1306_poweroff(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_poweron(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_contrast(&disp, 0xFF);

    // Splash
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "     TERVETULOA!");
    ssd1306_show(&disp);
    vTaskDelay(pdMS_TO_TICKS(1500)); // 1.5 s splash

    // UI käyntiin
    ui_menu_callbacks_t callbacks = {
        .on_connect_usb = on_usb,
        .on_connect_wireless = on_wireless,
        .on_shutdown = on_shutdown
    };
    ui_menu_init(&disp, &callbacks);

    ui_menu_poll();
    vTaskDelay(pdMS_TO_TICKS(50));

    while (1) {
        ui_menu_poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
