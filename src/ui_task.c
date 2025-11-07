#include "FreeRTOS.h"
#include "task.h"
#include "ssd1306.h"
#include "ui_menu.h"
#include "tkjhat/sdk.h"
#include <stdio.h>

static ssd1306_t disp;

static void on_usb(void) {
    printf("USB valittu\n");
    // USB-käynnistys
}

static void on_wireless(void) {
    printf("Wireless valittu\n");
    // Wireless-toiminnallisuus
}

static void on_shutdown(void) {
    printf("Shutdown valittu\n");
    // Käsitellään shutdown esim. tallennus ja resetti
}

void ui_task(void *params) {
    (void)params;

    // muistutus itelle varmista että TKJHAT ja I2C on alustettu aivan aiemmin
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);

    ui_menu_callbacks_t callbacks = {
        .on_connect_usb = on_usb,
        .on_connect_wireless = on_wireless,
        .on_shutdown = on_shutdown
    };

    ui_menu_init(&disp, &callbacks);
    ui_menu_force_redraw();

    while (1) {
        ui_menu_poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Tämä funktio käynnistää UI-taskin, kutsu tämä esim. jostain jo alustetusta tehtävästä
void start_ui_task() {
    xTaskCreate(ui_task, "UI_Task", 2048, NULL, 2, NULL);
}